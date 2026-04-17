#include "spihandler.h"
#include <cstring> // for memcpy
#include <chrono>  // for timing diagnostics

#define SPI_CHANNEL 0        // SPI0 CE0 (use 1 for CE1)
#define SPI_BAUD 1000000     // 1 MHz
#define SPI_MODE 0           // CPOL=0, CPHA=0
#define SPI_FLAG (SPI_MODE)  // Just mode bits, no special flags

#define SIGNAL_PARAMS_CMD 0xD1  // Command to announce parameter block
#define AMPLITUDE_REQUEST_CMD 0xAA  // Command to request amplitude
#define SIGNAL_STATUS_CMD 0xAB      // Command to request signal running status
#define EMERGENCY_STOP_CMD 0xFF     // Emergency stop command
#define GPIO_INIT_CMD 0xC3          // GPIO expander initialization command
#define COMBINED_CONFIG_CMD 0xE3    // Combined electrode config + signal params command

// External global variables from sessionwindow.cpp
extern double g_latestAmplitude;

SpiHandler::SpiHandler(QObject *parent) : QObject(parent) {}

SpiHandler::~SpiHandler()
{
    if (spiHandle >= 0)
        spiClose(spiHandle);
}

bool SpiHandler::init() {
    static bool initialized = false;
    if (initialized) return true;
    initialized = true;

    if (gpioInitialise() < 0) {
        std::cerr << "pigpio init failed!" << std::endl;
        return false;
    }

    spiHandle = spiOpen(SPI_CHANNEL, SPI_BAUD, SPI_FLAG);
    if (spiHandle < 0) {
        std::cerr << "SPI open failed! Error code: " << spiHandle << std::endl;
        gpioTerminate();
        return false;
    }

    std::cout << "SPI initialized successfully on channel " << SPI_CHANNEL
              << " at " << SPI_BAUD << " Hz" << std::endl;
    return true;
}

void SpiHandler::initializeGPIOExpanders()
{
    // DEPRECATED: GPIO expanders now auto-initialize when Pico boots up
    // This function is kept for compatibility but does nothing
    std::cout << "[INFO] GPIO expander init skipped (auto-initialized at Pico startup)" << std::endl;
}

