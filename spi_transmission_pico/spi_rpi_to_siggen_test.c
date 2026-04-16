#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "pico/stdlib.h"
#include "hardware/spi.h"
#include "hardware/clocks.h"
#include "hardware/dma.h"
#include "hardware/pio.h"

// ====== SPI Configuration ======
#define SIGNAL_PARAM_FLOAT_COUNT 6 // amp, carrier, burst, ramp-up, coast, ramp-down
#define BUF_LEN (SIGNAL_PARAM_FLOAT_COUNT * 4)
#define ELECTRODE_BUF_LEN 6 // 6 bytes for electrode matrix configuration
#define COMBINED_BUF_LEN (ELECTRODE_BUF_LEN + BUF_LEN)
#define LED_PIN 25          // Onboard LED

// SPI0 Pins (Slave - receives from RPi)
#define SPI_RX_PIN  19      // Connected to RPi MOSI (GPIO 10)
#define SPI_CSN_PIN 17      // Connected to RPi CE0 (GPIO 8)
#define SPI_SCK_PIN 18      // Connected to RPi SCLK (GPIO 11)
#define SPI_TX_PIN  16      // Connected to RPi MISO (GPIO 9)

// SPI1 Pins (Master - transmits electrode config)
#define SPI1_RX_PIN  12     // GP12 - MISO (not used for electrode config)
#define SPI1_CSN_PIN 13     // GP13 - Chip Select
#define SPI1_SCK_PIN 14     // GP14 - Clock
#define SPI1_TX_PIN  15     // GP15 - MOSI (transmit electrode config)

// ====== Signal Generator Configuration ======
#define MAX_SAMPLES 16384   // should be a multiple of 4

// QUARTER-LUT for sine
#define QUARTER_LUT 256
#define QBITS 8
static uint8_t qsin_lut[QUARTER_LUT];

// Burst LUT (256 entries, 50% duty by default)
#define BURST_LUT 256
#define BURST_BITS 8
static uint8_t burst_lut[BURST_LUT];

// FES envelope LUT (precomputed)
#define FES_LUT 4096
#define FES_BITS 12
static uint16_t fes_lut[FES_LUT];   // values 0..256 (8.8 fixed-point scale; 256 == 1.0)

// phase accumulators and steps (DDS)
static uint32_t dds_phase_acc = 0;
static uint32_t dds_phase_acc_burst = 0;
static uint32_t dds_phase_acc_fes = 0;
static uint32_t dds_phase_step_carrier = 0;
static uint32_t dds_phase_step_burst = 0;
static uint32_t dds_phase_step_fes = 0;

const uint TEST_PIN = 10;

// buffers
uint8_t *bufferAddress[2];
uint8_t bufferQueue[2][MAX_SAMPLES];
int bufferIndex = 0;

// clocks
unsigned int duplicate = 0;
unsigned int clockDivider = 0;
unsigned int numOfSamples = 0;
double startPosition = 0;

// PIO
PIO pio = pio0;
uint sm;

// DMA channels
int ch0, ch1;
static volatile int dma_current_buf = 0;

// signal parameters
typedef struct signal {
    double phi;
    double F_osc;   // carrier frequency (Hz)
    double F_s;     // sample rate (Hz)
    double F_burst; // burst frequency (Hz)
    double fes_env; // not used per-sample, kept for completeness
    double t;
} SIGNAL;

SIGNAL signal;

// SIGNAL PARAMETERS - set by SPI
static volatile float fes_amp = 0.0f;
static volatile float carrier_freq = 0.0f;
static volatile float burst_freq = 0.0f;
static volatile float ramp_up_rate_v_per_sec = 1.0f;
static volatile float coast_duration_sec = 1.0f;
static volatile float ramp_down_rate_v_per_sec = 1.0f;
static volatile bool parameters_received = false;
static volatile bool signal_active = false;
static volatile bool emergency_stop_requested = false;
static volatile float current_output_amplitude = 0.0f;  // Current real-time amplitude (-5 to +5 V)

// Envelope timing derived from received signal parameters
static double phase_ramp_up_end = 0.0;
static double phase_coast_end = 0.0;
static double phase_ramp_down_end = 1.0;

// Electrode configuration storage
static uint8_t electrode_config[ELECTRODE_BUF_LEN] = {0};

#define EMERGENCY_STOP_CMD 0xFF  // Special command byte for emergency stop
#define AMPLITUDE_REQUEST_CMD 0xAA  // Command to request current amplitude
#define ELECTRODE_CONFIG_CMD 0xE2  // Electrode configuration command
#define SIGNAL_PARAMS_CMD 0xD1     // Signal parameter block command
#define COMBINED_CONFIG_CMD 0xE3    // Electrode + signal parameter block command
// GPIO_INIT_CMD removed - GPIO expanders now initialized automatically at startup

// ====== Function Prototypes ======
void init_spi1_master();
void send_electrode_config_via_spi1();
void init_gpio_expanders_via_spi1();
void getDMAChannel();
void init_DMA(int samples);
void DMA_IRQ_handler();
void init_PIO();
void init_signalData();
void sendWaveData();
void fillNextData();
void output_zero_level();
void stop_signal_generator();
double fesEnvelope(double fesIndex);
void apply_signal_parameters_from_rx(const uint8_t *signal_rx_buf, uint32_t sys_clock);

