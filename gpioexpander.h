#ifndef GPIOEXPANDER_H
#define GPIOEXPANDER_H

#include <QDebug>

// MCP23S17 Register Definitions
#define ICON_REG 0x05
#define ICON_REG_DEF_VAL 0x28

// ICON Registers (when Bank set to 0)
#define ICON_REG_A 0x0A
#define ICON_REG_B 0x0B

// Registers for A (when Bank set to 0)
#define IODIRA_REG 0x00 // I/O Direction
#define IPOLA_REG 0x02
#define GPIOA_REG 0x12
#define OLATA_REG 0x14

// Registers for B (when Bank set to 0)
#define IODIRB_REG 0x01
#define IPOLB_REG 0x03
#define GPIOB_REG 0x13
#define OLATB_REG 0x15

// Hardware Addresses of Boards (MCP23S17 A2 A1 A0 pins)
#define HWA_LEFT   0x00  // Left Board   - 000
#define HWA_RIGHT  0x01  // Right Board  - 001
#define HWA_CENTER 0x02  // Center Board - 010

// Electrode Matrices (24 electrodes matching your GUI layout)
enum ELECTRODE_MATRIX
{
    A11,    // 0
    B11,    // 1
    C11,    // 2
    D11,    // 3
    A10,    // 4
    B10,    // 5
    C10,    // 6
    D10,    // 7
    A9,     // 8
    B9,     // 9
    C9,     // 10
    D9,     // 11
    A8,     // 12
    B8,     // 13
    C8,     // 14
    D8,     // 15
    A7,     // 16
    B7,     // 17
    C7,     // 18
    D7,     // 19
    AB65,   // 20
    CD65,   // 21
    AB43,   // 22
    CD43    // 23
};

// Electrode Configurations
enum ELECTRODE_CONFIG
{
    OFF,
    ELECTRODE_A,
    ELECTRODE_B,
    NO_CHANGE
};

// Electrode matrix configuration mapping
static int *ELECTRODE_MATRIX_CONFIG(ELECTRODE_MATRIX MAT_VAL)
{
    static int config[2]; // [board_index, gpio_pin]

    switch (MAT_VAL)
    {
    // Left Board 000
    case AB43:
        config[0] = 0; config[1] = 0; break;
    case AB65:
        config[0] = 0; config[1] = 1; break;
    case A7:
        config[0] = 0; config[1] = 2; break;
    case A8:
        config[0] = 0; config[1] = 3; break;
    case A9:
        config[0] = 0; config[1] = 4; break;
    case A10:
        config[0] = 0; config[1] = 5; break;
    case A11:
        config[0] = 0; config[1] = 6; break;
    case B11:
        config[0] = 0; config[1] = 7; break;

    // Center Board 010
    case B7:
        config[0] = 2; config[1] = 0; break;
    case B8:
        config[0] = 2; config[1] = 1; break;
    case B9:
        config[0] = 2; config[1] = 2; break;
    case B10:
        config[0] = 2; config[1] = 3; break;
    case C10:
        config[0] = 2; config[1] = 4; break;
    case C9:
        config[0] = 2; config[1] = 5; break;
    case C8:
        config[0] = 2; config[1] = 6; break;
    case C7:
        config[0] = 2; config[1] = 7; break;

    // Right Board 001
    case D9:
        config[0] = 1; config[1] = 0; break;
    case D10:
        config[0] = 1; config[1] = 1; break;
    case D11:
        config[0] = 1; config[1] = 2; break;
    case C11:
        config[0] = 1; config[1] = 3; break;
    case CD43:
        config[0] = 1; config[1] = 4; break;
    case CD65:
        config[0] = 1; config[1] = 5; break;
    case D7:
        config[0] = 1; config[1] = 6; break;
    case D8:
        config[0] = 1; config[1] = 7; break;

    default:
        config[0] = -1; config[1] = -1; break;
    }

    return config;
}

static const char *ELECTRODE_MATRIX_NAMES[] = {
    "A11", "B11", "C11", "D11",
    "A10", "B10", "C10", "D10",
    "A9",  "B9",  "C9",  "D9",
    "A8",  "B8",  "C8",  "D8",
    "A7",  "B7",  "C7",  "D7",
    "AB65", "CD65", "AB43", "CD43"
};

class GPIOExpander
{
private:
    int spiHandle;
    unsigned char HWAddress;
    bool isInitialized;
    
    struct GPCONFIG
    {
        unsigned char A, B;
    } GPIO_CONFIG;

    void setHardwareAddress(unsigned char address);
    void setGPIO(bool AB, unsigned char GPIO_NUMBER);
    void unsetGPIO(bool AB, unsigned char GPIO_NUMBER);
    void set_unset_GPIO(bool AB, bool SET_UNSET, unsigned char GPIO_NUMBER);

    // Individual commands that control 1 GPIO
    void setGPIO_A(unsigned char GPIO_NUMBER);
    void setGPIO_B(unsigned char GPIO_NUMBER);
    void unsetGPIO_A(unsigned char GPIO_NUMBER);
    void unsetGPIO_B(unsigned char GPIO_NUMBER);
    void sendCommand(char *txBuffer);

public:
    GPIOExpander();
    ~GPIOExpander();
    
    void initGPIOExpander(unsigned char address, int spiChannel = 1);

    // Commands that affect all GPIOs
    void setGPIO_A_OFF();
    void setGPIO_B_OFF();
    void reset_GPIOs();

    // Set GPIO
    void setGPIO(unsigned char GPIO_NUMBER, ELECTRODE_CONFIG CONFIG);
    void setEConfig(char *txBuffer);

    int getSPIHandle();
    bool getInitStatus() const { return isInitialized; }
    
    // For loopback testing
    unsigned char readRegister(unsigned char regAddress);
};

// Other function prototypes
void setElectrode(
    GPIOExpander *GPIO_EXPAND,
    ELECTRODE_MATRIX MAT_VAL,
    ELECTRODE_CONFIG CONFIG);

void setElectrodes(
    GPIOExpander *GPIO_EXPAND,
    ELECTRODE_CONFIG *CONFIG);

#endif // GPIOEXPANDER_H