void SpiHandler::sendParameters(double amplitude, double carrierFreq, double burstFreq,
                                double rampUpRate, double coastDuration, double rampDownRate)
{
    if (spiHandle < 0) {
        std::cerr << "SPI not initialized!" << std::endl;
        return;
    }

    // Convert to floats and pack into buffer
    float amp = static_cast<float>(amplitude);
    float carrier = static_cast<float>(carrierFreq);
    float burst = static_cast<float>(burstFreq);
    float rampUp = static_cast<float>(rampUpRate);
    float coast = static_cast<float>(coastDuration);
    float rampDown = static_cast<float>(rampDownRate);

    char tx[BUFFER_SIZE];
    char rx[BUFFER_SIZE] = {0};

    // Debug: verify float values before packing
    std::cout << "DEBUG: amp=" << amp << ", carrier=" << carrier << ", burst=" << burst
              << ", rampUp=" << rampUp << ", coast=" << coast << ", rampDown=" << rampDown << std::endl;

    // Pack 6 floats into 24-byte buffer
    std::memcpy(tx + 0, &amp, sizeof(float));      // Bytes 0-3: amplitude
    std::memcpy(tx + 4, &carrier, sizeof(float));  // Bytes 4-7: carrier freq
    std::memcpy(tx + 8, &burst, sizeof(float));    // Bytes 8-11: burst freq
    std::memcpy(tx + 12, &rampUp, sizeof(float));  // Bytes 12-15: ramp-up rate
    std::memcpy(tx + 16, &coast, sizeof(float));   // Bytes 16-19: coast duration
    std::memcpy(tx + 20, &rampDown, sizeof(float));// Bytes 20-23: ramp-down rate

    // Debug: verify packing
    float test_amp, test_carrier, test_burst, test_rampUp, test_coast, test_rampDown;
    std::memcpy(&test_amp, tx + 0, sizeof(float));
    std::memcpy(&test_carrier, tx + 4, sizeof(float));
    std::memcpy(&test_burst, tx + 8, sizeof(float));
    std::memcpy(&test_rampUp, tx + 12, sizeof(float));
    std::memcpy(&test_coast, tx + 16, sizeof(float));
    std::memcpy(&test_rampDown, tx + 20, sizeof(float));
    std::cout << "DEBUG after memcpy: test_amp=" << test_amp
              << ", test_carrier=" << test_carrier
              << ", test_burst=" << test_burst
              << ", test_rampUp=" << test_rampUp
              << ", test_coast=" << test_coast
              << ", test_rampDown=" << test_rampDown << std::endl;

    std::cout << "Sending parameters:" << std::endl;
    std::cout << "  Amplitude: " << amp << " V" << std::endl;
    std::cout << "  Carrier Freq: " << carrier << " Hz" << std::endl;
    std::cout << "  Burst Freq: " << burst << " Hz" << std::endl;
    std::cout << "  Ramp Up Rate: " << rampUp << " V/s" << std::endl;
    std::cout << "  Coast Duration: " << coast << " s" << std::endl;
    std::cout << "  Ramp Down Rate: " << rampDown << " V/s" << std::endl;

    std::cout << "  TX bytes (raw): ";
    for (int i = 0; i < BUFFER_SIZE; i++) {
        std::cout << std::hex << std::setfill('0') << std::setw(2)
                  << (int)(unsigned char)tx[i] << " ";
    }
    std::cout << std::dec << std::endl;

    // Single-transfer-per-byte: send tx[i] and read echo of tx[i-1] simultaneously
    char dummy = 0x00;
    char rx_discard;
    char ack_byte = 0x00;

    // Send parameter-block command byte first so the Pico can sync
    char paramsCmd = SIGNAL_PARAMS_CMD;
    std::cout << "\n[PARAM TX] Command byte: 0x" << std::hex << std::setfill('0') << std::setw(2)
              << (int)(unsigned char)paramsCmd << std::dec << std::endl;
    spiXfer(spiHandle, &paramsCmd, &rx_discard, 1);
    std::cout << "  RX: 0x" << std::hex << std::setfill('0') << std::setw(2)
              << (int)(unsigned char)rx_discard << std::dec << " (discard)" << std::endl;
    gpioDelay(150);

    // Read back acknowledgement of 0xD1
    if (spiXfer(spiHandle, &dummy, &ack_byte, 1) < 0) {
        std::cerr << "Failed to read parameter command acknowledgement" << std::endl;
        return;
    }
    std::cout << "  ACK: 0x" << std::hex << std::setfill('0') << std::setw(2)
              << (int)(unsigned char)ack_byte << std::dec;
    if ((unsigned char)ack_byte != (unsigned char)paramsCmd) {
        std::cout << "  ❌ EXPECTED 0x" << std::hex << std::setfill('0') << std::setw(2)
                  << (int)(unsigned char)paramsCmd << std::dec << std::endl;
        return;
    } else {
        std::cout << "  ✓ OK" << std::endl;
    }
    gpioDelay(150);

    // First transfer: send tx[0], discard RX (no previous data)
    spiXfer(spiHandle, &tx[0], &rx_discard, 1);
    gpioDelay(150);

    // Transfers 1..N-1: send tx[i], receive rx[i-1] (echo of previous byte)
    for (int i = 1; i < BUFFER_SIZE; i++) {
        int res = spiXfer(spiHandle, &tx[i], &rx[i-1], 1);
        if (res < 0) {
            std::cerr << "SPI transfer failed at byte " << i << std::endl;
            return;
        }
        gpioDelay(150);
    }

    // Final transfer: send dummy, receive rx[11] (echo of last byte)
    spiXfer(spiHandle, &dummy, &rx[BUFFER_SIZE-1], 1);
    gpioDelay(150);

    std::cout << "  RX bytes (echo of previous): ";
    for (int i = 0; i < BUFFER_SIZE; i++) {
        std::cout << std::hex << std::setfill('0') << std::setw(2)
                  << (int)(unsigned char)rx[i] << " ";
    }
    std::cout << std::dec << std::endl;

    // Decode echoed values
    float echoedAmp = 0.0f;
    float echoedCarrier = 0.0f;
    float echoedBurst = 0.0f;
    float echoedRampUp = 0.0f;
    float echoedCoast = 0.0f;
    float echoedRampDown = 0.0f;
    std::memcpy(&echoedAmp, rx + 0, sizeof(float));
    std::memcpy(&echoedCarrier, rx + 4, sizeof(float));
    std::memcpy(&echoedBurst, rx + 8, sizeof(float));
    std::memcpy(&echoedRampUp, rx + 12, sizeof(float));
    std::memcpy(&echoedCoast, rx + 16, sizeof(float));
    std::memcpy(&echoedRampDown, rx + 20, sizeof(float));

    std::cout << std::fixed << std::setprecision(2)
              << "  Echo (previous values):" << std::endl
              << "    Amplitude: " << echoedAmp << " V" << std::endl
              << "    Carrier: " << echoedCarrier << " Hz" << std::endl
              << "    Burst: " << echoedBurst << " Hz" << std::endl
              << "    Ramp Up: " << echoedRampUp << " V/s" << std::endl
              << "    Coast: " << echoedCoast << " s" << std::endl
              << "    Ramp Down: " << echoedRampDown << " V/s" << std::endl;
    std::cout << std::endl;
}