static inline void spi_slave_write_byte(uint8_t value)
{
    while (!spi_is_writable(spi0)) {
        tight_loop_contents();
    }
    spi_get_hw(spi0)->dr = value;
}

static inline uint8_t spi_slave_read_byte(void)
{
    while (!spi_is_readable(spi0)) {
        tight_loop_contents();
    }
    return spi_get_hw(spi0)->dr;
}

static inline void spi_slave_flush_rx_fifo(void)
{
    while (spi_is_readable(spi0)) {
        (void)spi_get_hw(spi0)->dr;
    }
}

static inline void spi_slave_flush_tx_fifo(void)
{
    // TX FIFO is write-only; we can't read it back. The hardware spec says it auto-clears
    // after all bytes are shifted out. For our purposes we simply ensure the next write
    // overwrites any stale content by writing immediately.
}

static inline void spi_slave_write_ack_blocking(uint8_t ack_byte)
{
    // Spin until TX FIFO has room, write the ACK, then spin until it's shifted out
    // so the Pi's acknowledgement read is guaranteed to see it.
    while (!spi_is_writable(spi0)) tight_loop_contents();
    spi_get_hw(spi0)->dr = ack_byte;
    
    // Wait for the master to clock in the ACK (they send a dummy byte)
    // We must consume that dummy so the next data-byte read stays synchronized
    while (!spi_is_readable(spi0)) tight_loop_contents();
    (void)spi_get_hw(spi0)->dr;  // discard the Pi's dummy byte
}

// ====== SPI1 Master Functions ======
void init_spi1_master(void)
{
    // Initialize SPI1 at 1 MHz as master
    spi_init(spi1, 1000000);
    spi_set_format(spi1, 8, SPI_CPOL_0, SPI_CPHA_0, SPI_MSB_FIRST);
    
    // Configure GPIO pins for SPI1
    gpio_set_function(SPI1_RX_PIN, GPIO_FUNC_SPI);
    gpio_set_function(SPI1_SCK_PIN, GPIO_FUNC_SPI);
    gpio_set_function(SPI1_TX_PIN, GPIO_FUNC_SPI);
    
    // Configure CSN as GPIO output (manual control)
    gpio_init(SPI1_CSN_PIN);
    gpio_set_dir(SPI1_CSN_PIN, GPIO_OUT);
    gpio_put(SPI1_CSN_PIN, 1);  // CS idle high (inactive)
}

void init_gpio_expanders_via_spi1(void)
{
    // MCP23S17 GPIO expander initialization sequence for all 3 boards
    // Hardware addresses: 0 (Left), 1 (Right), 2 (Center)
    
    const uint8_t hw_addresses[3] = {0, 1, 2};  // Left, Right, Center
    
    for (int board = 0; board < 3; board++) {
        uint8_t hw_addr = hw_addresses[board];
        uint8_t dev_opcode = 0x40 | ((hw_addr << 1) & 0x0E);  // MCP23S17 opcode: 0100 A2A1A0 W
        
        // Pull CS low for this expander
        gpio_put(SPI1_CSN_PIN, 0);
        sleep_us(10);
        
        // 1. Configure IOCON register
        uint8_t init_seq[] = {dev_opcode, 0x0A, 0x28};  // IOCON_REG, ICON_REG_DEF_VAL
        spi_write_blocking(spi1, init_seq, 3);
        gpio_put(SPI1_CSN_PIN, 1);
        sleep_us(100);
        
        // 2. Configure IOCONA
        gpio_put(SPI1_CSN_PIN, 0);
        sleep_us(10);
        init_seq[1] = 0x0A; init_seq[2] = 0x28;
        spi_write_blocking(spi1, init_seq, 3);
        gpio_put(SPI1_CSN_PIN, 1);
        sleep_us(100);
        
        // 3. Configure IOCONB
        gpio_put(SPI1_CSN_PIN, 0);
        sleep_us(10);
        init_seq[1] = 0x0B; init_seq[2] = 0x28;
        spi_write_blocking(spi1, init_seq, 3);
        gpio_put(SPI1_CSN_PIN, 1);
        sleep_us(100);
        
        // 4. Set GPIOA to OUTPUT (IODIRA = 0x00)
        gpio_put(SPI1_CSN_PIN, 0);
        sleep_us(10);
        init_seq[1] = 0x00; init_seq[2] = 0x00;  // IODIRA_REG
        spi_write_blocking(spi1, init_seq, 3);
        gpio_put(SPI1_CSN_PIN, 1);
        sleep_us(100);
        
        // 5. Set GPIOB to OUTPUT (IODIRB = 0x00)
        gpio_put(SPI1_CSN_PIN, 0);
        sleep_us(10);
        init_seq[1] = 0x01; init_seq[2] = 0x00;  // IODIRB_REG
        spi_write_blocking(spi1, init_seq, 3);
        gpio_put(SPI1_CSN_PIN, 1);
        sleep_us(100);
        
        // 6. Set OLATA to 0xFF (electrodes OFF)
        gpio_put(SPI1_CSN_PIN, 0);
        sleep_us(10);
        init_seq[1] = 0x14; init_seq[2] = 0xFF;  // OLATA_REG
        spi_write_blocking(spi1, init_seq, 3);
        gpio_put(SPI1_CSN_PIN, 1);
        sleep_us(100);
        
        // 7. Set OLATB to 0xFF (electrodes OFF)
        gpio_put(SPI1_CSN_PIN, 0);
        sleep_us(10);
        init_seq[1] = 0x15; init_seq[2] = 0xFF;  // OLATB_REG
        spi_write_blocking(spi1, init_seq, 3);
        gpio_put(SPI1_CSN_PIN, 1);
        sleep_us(100);
        
        // 8. Set GPIOA to 0xFF (electrodes OFF)
        gpio_put(SPI1_CSN_PIN, 0);
        sleep_us(10);
        init_seq[1] = 0x12; init_seq[2] = 0xFF;  // GPIOA_REG
        spi_write_blocking(spi1, init_seq, 3);
        gpio_put(SPI1_CSN_PIN, 1);
        sleep_us(100);
        
        // 9. Set GPIOB to 0xFF (electrodes OFF)
        gpio_put(SPI1_CSN_PIN, 0);
        sleep_us(10);
        init_seq[1] = 0x13; init_seq[2] = 0xFF;  // GPIOB_REG
        spi_write_blocking(spi1, init_seq, 3);
        gpio_put(SPI1_CSN_PIN, 1);
        sleep_us(100);
    }
}

void send_electrode_config_via_spi1(void)
{
    // Pull CS low to begin transmission
    gpio_put(SPI1_CSN_PIN, 0);
    sleep_us(10);  // Small delay for CS settling
    
    // Send all 6 bytes of electrode configuration
    spi_write_blocking(spi1, electrode_config, ELECTRODE_BUF_LEN);
    
    sleep_us(10);  // Small delay before releasing CS
    
    // Pull CS high to end transmission
    gpio_put(SPI1_CSN_PIN, 1);
}

static void receive_electrode_configuration_block(void)
{
    uint8_t electrode_rx_buf[ELECTRODE_BUF_LEN];

    for (int i = 0; i < ELECTRODE_BUF_LEN; i++) {
        uint8_t byte = spi_slave_read_byte();
        electrode_rx_buf[i] = byte;
        spi_slave_write_byte(byte);  // preload echo for next transfer
    }

    memcpy(electrode_config, electrode_rx_buf, ELECTRODE_BUF_LEN);

    // Prime a zero for the next command byte
    spi_slave_write_byte(0x00);

    // Forward electrode configuration via SPI1 to electrode matrix controller
    send_electrode_config_via_spi1();

    // Visual indicator
    gpio_put(LED_PIN, !gpio_get(LED_PIN));
}

static void receive_echo_payload_block(uint8_t *rx_buf, int payload_len)
{
    // Pi uses echo-style: first transfer discards RX, then i=1..N-1 capture echoes, final dummy gets last byte.
    // The ACK helper already consumed the Pi's acknowledgement dummy, so the next reads are payload bytes.
    // We prime each echo immediately after reading.

    // Read byte 0, prime its echo (Pi will discard this RX anyway)
    uint8_t byte0 = spi_slave_read_byte();
    rx_buf[0] = byte0;
    spi_slave_write_byte(byte0);

    // Read bytes 1..N-1, prime echoes for next transfer
    for (int i = 1; i < payload_len; ++i) {
        uint8_t byte = spi_slave_read_byte();
        rx_buf[i] = byte;
        spi_slave_write_byte(byte);
    }

    // Pi will send one final dummy to read the echo of the last payload byte; consume it.
    (void)spi_slave_read_byte();

    // Prime a zero so the next command sees a clean placeholder
    spi_slave_write_byte(0x00);
}

static void receive_signal_parameter_block(uint8_t *rx_buf)
{
    receive_echo_payload_block(rx_buf, BUF_LEN);
}

static void receive_combined_configuration_block(uint8_t *rx_buf)
{
    receive_echo_payload_block(rx_buf, COMBINED_BUF_LEN);
}

// ====== DMA Functions ======
void getDMAChannel()
{
    ch0 = dma_claim_unused_channel(true);
    ch1 = dma_claim_unused_channel(true);
}

void init_DMA(int samples)
{
    bufferAddress[0] = bufferQueue[0];
    bufferAddress[1] = bufferQueue[1];

    dma_channel_config c0 = dma_channel_get_default_config(ch0);
    dma_channel_config c1 = dma_channel_get_default_config(ch1);

    // configure channel 0: write to PIO TX FIFO
    channel_config_set_transfer_data_size(&c0, DMA_SIZE_32);
    channel_config_set_dreq(&c0, pio_get_dreq(pio, sm, true));
    channel_config_set_read_increment(&c0, true);
    channel_config_set_write_increment(&c0, false);
    channel_config_set_chain_to(&c0, ch1);
    channel_config_set_enable(&c0, true);

    dma_channel_configure(
        ch0,
        &c0,
        &pio0_hw->txf[0],
        bufferQueue[bufferIndex],
        samples,
        false
    );

    dma_channel_set_irq0_enabled(ch0, true);
    irq_set_exclusive_handler(DMA_IRQ_0, DMA_IRQ_handler);
    irq_set_enabled(DMA_IRQ_0, true);

    // configure channel 1: update channel 0 read address from bufferAddress array (ring)
    channel_config_set_transfer_data_size(&c1, DMA_SIZE_32);
    channel_config_set_read_increment(&c1, true);
    channel_config_set_write_increment(&c1, false);
    channel_config_set_chain_to(&c1, ch0);
    channel_config_set_ring(&c1, false, 3);
    channel_config_set_enable(&c1, true);

    dma_channel_configure(
        ch1,
        &c1,
        &dma_hw->ch[ch0].read_addr,
        bufferAddress,
        1,
        false
    );

    dma_current_buf = 0;
    dma_channel_start(ch0);
}