void SpiHandler::sendEmergencyStop()
{
    if (spiHandle < 0) {
        std::cerr << "SPI not initialized!" << std::endl;
        return;
    }

    std::cout << "\n=== EMERGENCY STOP SENT ===" << std::endl;

    // Send emergency stop command ONCE
    char stopCmd = EMERGENCY_STOP_CMD;
    char rx_dummy;
    
    int res = spiXfer(spiHandle, &stopCmd, &rx_dummy, 1);
    if (res < 0) {
        std::cerr << "Emergency stop command failed to send!" << std::endl;
        return;
    }
    
    std::cout << "Emergency stop command transmitted successfully." << std::endl;
    std::cout << "  TX: 0x" << std::hex << std::setfill('0') << std::setw(2)
              << (int)(unsigned char)stopCmd << std::dec << std::endl;
    std::cout << "  RX: 0x" << std::hex << std::setfill('0') << std::setw(2)
              << (int)(unsigned char)rx_dummy << std::dec << std::endl;

    // CRITICAL: Give Pico sufficient time to:
    // 1. Detect emergency stop in main loop (up to 100µs worst case)
    // 2. Execute stop_signal_generator() (~50µs)
    // 3. Capture final amplitude and prepare 4-byte response
    // Total worst case: ~5ms for safety margin
    gpioDelay(5000);
    
    // NEW: Read final amplitude value (4 bytes) that Pico sends after stopping
    char amp_rx[4] = {0};
    char amp_tx[4] = {0};
    for (int i = 0; i < 4; i++) {
        spiXfer(spiHandle, &amp_tx[i], &amp_rx[i], 1);
        gpioDelay(50);  // 50µs spacing
    }
    
    // Decode final amplitude
    float finalAmplitude = 0.0f;
    std::memcpy(&finalAmplitude, amp_rx, sizeof(float));
    
    // Store globally for history window
    g_latestAmplitude = finalAmplitude;
    
    std::cout << "  Final output amplitude: " << std::fixed << std::setprecision(1) 
              << finalAmplitude << " V" << std::endl;
    std::cout << "  Final amplitude bytes: 0x";
    for (int i = 0; i < 4; i++) {
        std::cout << std::hex << std::setfill('0') << std::setw(2)
                  << (int)(unsigned char)amp_rx[i] << " ";
    }
    std::cout << std::dec << std::endl;
    
    // Aggressive FIFO flush: send 20 dummy bytes to completely drain any
    // leftover data that Pico may have pre-loaded during interrupted amplitude poll
    char dummy_tx = 0x00;
    char dummy_rx;
    for (int i = 0; i < 20; i++) {
        spiXfer(spiHandle, &dummy_tx, &dummy_rx, 1);
        gpioDelay(100);  // 100µs between each flush byte
    }
    
    // Final delay to ensure Pico processes the flush
    gpioDelay(1000);
    
    std::cout << "=== END EMERGENCY STOP ===\n" << std::endl;
}