void DMA_IRQ_handler()
{
    gpio_put(TEST_PIN, 1);
    dma_hw->ints0 = 1u << ch0;

    int completed = dma_current_buf;
    bufferIndex = completed ^ 1;

    fillNextData();

    bufferAddress[bufferIndex] = bufferQueue[bufferIndex];
    dma_current_buf = bufferIndex;
    gpio_put(TEST_PIN, 0);
}

// ====== PIO Setup ======
static const uint16_t signal_output[] = {
    0x6008,             // OUT PINS, 8 bits
};

static const struct pio_program signal_output_pio_program = {
    .instructions = signal_output,
    .length = 1,
    .origin = -1,
};

void init_PIO()
{
    sm = pio_claim_unused_sm(pio, true);
    uint offset = pio_add_program(pio, &signal_output_pio_program);
    pio_sm_config c = pio_get_default_sm_config();

    sm_config_set_wrap(&c, offset, offset);
    sm_config_set_clkdiv(&c, 1);
    sm_config_set_out_shift(&c, true, true, 32);
    sm_config_set_out_pins(&c, 2, 8);

    for (int i = 0; i < 8; i++) {
        pio_gpio_init(pio, 2 + i);
        gpio_set_dir(2 + i, GPIO_OUT);
    }
    pio_sm_set_consecutive_pindirs(pio, sm, 2, 8, true);

    pio_sm_init(pio, sm, offset, &c);
    pio_sm_set_enabled(pio, sm, true);
}

// ====== Signal Data Initialization ======
void init_signalData()
{
    // Carrier Sine Wave
    for (unsigned int i = 0; i < QUARTER_LUT; i++)
    {
        double theta = (M_PI * 0.5) * ((double)i / (double)(QUARTER_LUT - 1));
        double s = sin(theta);
        int v = (int)lround(127.0 * s);
        if (v < 0) v = 0; if (v > 127) v = 127;
        qsin_lut[i] = (uint8_t)v;
    }

    // Burst Wave
    for (unsigned int i = 0; i < BURST_LUT; i++) {
        burst_lut[i] = (i < (BURST_LUT / 2)) ? 1 : 0;    
    }

    // FES envelope with configurable ramp-up/coast/ramp-down durations (no zero phase).
    const double max_dac_voltage = 5.0;
    double amp_v = fes_amp;
    if (amp_v < 0.0) amp_v = 0.0;
    if (amp_v > max_dac_voltage) amp_v = max_dac_voltage;
    double amplitude_scale = amp_v / max_dac_voltage;

    double ramp_up_rate = (ramp_up_rate_v_per_sec > 0.001f) ? ramp_up_rate_v_per_sec : 0.001;
    double ramp_down_rate = (ramp_down_rate_v_per_sec > 0.001f) ? ramp_down_rate_v_per_sec : 0.001;
    double coast_dur = (coast_duration_sec >= 0.0f) ? coast_duration_sec : 0.0;

    double ramp_up_duration = amp_v / ramp_up_rate;
    double ramp_down_duration = amp_v / ramp_down_rate;
    double total_cycle_duration = ramp_up_duration + coast_dur + ramp_down_duration;
    if (total_cycle_duration <= 1e-6) {
        // Fallback keeps a valid envelope if all inputs are near zero.
        ramp_up_duration = 0.5;
        coast_dur = 0.0;
        ramp_down_duration = 0.5;
        total_cycle_duration = 1.0;
    }

    phase_ramp_up_end = ramp_up_duration / total_cycle_duration;
    phase_coast_end = (ramp_up_duration + coast_dur) / total_cycle_duration;
    phase_ramp_down_end = 1.0;

    for (unsigned int i = 0; i < FES_LUT; i++) {
        double phase = (double)i / (double)FES_LUT;
        double env;

        if (phase < phase_ramp_up_end && phase_ramp_up_end > 1e-9) {
            double local_phase = phase / phase_ramp_up_end;
            env = local_phase * amplitude_scale;
        } else if (phase < phase_coast_end) {
            env = amplitude_scale;
        } else if (phase < phase_ramp_down_end && (phase_ramp_down_end - phase_coast_end) > 1e-9) {
            double local_phase = (phase - phase_coast_end) / (phase_ramp_down_end - phase_coast_end);
            env = amplitude_scale * (1.0 - local_phase);
        } else {
            env = 0.0;
        }
            
        int v = (int)lround(env * 256.0);
        if (v < 0) v = 0; if (v > 256) v = 256;
        fes_lut[i] = (uint16_t)v;
    }

    // phase steps (dds)
    signal.F_s = signal.F_osc * 100; // 100 samples per carrier cycle
    if (signal.F_s <= 0.0) signal.F_s = 1.0;

    dds_phase_step_carrier = (uint32_t)((signal.F_osc * 4294967296.0) / signal.F_s);
    dds_phase_step_burst   = (uint32_t)((signal.F_burst * 4294967296.0) / signal.F_s);

    double F_fes = 1.0 / total_cycle_duration;
    dds_phase_step_fes = (uint32_t)((F_fes * 4294967296.0) / signal.F_s);
}

void apply_signal_parameters_from_rx(const uint8_t *signal_rx_buf, uint32_t sys_clock)
{
    // Decode parameters (copy into locals first to preserve volatile qualifiers)
    float new_fes_amp;
    float new_carrier_freq;
    float new_burst_freq;
    float new_ramp_up_rate;
    float new_coast_duration;
    float new_ramp_down_rate;

    memcpy(&new_fes_amp, signal_rx_buf + 0, sizeof(float));
    memcpy(&new_carrier_freq, signal_rx_buf + 4, sizeof(float));
    memcpy(&new_burst_freq, signal_rx_buf + 8, sizeof(float));
    memcpy(&new_ramp_up_rate, signal_rx_buf + 12, sizeof(float));
    memcpy(&new_coast_duration, signal_rx_buf + 16, sizeof(float));
    memcpy(&new_ramp_down_rate, signal_rx_buf + 20, sizeof(float));

    fes_amp = new_fes_amp;
    carrier_freq = new_carrier_freq;
    burst_freq = new_burst_freq;
    ramp_up_rate_v_per_sec = (new_ramp_up_rate > 0.001f) ? new_ramp_up_rate : 0.001f;
    coast_duration_sec = (new_coast_duration >= 0.0f) ? new_coast_duration : 0.0f;
    ramp_down_rate_v_per_sec = (new_ramp_down_rate > 0.001f) ? new_ramp_down_rate : 0.001f;
    parameters_received = true;

    signal.F_burst = burst_freq;
    signal.F_osc = carrier_freq;
    init_signalData();

    signal.F_s = signal.F_osc * 100;
    if (signal.F_s <= 0.0) signal.F_s = 1.0;
    double division = (double)sys_clock / signal.F_s;
    unsigned int clkDiv = (division < 1.0) ? 1 : ((division > 65535.0) ? 65535 : (unsigned int)(division + 0.5));
    pio_sm_set_clkdiv_int_frac(pio, sm, clkDiv, 0);

    dds_phase_acc = 0;
    dds_phase_acc_burst = 0;
    dds_phase_acc_fes = 0;
}

// ====== Output Zero Level (127.5 = 0V) ======
void output_zero_level()
{
    // Fill both buffers with 127 or 128 (representing 0V)
    for (int buf = 0; buf < 2; buf++) {
        for (uint32_t i = 0; i < MAX_SAMPLES; i++) {
            bufferQueue[buf][i] = 128; // 0V level
        }
    }
}

// ====== Fill Buffer with Signal Data ======
void fillNextData(void)
{
    // If signal is not active, output zero level
    if (!signal_active) {
        for (uint32_t i = 0; i < numOfSamples; i++) {
            bufferQueue[bufferIndex][i] = 128; // 0V level
        }
        current_output_amplitude = 0.0f;
        return;
    }

    uint32_t acc_carrier = dds_phase_acc;
    uint32_t acc_burst = dds_phase_acc_burst;
    uint32_t acc_fes = dds_phase_acc_fes;

    const uint32_t QUAD_SHIFT = 30;
    const uint32_t IDX_SHIFT = 32 - (2 + QBITS);
    const uint32_t IDX_MASK = (1u << QBITS) - 1u;

    const uint32_t FES_SHIFT = 32 - FES_BITS;
    const uint32_t FES_MASK = (1u << FES_BITS) - 1u;

    int16_t max_modulated = 0;  // Track peak for amplitude calculation

    for (uint32_t i = 0; i < numOfSamples; i++)
    {
        // Burst gate from MSB of burst accumulator
        uint8_t A_burst = ( (acc_burst >> 31) == 0 ) ? 1u : 0u;

        // Carrier sample via quarter-wave LUT
        uint32_t car_quad = (acc_carrier >> QUAD_SHIFT) & 0x3u;
        uint32_t idx_raw = (acc_carrier >> IDX_SHIFT) & IDX_MASK;
        uint32_t idx = (car_quad & 1u) ? (uint32_t)(QUARTER_LUT - 1 - idx_raw) : idx_raw;
        uint8_t mag = qsin_lut[idx]; // 0..127
        int16_t carrier_bipolar = (car_quad < 2) ? (int16_t)mag : -(int16_t)mag;

        // FES envelope value from precomputed LUT (0..256)
        uint32_t fes_idx = (acc_fes >> FES_SHIFT) & FES_MASK;
        uint16_t fes_env_fp = fes_lut[fes_idx];

        // Fast modulation
        int16_t modulated;
        if (A_burst == 0 || fes_env_fp == 0) {
            modulated = 0;
        } else {
            modulated = (carrier_bipolar * (int)fes_env_fp) >> 8;
        }

        // Track peak amplitude for this buffer
        if (abs(modulated) > abs(max_modulated)) {
            max_modulated = modulated;
        }

        bufferQueue[bufferIndex][i] = (uint8_t)(modulated + 128);

        acc_carrier += dds_phase_step_carrier;
        acc_burst += dds_phase_step_burst;
        acc_fes += dds_phase_step_fes;
    }

    dds_phase_acc = acc_carrier;
    dds_phase_acc_burst = acc_burst;
    dds_phase_acc_fes = acc_fes;

    // Calculate current output amplitude in real voltage (-5V to +5V range)
    // Pico outputs 0-255 (0-3.3V), centered at 128 (1.65V)
    // Scale: (value - 128) / 127 = normalized -1 to +1
    // Then multiply by 5 to get -5V to +5V range
    current_output_amplitude = (abs(max_modulated) / 127.0f) * 5.0f;
    
    // Debug output every 100 buffers (reduce spam)
    static int buffer_count = 0;
    buffer_count++;
    // if (buffer_count % 100 == 0) {
    //     printf("Buffer #%d: max_modulated=%d, current_amp=%.3f V\n", 
    //            buffer_count, max_modulated, current_output_amplitude);
    // }
}