float SpiHandler::requestCurrentAmplitude()
{
    if (spiHandle < 0) {
        std::cerr << "SPI not initialized!" << std::endl;
        return 0.0f;
    }

    // ===== TIMING DIAGNOSTIC MODE =====
    // Measure total request duration to understand polling limitations
    auto start_time = std::chrono::high_resolution_clock::now();

    // Simpler amplitude protocol (NO ACK):
    // 1. Send command byte 0xAA - Pico sees it and pre-loads 4 bytes into TX FIFO
    // 2. Wait for Pico to pre-load (critical delay)
    // 3. Send 4 dummy bytes - receive 4 amplitude bytes
    
    char tx_buf[5] = {AMPLITUDE_REQUEST_CMD, 0x00, 0x00, 0x00, 0x00};
    char rx_buf[5] = {0};
    
    auto cmd_send_start = std::chrono::high_resolution_clock::now();
    
    // Send command byte
    int res = spiXfer(spiHandle, &tx_buf[0], &rx_buf[0], 1);
    if (res < 0) {
        std::cerr << "[AMPLITUDE] SPI transfer failed at command byte" << std::endl;
        return 0.0f;
    }
    
    auto cmd_send_end = std::chrono::high_resolution_clock::now();
    
    // CRITICAL: Give Pico time to pre-load TX FIFO before we send dummies
    // Pico needs to: see 0xAA, copy amplitude, memcpy, write 4 bytes to FIFO
    auto delay_start = std::chrono::high_resolution_clock::now();
    gpioDelay(1000); // 1000µs (1ms) to ensure Pico completes pre-load
    auto delay_end = std::chrono::high_resolution_clock::now();
    
    // Send 4 dummy bytes to receive pre-loaded amplitude
    auto data_transfer_start = std::chrono::high_resolution_clock::now();
    for (int i = 1; i < 5; i++) {
        res = spiXfer(spiHandle, &tx_buf[i], &rx_buf[i], 1);
        if (res < 0) {
            std::cerr << "[AMPLITUDE] SPI transfer failed at byte " << i << std::endl;
            return 0.0f;
        }
        gpioDelay(50);
    }
    auto data_transfer_end = std::chrono::high_resolution_clock::now();
    
    // Give Pico time to flush RX FIFO and prime clean 0x00 for next command
    // Pico needs: 300µs delay + FIFO flush + prime 0x00 = ~350µs total
    // We wait 500µs to GUARANTEE Pico is completely ready (eliminates all byte rotation)
    auto final_delay_start = std::chrono::high_resolution_clock::now();
    gpioDelay(500);
    auto final_delay_end = std::chrono::high_resolution_clock::now();

    // DEBUG: Print ALL received bytes including rx_buf[0]
    std::cout << "\n[AMPLITUDE DEBUG] All RX bytes:" << std::endl;
    std::cout << "  rx_buf[0] (cmd echo): 0x" << std::hex << std::setfill('0') << std::setw(2) 
              << (int)(unsigned char)rx_buf[0] << std::dec << std::endl;
    for (int i = 1; i < 5; i++) {
        std::cout << "  rx_buf[" << i << "]: 0x" << std::hex << std::setfill('0') << std::setw(2)
                  << (int)(unsigned char)rx_buf[i] << std::dec << std::endl;
    }
    
    // The amplitude bytes are in rx_buf[1..4] (RX from the 4 dummy transfers)
    float currentAmplitude = 0.0f;
    std::memcpy(&currentAmplitude, rx_buf + 1, sizeof(float));

    auto end_time = std::chrono::high_resolution_clock::now();
    
    // Calculate timing breakdown
    auto total_duration = std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time).count();
    auto cmd_duration = std::chrono::duration_cast<std::chrono::microseconds>(cmd_send_end - cmd_send_start).count();
    auto delay_duration = std::chrono::duration_cast<std::chrono::microseconds>(delay_end - delay_start).count();
    auto data_duration = std::chrono::duration_cast<std::chrono::microseconds>(data_transfer_end - data_transfer_start).count();
    auto final_delay_duration = std::chrono::duration_cast<std::chrono::microseconds>(final_delay_end - final_delay_start).count();
    
    // Print timing diagnostics
    std::cout << "\n[AMPLITUDE TIMING DIAGNOSTIC]" << std::endl;
    std::cout << "  Total duration: " << total_duration << " µs (" 
              << std::fixed << std::setprecision(2) << (total_duration / 1000.0) << " ms)" << std::endl;
    std::cout << "  └─ Command send: " << cmd_duration << " µs" << std::endl;
    std::cout << "  └─ Pre-load wait: " << delay_duration << " µs (gpioDelay(1000))" << std::endl;
    std::cout << "  └─ Data transfer: " << data_duration << " µs (4 bytes × 50µs)" << std::endl;
    std::cout << "  └─ Final delay: " << final_delay_duration << " µs (gpioDelay(500))" << std::endl;
    std::cout << "  Decoded value: " << std::fixed << std::setprecision(3) 
              << currentAmplitude << " V" << std::endl;
    std::cout << "  RX bytes [1..4]: 0x";
    for (int i = 1; i < 5; i++) {
        std::cout << std::hex << std::setfill('0') << std::setw(2)
                  << (int)(unsigned char)rx_buf[i] << " ";
    }
    std::cout << std::dec << std::endl;
    std::cout << "  Max poll rate: " << std::fixed << std::setprecision(1) 
              << (1000000.0 / total_duration) << " Hz (" 
              << (total_duration / 1000.0) << " ms period)" << std::endl;
    std::cout << std::endl;

    return currentAmplitude;
}

bool SpiHandler::requestSignalRunning()
{
    if (spiHandle < 0) {
        std::cerr << "SPI not initialized!" << std::endl;
        return false;
    }

    // Read-only status request protocol:
    // 1) Send 0xAB command
    // 2) Wait briefly for Pico to preload one status byte
    // 3) Send one dummy byte and receive status (1=running, 0=finished)
    char tx_cmd = SIGNAL_STATUS_CMD;
    char rx_discard = 0x00;
    if (spiXfer(spiHandle, &tx_cmd, &rx_discard, 1) < 0) {
        std::cerr << "[STATUS] Failed to send status command" << std::endl;
        return false;
    }

    gpioDelay(300);

    char dummy = 0x00;
    char status_byte = 0x00;
    if (spiXfer(spiHandle, &dummy, &status_byte, 1) < 0) {
        std::cerr << "[STATUS] Failed to read status byte" << std::endl;
        return false;
    }

    gpioDelay(100);

    return (status_byte != 0);
}

void SpiHandler::sendElectrodeConfiguration(unsigned char* electrodeData, int dataSize)
{
    if (spiHandle < 0) {
        std::cerr << "SPI not initialized!" << std::endl;
        return;
    }

    std::cout << "\n=== Sending Electrode Configuration ===" << std::endl;
    std::cout << "Data size: " << dataSize << " bytes" << std::endl;

    // Define electrode configuration command byte (0xE2 for batch electrode update)
    #define ELECTRODE_CONFIG_CMD 0xE2

    // Send command byte first
    char cmdByte = ELECTRODE_CONFIG_CMD;
    char rx_discard;
    char ack_byte = 0x00;
    char dummy = 0x00;

    std::cout << "\n[ELECTRODE TX] Command byte: 0x" << std::hex << std::setfill('0') << std::setw(2)
              << (int)(unsigned char)cmdByte << std::dec << std::endl;
    std::cout << "  TX: 0x" << std::hex << std::setfill('0') << std::setw(2)
              << (int)(unsigned char)cmdByte << std::dec;

    spiXfer(spiHandle, &cmdByte, &rx_discard, 1);

    std::cout << " | RX: 0x" << std::hex << std::setfill('0') << std::setw(2)
              << (int)(unsigned char)rx_discard << std::dec << " (discard)" << std::endl;
    gpioDelay(200);  // Increased delay to let Pico prepare ACK
    // Expect Pico to echo command byte immediately
    if (spiXfer(spiHandle, &dummy, &ack_byte, 1) < 0) {
        std::cerr << "Failed to read electrode command acknowledgement" << std::endl;
        return;
    }
    std::cout << "[ELECTRODE ACK] RX: 0x" << std::hex << std::setfill('0') << std::setw(2)
              << (int)(unsigned char)ack_byte << std::dec;
    if ((unsigned char)ack_byte != (unsigned char)cmdByte) {
        std::cout << "  ❌ MISMATCH (expected 0x" << std::hex << std::setfill('0') << std::setw(2)
                  << (int)(unsigned char)cmdByte << std::dec << ")" << std::endl;
        return;
    } else {
        std::cout << "  ✓ OK" << std::endl;
    }
    gpioDelay(150);

    // Send electrode data using the same echo-style as sendParameters():
    // 1) send first data byte, discard RX (no previous data)
    // 2) for bytes 1..N-1 send data[i], receive echo of data[i-1]
    // 3) final dummy transfer to receive echo of last byte

    char rx_tmp = 0x00;

    // First data byte: send and discard RX (it's echo of previous transfer)
    if (dataSize > 0) {
        std::cout << "[ELECTRODE BYTE 0] TX: 0x" << std::hex << std::setfill('0') << std::setw(2)
                  << (int)(unsigned char)electrodeData[0] << std::dec;
        int res = spiXfer(spiHandle, (char*)&electrodeData[0], &rx_tmp, 1);
        if (res < 0) {
            std::cerr << " | SPI transfer FAILED at electrode byte 0" << std::endl;
            return;
        }
        std::cout << " | RX: 0x" << std::hex << std::setfill('0') << std::setw(2)
                  << (int)(unsigned char)rx_tmp << std::dec << " (discard)" << std::endl;
        gpioDelay(50);
    }

    // Middle bytes: send data[i], receive echo of data[i-1]
    for (int i = 1; i < dataSize; ++i) {
        char rx_echo = 0x00;
        std::cout << "[ELECTRODE BYTE " << i << "] TX: 0x" << std::hex << std::setfill('0') << std::setw(2)
                  << (int)(unsigned char)electrodeData[i] << std::dec;
        int res = spiXfer(spiHandle, (char*)&electrodeData[i], &rx_echo, 1);
        if (res < 0) {
            std::cerr << " | SPI transfer FAILED at electrode byte " << i << std::endl;
            return;
        }
        std::cout << " | RX: 0x" << std::hex << std::setfill('0') << std::setw(2)
                  << (int)(unsigned char)rx_echo << std::dec;
        // Verify echo matches previous byte
        if ((unsigned char)rx_echo != (unsigned char)electrodeData[i-1]) {
            std::cout << "  ❌ MISMATCH (expected 0x" << std::hex << std::setfill('0') << std::setw(2)
                      << (int)(unsigned char)electrodeData[i-1] << std::dec << ")" << std::endl;
        } else {
            std::cout << "  ✓ OK" << std::endl;
        }
        gpioDelay(50);
    }

    // Final dummy transfer: read echo of last data byte
    if (dataSize > 0) {
        char final_echo = 0x00;
        std::cout << "[ELECTRODE FINAL] TX: 0x" << std::hex << std::setfill('0') << std::setw(2)
                  << (int)(unsigned char)dummy << std::dec;
        int res = spiXfer(spiHandle, &dummy, &final_echo, 1);
        if (res < 0) {
            std::cerr << " | SPI transfer FAILED at final dummy" << std::endl;
            return;
        }
        std::cout << " | RX: 0x" << std::hex << std::setfill('0') << std::setw(2)
                  << (int)(unsigned char)final_echo << std::dec;
        if ((unsigned char)final_echo != (unsigned char)electrodeData[dataSize-1]) {
            std::cout << "  ❌ MISMATCH (expected 0x" << std::hex << std::setfill('0') << std::setw(2)
                      << (int)(unsigned char)electrodeData[dataSize-1] << std::dec << ")" << std::endl;
        } else {
            std::cout << "  ✓ OK (echo of last byte)" << std::endl;
        }
        gpioDelay(50);
    }

    std::cout << "\n✓ Electrode configuration sent (echo-style)" << std::endl;
    std::cout << "=== End Electrode Configuration ===\n" << std::endl;
}