// ====== Stop Signal Generator ======
void stop_signal_generator()
{
    // printf("Stopping signal generator...\n");
    
    // Mark signal as inactive (fillNextData will now output 127.5)
    signal_active = false;
    
    // Reset phase accumulators for next run
    dds_phase_acc = 0;
    dds_phase_acc_burst = 0;
    dds_phase_acc_fes = 0;
    
    // Note: We keep DMA and PIO running, but fillNextData outputs zero level
    // printf("Signal generator now outputting 0V (127.5)\n");
}

double fesEnvelope(double fesIndex)
{
    fesIndex = fesIndex - floor(fesIndex);
    if (fesIndex < phase_ramp_up_end && phase_ramp_up_end > 1e-9) {
        return fesIndex / phase_ramp_up_end;
    }
    if (fesIndex < phase_coast_end) {
        return 1.0;
    }
    if ((phase_ramp_down_end - phase_coast_end) > 1e-9) {
        double local_phase = (fesIndex - phase_coast_end) / (phase_ramp_down_end - phase_coast_end);
        if (local_phase < 0.0) local_phase = 0.0;
        if (local_phase > 1.0) local_phase = 1.0;
        return 1.0 - local_phase;
    }
    return 0.0;
}

// ====== Configure PIO Clock and Start DMA ======
void sendWaveData()
{
    uint32_t sys_clock = clock_get_hz(clk_sys);
    double desired = signal.F_s;
    if (desired <= 1.0) desired = 1.0;

    double division = (double)sys_clock / desired;
    unsigned int clkDiv;
    if (division < 1.0) {
        clkDiv = 1;
    } else if (division > 65535.0) {
        clkDiv = 65535;
    } else {
        clkDiv = (unsigned int)(division + 0.5);
    }
    clockDivider = clkDiv;

    numOfSamples = MAX_SAMPLES;

    fillNextData();

    unsigned int clkdivInt = (clkDiv < 65535) ? clkDiv : 65535;
    unsigned int clkdivFrac = 0;
    pio_sm_set_clkdiv_int_frac(pio, sm, clkdivInt, clkdivFrac);

    init_DMA(numOfSamples / 4);

    double actual = (double)sys_clock / (double)clkDiv;
    // printf("Target F_s: %.0f, clkDiv: %u, actual: %.0f\n", signal.F_s, clkDiv, actual);
}

// ====== Main Function ======
int main()
{
    stdio_init_all();

    // Initialize PIO and DMA first (to output 0V from start)
    init_PIO();
    getDMAChannel();

    gpio_init(TEST_PIN);
    gpio_set_dir(TEST_PIN, GPIO_OUT);
    
    gpio_init(LED_PIN);
    gpio_set_dir(LED_PIN, GPIO_OUT);
    gpio_put(LED_PIN, 0);

    // Set default values for initialization
    signal.F_burst = 50.0;
    signal.F_osc   = 15000.0;
    fes_amp = 1.0f;
    ramp_up_rate_v_per_sec = 1.0f;
    coast_duration_sec = 1.0f;
    ramp_down_rate_v_per_sec = 1.0f;
    
    // Initialize signal data structures (but don't start signal yet)
    init_signalData();
    
    // Fill buffers with zero level (127.5 = 0V)
    output_zero_level();
    
    // Start DMA/PIO to continuously output 0V
    numOfSamples = MAX_SAMPLES;
    uint32_t sys_clock = clock_get_hz(clk_sys);
    signal.F_s = signal.F_osc * 100;
    double division = (double)sys_clock / signal.F_s;
    unsigned int clkDiv = (division < 1.0) ? 1 : ((division > 65535.0) ? 65535 : (unsigned int)(division + 0.5));
    pio_sm_set_clkdiv_int_frac(pio, sm, clkDiv, 0);
    init_DMA(numOfSamples / 4);

    // Initialize SPI0 as slave ONCE and keep it persistent
    spi_init(spi0, 1000000);
    spi_set_slave(spi0, true);
    spi_set_format(spi0, 8, SPI_CPOL_0, SPI_CPHA_0, SPI_MSB_FIRST);
    gpio_set_function(SPI_RX_PIN, GPIO_FUNC_SPI);
    gpio_set_function(SPI_SCK_PIN, GPIO_FUNC_SPI);
    gpio_set_function(SPI_TX_PIN, GPIO_FUNC_SPI);
    gpio_set_function(SPI_CSN_PIN, GPIO_FUNC_SPI);

    // Clear RX FIFO and prime TX with zero
    spi_slave_flush_rx_fifo();
    spi_slave_write_byte(0x00);

    // Initialize SPI1 as master for electrode configuration forwarding
    init_spi1_master();
    
    // Initialize GPIO expanders ONCE at startup (no longer needs RPi command)
    // printf("[PICO] Initializing GPIO expanders via SPI1...\n");
    init_gpio_expanders_via_spi1();
    // printf("[PICO] GPIO expanders ready\n");
    
    // CRITICAL: Complete SPI0 reset after GPIO init to clear any crosstalk
    // GPIO init on SPI1 can cause timing artifacts that contaminate SPI0 FIFOs
    sleep_ms(10);  // Let any transients settle
    
    // Method 1: Drain both RX and TX by reading until empty
    while (spi_is_readable(spi0)) {
        (void)spi_get_hw(spi0)->dr;  // Discard any garbage in RX FIFO
    }
    
    // Method 2: Reset SPI0 peripheral to completely clear state
    spi_deinit(spi0);
    sleep_us(100);
    spi_init(spi0, 1000000);
    spi_set_slave(spi0, true);
    spi_set_format(spi0, 8, SPI_CPOL_0, SPI_CPHA_0, SPI_MSB_FIRST);
    
    // GPIO pins already configured, just flush and prime
    while (spi_is_readable(spi0)) {
        (void)spi_get_hw(spi0)->dr;
    }
    spi_slave_write_byte(0x00);
    // printf("[PICO] SPI0 reset and ready for RPi commands\n");

    uint8_t signal_rx_buf[BUF_LEN];
    uint8_t combined_rx_buf[COMBINED_BUF_LEN];
    uint32_t signal_start_time = 0;
    bool signal_running = false;

    // Unified polling loop - handle all commands here
    while (true) {
        // Poll SPI for incoming commands
        while (spi_is_readable(spi0)) {
            uint8_t cmd = spi_get_hw(spi0)->dr;

            if (cmd == ELECTRODE_CONFIG_CMD) {
                // CRITICAL: Delay must be LESS than RPi's ACK read delay (200\u00b5s on RPi side)
                // This ensures ACK is primed before RPi tries to read it
                sleep_us(100);
                
                // ACK and receive electrode block
                spi_slave_write_ack_blocking(ELECTRODE_CONFIG_CMD);
                receive_electrode_configuration_block();
                gpio_put(LED_PIN, !gpio_get(LED_PIN));
            }
            else if (cmd == SIGNAL_PARAMS_CMD) {
                // CRITICAL: Delay must be LESS than RPi's ACK read delay (150\u00b5s on RPi side)
                // This ensures ACK is primed before RPi tries to read it
                sleep_us(100);
                
                // ACK and receive parameter block
                spi_slave_write_ack_blocking(SIGNAL_PARAMS_CMD);
                receive_signal_parameter_block(signal_rx_buf);
                apply_signal_parameters_from_rx(signal_rx_buf, sys_clock);
                
                // Start signal
                signal_active = true;
                signal_running = true;
                signal_start_time = to_ms_since_boot(get_absolute_time());
                gpio_put(LED_PIN, 1);
                
                // CRITICAL: Complete FIFO reset after parameter reception
                // The 12-byte echo sequence leaves stale bytes in both RX and TX FIFOs
                // Method: Reset SPI0 peripheral completely, then reconfigure
                sleep_ms(1);  // Let RPi finish reading final parameter echo byte
                
                spi_deinit(spi0);
                sleep_us(100);
                spi_init(spi0, 1000000);
                spi_set_slave(spi0, true);
                spi_set_format(spi0, 8, SPI_CPOL_0, SPI_CPHA_0, SPI_MSB_FIRST);
                
                // GPIO pins already configured, just flush and prime
                while (spi_is_readable(spi0)) {
                    (void)spi_get_hw(spi0)->dr;
                }
                spi_slave_write_byte(0x00);
            }
            else if (cmd == COMBINED_CONFIG_CMD) {
                sleep_us(100);

                // ACK and receive 6-byte electrode + 24-byte signal parameter block
                spi_slave_write_ack_blocking(COMBINED_CONFIG_CMD);
                receive_combined_configuration_block(combined_rx_buf);

                // First 6 bytes are electrode configuration
                memcpy(electrode_config, combined_rx_buf, ELECTRODE_BUF_LEN);
                send_electrode_config_via_spi1();

                // Remaining 24 bytes are signal parameters
                apply_signal_parameters_from_rx(combined_rx_buf + ELECTRODE_BUF_LEN, sys_clock);

                signal_active = true;
                signal_running = true;
                signal_start_time = to_ms_since_boot(get_absolute_time());
                gpio_put(LED_PIN, 1);

                // Complete FIFO reset after combined reception to avoid stale bytes.
                sleep_ms(1);

                spi_deinit(spi0);
                sleep_us(100);
                spi_init(spi0, 1000000);
                spi_set_slave(spi0, true);
                spi_set_format(spi0, 8, SPI_CPOL_0, SPI_CPHA_0, SPI_MSB_FIRST);

                while (spi_is_readable(spi0)) {
                    (void)spi_get_hw(spi0)->dr;
                }
                spi_slave_write_byte(0x00);
            }
            else if (cmd == AMPLITUDE_REQUEST_CMD) {
                // SIMPLE NO-ACK AMPLITUDE PROTOCOL:
                // Main loop already consumed 0xAA, just need to send 4-byte amplitude response
                
                // CRITICAL: The issue is that TX FIFO already has the primed 0x00 from previous command!
                // When RPi sends 4 dummy bytes, it receives: [0x00, byte0, byte1, byte2]
                // Missing byte3, and getting the old primed 0x00 as first byte
                //
                // Solution: DON'T send amplitude bytes directly. Instead, prime them one-by-one
                // and let the RPi's dummy transfers clock them out naturally.
                
                // Wait a moment for RPi to finish sending 0xAA command byte
                sleep_us(50);
                
                // Get current amplitude value
                float amp_copy = current_output_amplitude;
                uint8_t tx_buf[4];
                memcpy(tx_buf, &amp_copy, sizeof(float));
                
                // Prime all 4 bytes into TX FIFO sequentially
                // The RPi will send 4 dummy bytes and receive these 4 amplitude bytes
                for (int i = 0; i < 4; i++) {
                    while (!spi_is_writable(spi0)) tight_loop_contents();
                    spi_get_hw(spi0)->dr = tx_buf[i];
                }
                
                // Wait for RPi to complete reading all 4 bytes
                // RPi waits 1ms then does 4 transfers with 50µs spacing = ~1.2ms total
                sleep_us(1500);
                
                // NOW drain any received dummy bytes from RX FIFO
                while (spi_is_readable(spi0)) {
                    (void)spi_get_hw(spi0)->dr;
                }
                
                // Prime clean 0x00 for next command
                while (!spi_is_writable(spi0)) tight_loop_contents();
                spi_get_hw(spi0)->dr = 0x00;
            }
            else if (cmd == EMERGENCY_STOP_CMD) {
                // CRITICAL: Capture final amplitude IMMEDIATELY before any processing
                // current_output_amplitude is only updated in DMA interrupt, so grab it NOW
                float final_amplitude = current_output_amplitude;
                
                // Stop signal generation
                stop_signal_generator();
                signal_running = false;
                gpio_put(LED_PIN, 0);
                
                // Small delay to let Pi finish sending 0xFF command
                sleep_us(100);
                
                // Flush any leftover RX bytes from command phase
                while (spi_is_readable(spi0)) {
                    (void)spi_get_hw(spi0)->dr;
                }
                
                // CRITICAL: Wait for RPi's 5ms delay to elapse BEFORE priming bytes
                // This ensures RPi won't start reading before we're ready
                // Subtract the 100µs we already waited
                sleep_us(4400);
                
                // Send final amplitude value to RPi (4 bytes)
                // RPi will read this after emergency stop to display final output voltage
                uint8_t amp_buf[4];
                memcpy(amp_buf, &final_amplitude, sizeof(float));
                
                // Prime all 4 amplitude bytes into TX FIFO quickly
                for (int i = 0; i < 4; i++) {
                    while (!spi_is_writable(spi0)) tight_loop_contents();
                    spi_get_hw(spi0)->dr = amp_buf[i];
                }
                
                // Wait for RPi to read the 4 amplitude bytes
                // RPi does 4 transfers with 50µs spacing = ~300µs total
                sleep_us(500);
                
                // NOW drain RPi's 4 dummy TX bytes from the amplitude read
                while (spi_is_readable(spi0)) {
                    (void)spi_get_hw(spi0)->dr;
                }
                
                // RPi will now send 20 dummy flush bytes
                // Wait for those to arrive: 20 bytes × 100µs spacing = ~2ms
                sleep_us(2500);
                
                // Flush the 20 dummy bytes
                while (spi_is_readable(spi0)) {
                    (void)spi_get_hw(spi0)->dr;
                }
                
                // Prime clean 0x00 for next command
                while (!spi_is_writable(spi0)) tight_loop_contents();
                spi_get_hw(spi0)->dr = 0x00;
            }
        }

        // Auto-stop signal after 20 seconds
        if (signal_running) {
            uint32_t elapsed = to_ms_since_boot(get_absolute_time()) - signal_start_time;
            if (elapsed >= 20000) {
                stop_signal_generator();
                signal_running = false;
                gpio_put(LED_PIN, 0);
            }
        }

        sleep_us(100);
    }

    return 0;
}