void SpiHandler::sendCombinedConfiguration(unsigned char* electrodeData, int electrodeDataSize,
                                           double amplitude, double carrierFreq, double burstFreq,
                                           double rampUpRate, double coastDuration, double rampDownRate)
{
    if (spiHandle < 0) {
        std::cerr << "SPI not initialized!" << std::endl;
        return;
    }

    std::cout << "\n=== Sending Combined Configuration (Electrode + Signal Params) ===" << std::endl;
    std::cout << "Electrode data size: " << electrodeDataSize << " bytes" << std::endl;
    std::cout << "Signal params: Amp=" << amplitude << "V, Carrier=" << carrierFreq 
              << "Hz, Burst=" << burstFreq << "Hz, RampUp=" << rampUpRate
              << "V/s, Coast=" << coastDuration << "s, RampDown=" << rampDownRate << "V/s" << std::endl;

    // Total payload: 6 bytes electrode + 24 bytes signal params = 30 bytes
    const int COMBINED_BUFFER_SIZE = 30;
    char tx[COMBINED_BUFFER_SIZE];
    char rx[COMBINED_BUFFER_SIZE] = {0};
    std::memset(tx, 0, sizeof(tx));

    // Pack electrode data (6 bytes)
    for (int i = 0; i < electrodeDataSize && i < 6; i++) {
        tx[i] = electrodeData[i];
    }

    // Pack signal parameters (24 bytes: 6 floats)
    float amp = static_cast<float>(amplitude);
    float carrier = static_cast<float>(carrierFreq);
    float burst = static_cast<float>(burstFreq);
    float rampUp = static_cast<float>(rampUpRate);
    float coast = static_cast<float>(coastDuration);
    float rampDown = static_cast<float>(rampDownRate);
    std::memcpy(tx + 6, &amp, sizeof(float));      // Bytes 6-9: amplitude
    std::memcpy(tx + 10, &carrier, sizeof(float)); // Bytes 10-13: carrier freq
    std::memcpy(tx + 14, &burst, sizeof(float));   // Bytes 14-17: burst freq
    std::memcpy(tx + 18, &rampUp, sizeof(float));  // Bytes 18-21: ramp-up rate
    std::memcpy(tx + 22, &coast, sizeof(float));   // Bytes 22-25: coast duration
    std::memcpy(tx + 26, &rampDown, sizeof(float));// Bytes 26-29: ramp-down rate

    std::cout << "  TX bytes (raw): ";
    for (int i = 0; i < COMBINED_BUFFER_SIZE; i++) {
        std::cout << std::hex << std::setfill('0') << std::setw(2)
                  << (int)(unsigned char)tx[i] << " ";
    }
    std::cout << std::dec << std::endl;

    // Send command byte first - with retry on ACK failure
    char cmdByte = COMBINED_CONFIG_CMD;
    char rx_discard;
    char ack_byte = 0x00;
    char dummy = 0x00;
    int maxRetries = 3;
    bool ackSuccess = false;

    for (int retry = 0; retry < maxRetries && !ackSuccess; retry++) {
        if (retry > 0) {
            std::cout << "\n[COMBINED] Retry " << retry << " - flushing SPI and resending command..." << std::endl;
            // Flush by sending dummy bytes to clear any garbage
            for (int i = 0; i < 10; i++) {
                char flush_tx = 0x00;
                char flush_rx;
                spiXfer(spiHandle, &flush_tx, &flush_rx, 1);
                gpioDelay(50);
            }
            gpioDelay(500);  // Wait for Pico to stabilize
        }

        std::cout << "\n[COMBINED TX] Command byte: 0x" << std::hex << std::setfill('0') << std::setw(2)
                  << (int)(unsigned char)cmdByte << std::dec << std::endl;
        spiXfer(spiHandle, &cmdByte, &rx_discard, 1);
        std::cout << "  RX: 0x" << std::hex << std::setfill('0') << std::setw(2)
                  << (int)(unsigned char)rx_discard << std::dec << " (discard)" << std::endl;
        gpioDelay(200);

        // Read acknowledgement
        if (spiXfer(spiHandle, &dummy, &ack_byte, 1) < 0) {
            std::cerr << "Failed to read combined command acknowledgement" << std::endl;
            continue;
        }
        std::cout << "[COMBINED ACK] RX: 0x" << std::hex << std::setfill('0') << std::setw(2)
                  << (int)(unsigned char)ack_byte << std::dec;
        if ((unsigned char)ack_byte != (unsigned char)cmdByte) {
            std::cout << "  ❌ MISMATCH (expected 0x" << std::hex << std::setfill('0') << std::setw(2)
                      << (int)(unsigned char)cmdByte << std::dec << ")" << std::endl;
        } else {
            std::cout << "  ✓ OK" << std::endl;
            ackSuccess = true;
        }
    }

    if (!ackSuccess) {
        std::cerr << "[COMBINED] Failed to get ACK after " << maxRetries << " retries. Aborting." << std::endl;
        return;
    }

    gpioDelay(300);  // Delay after ACK to let Pico prepare for data reception

    // Send all 18 bytes using echo-style protocol
    char rx_tmp = 0x00;

    // First byte: send and discard RX
    if (COMBINED_BUFFER_SIZE > 0) {
        std::cout << "[COMBINED BYTE 0] TX: 0x" << std::hex << std::setfill('0') << std::setw(2)
                  << (int)(unsigned char)tx[0] << std::dec;
        int res = spiXfer(spiHandle, &tx[0], &rx_tmp, 1);
        if (res < 0) {
            std::cerr << " | SPI transfer FAILED at byte 0" << std::endl;
            return;
        }
        std::cout << " | RX: 0x" << std::hex << std::setfill('0') << std::setw(2)
                  << (int)(unsigned char)rx_tmp << std::dec << " (discard)" << std::endl;
        gpioDelay(100);  // Increased delay to ensure Pico primes echo
    }

    // Middle bytes: send tx[i], receive echo of tx[i-1]
    for (int i = 1; i < COMBINED_BUFFER_SIZE; ++i) {
        char rx_echo = 0x00;
        std::cout << "[COMBINED BYTE " << i << "] TX: 0x" << std::hex << std::setfill('0') << std::setw(2)
                  << (int)(unsigned char)tx[i] << std::dec;
        int res = spiXfer(spiHandle, &tx[i], &rx_echo, 1);
        if (res < 0) {
            std::cerr << " | SPI transfer FAILED at byte " << i << std::endl;
            return;
        }
        std::cout << " | RX: 0x" << std::hex << std::setfill('0') << std::setw(2)
                  << (int)(unsigned char)rx_echo << std::dec;
        // Verify echo matches previous byte
        if ((unsigned char)rx_echo != (unsigned char)tx[i-1]) {
            std::cout << "  ❌ MISMATCH (expected 0x" << std::hex << std::setfill('0') << std::setw(2)
                      << (int)(unsigned char)tx[i-1] << std::dec << ")" << std::endl;
        } else {
            std::cout << "  ✓ OK" << std::endl;
        }
        gpioDelay(100);  // Increased delay to ensure Pico primes echo
    }

    // Final dummy transfer: read echo of last byte
    if (COMBINED_BUFFER_SIZE > 0) {
        char final_echo = 0x00;
        std::cout << "[COMBINED FINAL] TX: 0x" << std::hex << std::setfill('0') << std::setw(2)
                  << (int)(unsigned char)dummy << std::dec;
        int res = spiXfer(spiHandle, &dummy, &final_echo, 1);
        if (res < 0) {
            std::cerr << " | SPI transfer FAILED at final dummy" << std::endl;
            return;
        }
        std::cout << " | RX: 0x" << std::hex << std::setfill('0') << std::setw(2)
                  << (int)(unsigned char)final_echo << std::dec;
        if ((unsigned char)final_echo != (unsigned char)tx[COMBINED_BUFFER_SIZE-1]) {
            std::cout << "  ❌ MISMATCH (expected 0x" << std::hex << std::setfill('0') << std::setw(2)
                      << (int)(unsigned char)tx[COMBINED_BUFFER_SIZE-1] << std::dec << ")" << std::endl;
        } else {
            std::cout << "  ✓ OK (echo of last byte)" << std::endl;
        }
        gpioDelay(100);  // Increased delay to ensure Pico primes echo
    }

    std::cout << "\n✓ Combined configuration sent successfully" << std::endl;
    std::cout << "  Electrode bytes [0-5]: ";
    for (int i = 0; i < 6; i++) {
        std::cout << std::hex << std::setfill('0') << std::setw(2)
                  << (int)(unsigned char)tx[i] << " ";
    }
    std::cout << std::dec << std::endl;
    std::cout << "  Signal params: Amp=" << amp << "V, Carrier=" << carrier 
              << "Hz, Burst=" << burst << "Hz, RampUp=" << rampUp
              << "V/s, Coast=" << coast << "s, RampDown=" << rampDown << "V/s" << std::endl;
    std::cout << "=== End Combined Configuration ===\n" << std::endl;
}
