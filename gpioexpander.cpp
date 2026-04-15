#include "gpioexpander.h"
#include <pigpio.h>
#include <bitset>
#include <QDebug>

GPIOExpander::GPIOExpander()
    : spiHandle(-1), HWAddress(0), isInitialized(false)
{
    GPIO_CONFIG.A = 0xFF;
    GPIO_CONFIG.B = 0xFF;
}

GPIOExpander::~GPIOExpander()
{
    if (spiHandle >= 0) {
        qDebug() << "[GPIO] Closing SPI handle for address" << (int)HWAddress;
        spiClose(spiHandle);
        spiHandle = -1;
    }
}

void GPIOExpander::setHardwareAddress(unsigned char address)
{
    HWAddress = address;
}

void GPIOExpander::initGPIOExpander(unsigned char address, int spiChannel)
{
    setHardwareAddress(address);
    
    qDebug() << "=== Initializing GPIO Expander ===";
    qDebug() << "Hardware Address:" << (int)address;
    qDebug() << "SPI Channel:" << spiChannel;
    
    // Open SPI on channel 1 (SPI0 is used for signal generator)
    // Speed: 100kHz, Mode: 0 (CPOL=0, CPHA=0)
    spiHandle = spiOpen(spiChannel, 100000, 0);
    
    if (spiHandle < 0) {
        qDebug() << "[ERROR] Failed to Open SPI:" << spiHandle;
        qDebug() << "        Make sure pigpiod daemon is running!";
        qDebug() << "        Run: sudo pigpiod";
        isInitialized = false;
        return;
    }
    
    qDebug() << "[OK] SPI Handle opened:" << spiHandle;

    char txBuffer[2];
    
    // === MCP23S17 Configuration Sequence ===
    
    // 1. Configure IOCON register
    txBuffer[0] = ICON_REG;
    txBuffer[1] = ICON_REG_DEF_VAL;
    sendCommand(txBuffer);
    qDebug() << "[CONFIG] IOCON register set";

    // 2. Configure IOCONA when bank = 0
    txBuffer[0] = ICON_REG_A;
    txBuffer[1] = ICON_REG_DEF_VAL;
    sendCommand(txBuffer);

    // 3. Configure IOCONB when bank = 0
    txBuffer[0] = ICON_REG_B;
    txBuffer[1] = ICON_REG_DEF_VAL;
    sendCommand(txBuffer);

    // 4. Set all GPIOAs to OUTPUT (0x00 = all outputs)
    txBuffer[0] = IODIRA_REG;
    txBuffer[1] = 0x00;
    sendCommand(txBuffer);
    qDebug() << "[CONFIG] GPIOA set to OUTPUT";

    // 5. Set all GPIOBs to OUTPUT
    txBuffer[0] = IODIRB_REG;
    txBuffer[1] = 0x00;
    sendCommand(txBuffer);
    qDebug() << "[CONFIG] GPIOB set to OUTPUT";

    // 6. Turn on all output latches for GPIOA
    txBuffer[0] = OLATA_REG;
    txBuffer[1] = 0xFF; // Turn ON (high = off for electrodes)
    sendCommand(txBuffer);

    // 7. Turn on all output latches for GPIOB
    txBuffer[0] = OLATB_REG;
    txBuffer[1] = 0xFF; // Turn ON
    sendCommand(txBuffer);

    // 8. Configure all GPIOAs to high (electrodes OFF)
    txBuffer[0] = GPIOA_REG;
    txBuffer[1] = 0xFF;
    sendCommand(txBuffer);
    qDebug() << "[CONFIG] GPIOA set to HIGH (electrodes OFF)";

    // 9. Configure all GPIOBs to high (electrodes OFF)
    txBuffer[0] = GPIOB_REG;
    txBuffer[1] = 0xFF;
    sendCommand(txBuffer);
    qDebug() << "[CONFIG] GPIOB set to HIGH (electrodes OFF)";

    GPIO_CONFIG.A = 0xFF;
    GPIO_CONFIG.B = 0xFF;
    
    isInitialized = true;
    qDebug() << "[OK] GPIO Expander" << (int)address << "initialized successfully!";
    qDebug() << "==============================\n";
}

void GPIOExpander::setGPIO_A(unsigned char GPIO_NUMBER)
{
    setGPIO(true, GPIO_NUMBER);
}

void GPIOExpander::unsetGPIO_A(unsigned char GPIO_NUMBER)
{
    unsetGPIO(true, GPIO_NUMBER);
}

void GPIOExpander::setGPIO_B(unsigned char GPIO_NUMBER)
{
    setGPIO(false, GPIO_NUMBER);
}

void GPIOExpander::unsetGPIO_B(unsigned char GPIO_NUMBER)
{
    unsetGPIO(false, GPIO_NUMBER);
}

void GPIOExpander::setGPIO(bool AB, unsigned char GPIO_NUMBER)
{
    set_unset_GPIO(AB, true, GPIO_NUMBER);
}

void GPIOExpander::unsetGPIO(bool AB, unsigned char GPIO_NUMBER)
{
    set_unset_GPIO(AB, false, GPIO_NUMBER);
}

void GPIOExpander::set_unset_GPIO(bool AB, bool SET_UNSET, unsigned char GPIO_NUMBER)
{
    if (!isInitialized) {
        qDebug() << "[ERROR] GPIO Expander not initialized!";
        return;
    }
    
    qDebug() << "[GPIO] set_unset_GPIO"
             << (AB ? "A" : "B")
             << (SET_UNSET ? "SET" : "UNSET")
             << "pin" << (int)GPIO_NUMBER;

    char CONFIG, GPIO_CONF, txBuffer[2];

    // Possible values for GPIO is 0-7 only
    if (GPIO_NUMBER >= 0 && GPIO_NUMBER <= 7) {
        CONFIG = 0x01 << GPIO_NUMBER;
        CONFIG = (SET_UNSET) ? (CONFIG ^ 0xFF) : (CONFIG);
        
        GPIO_CONF = (AB) ? GPIO_CONFIG.A : GPIO_CONFIG.B;
        GPIO_CONF = (SET_UNSET) ? (CONFIG & GPIO_CONF) : (CONFIG | GPIO_CONF);
        
        txBuffer[0] = (AB) ? GPIOA_REG : GPIOB_REG;
        txBuffer[1] = GPIO_CONF;

        qDebug() << "  Register:" << QString("0x%1").arg(txBuffer[0], 2, 16, QChar('0'))
                 << "Value:" << QString::number(txBuffer[1], 2).rightJustified(8, '0');

        sendCommand(txBuffer);

        if (AB) {
            GPIO_CONFIG.A = GPIO_CONF;
        } else {
            GPIO_CONFIG.B = GPIO_CONF;
        }
    } else {
        qDebug() << "[ERROR] GPIO Number" << (int)GPIO_NUMBER << "is out of range (0-7)";
        return;
    }
}

void GPIOExpander::setGPIO_A_OFF()
{
    char txBuffer[2];
    txBuffer[0] = GPIOA_REG;
    txBuffer[1] = 0xFF;
    sendCommand(txBuffer);
    GPIO_CONFIG.A = 0xFF;
    qDebug() << "[GPIO] All GPIOA set to OFF";
}

void GPIOExpander::setGPIO_B_OFF()
{
    char txBuffer[2];
    txBuffer[0] = GPIOB_REG;
    txBuffer[1] = 0xFF;
    sendCommand(txBuffer);
    GPIO_CONFIG.B = 0xFF;
    qDebug() << "[GPIO] All GPIOB set to OFF";
}

void GPIOExpander::reset_GPIOs()
{
    qDebug() << "[GPIO] Resetting all GPIOs to OFF";
    setGPIO_A_OFF();
    setGPIO_B_OFF();
}

void GPIOExpander::setGPIO(unsigned char GPIO_NUMBER, ELECTRODE_CONFIG CONFIG)
{
    switch (CONFIG) {
    case OFF:
        unsetGPIO_A(GPIO_NUMBER);
        unsetGPIO_B(GPIO_NUMBER);
        break;
    case ELECTRODE_A:
        unsetGPIO_B(GPIO_NUMBER);
        setGPIO_A(GPIO_NUMBER);
        break;
    case ELECTRODE_B:
        unsetGPIO_A(GPIO_NUMBER);
        setGPIO_B(GPIO_NUMBER);
        break;
    default:
        unsetGPIO_A(GPIO_NUMBER);
        break;
    }
}

int GPIOExpander::getSPIHandle()
{
    return spiHandle;
}

void GPIOExpander::sendCommand(char *txBuffer)
{
    if (!isInitialized || spiHandle < 0) {
        qDebug() << "[ERROR] Cannot send command - SPI not initialized";
        return;
    }

    // MCP23S17 OpCode: 0100 A2 A1 A0 R/W
    // Write = 0, so: 0100 A2 A1 A0 0
    char devOpCode = (0x40 | (0x0E & (HWAddress << 1)));
    char tBuffer[3] = {devOpCode, txBuffer[0], txBuffer[1]};

    qDebug() << "[SPI_TX] OpCode:" << QString::number(tBuffer[0], 2).rightJustified(8, '0')
             << "Reg:" << QString("0x%1").arg((unsigned char)tBuffer[1], 2, 16, QChar('0'))
             << "Val:" << QString::number((unsigned char)tBuffer[2], 2).rightJustified(8, '0')
             << "HWAddr:" << QString::number(HWAddress, 2).rightJustified(3, '0');

    int result = spiWrite(spiHandle, tBuffer, 3);
    
    if (result < 0) {
        qDebug() << "[ERROR] SPI write failed with code:" << result;
        return;
    }
    
    qDebug() << "[SPI_OK] Command sent successfully";
    
    // Small delay to allow MCP23S17 to process
    gpioDelay(100); // 100 microseconds
}

void GPIOExpander::setEConfig(char *txBuffer)
{
    sendCommand(txBuffer);
}

// Read register for loopback testing
unsigned char GPIOExpander::readRegister(unsigned char regAddress)
{
    if (!isInitialized || spiHandle < 0) {
        qDebug() << "[ERROR] Cannot read - SPI not initialized";
        return 0xFF;
    }

    // MCP23S17 OpCode for READ: 0100 A2 A1 A0 1
    char devOpCode = (0x40 | (0x0E & (HWAddress << 1)) | 0x01);
    char txBuf[3] = {devOpCode, regAddress, 0x00};
    char rxBuf[3] = {0, 0, 0};

    qDebug() << "[SPI_RX] Reading register:" << QString("0x%1").arg(regAddress, 2, 16, QChar('0'));

    // For MCP23S17, we need to send 3 bytes and read the 3rd byte
    int result = spiXfer(spiHandle, txBuf, rxBuf, 3);
    
    if (result < 0) {
        qDebug() << "[ERROR] SPI read failed with code:" << result;
        return 0xFF;
    }

    qDebug() << "[SPI_RX] Read value:" << QString::number((unsigned char)rxBuf[2], 2).rightJustified(8, '0');
    
    return (unsigned char)rxBuf[2];
}

// === Global Functions ===

void setElectrode(GPIOExpander *GPIO_EXPAND, ELECTRODE_MATRIX MAT_VAL, ELECTRODE_CONFIG CONFIG)
{
    qDebug() << "\n[ELECTRODE] Setting" << ELECTRODE_MATRIX_NAMES[MAT_VAL]
             << "to" << (CONFIG == ELECTRODE_A ? "A" : CONFIG == ELECTRODE_B ? "B" : "OFF");
    
    switch (MAT_VAL) {
    // Left Board 000
    case AB43:  GPIO_EXPAND[0].setGPIO(0, CONFIG); break;
    case AB65:  GPIO_EXPAND[0].setGPIO(1, CONFIG); break;
    case A7:    GPIO_EXPAND[0].setGPIO(2, CONFIG); break;
    case A8:    GPIO_EXPAND[0].setGPIO(3, CONFIG); break;
    case A9:    GPIO_EXPAND[0].setGPIO(4, CONFIG); break;
    case A10:   GPIO_EXPAND[0].setGPIO(5, CONFIG); break;
    case A11:   GPIO_EXPAND[0].setGPIO(6, CONFIG); break;
    case B11:   GPIO_EXPAND[0].setGPIO(7, CONFIG); break;

    // Center Board 010
    case B7:    GPIO_EXPAND[2].setGPIO(0, CONFIG); break;
    case B8:    GPIO_EXPAND[2].setGPIO(1, CONFIG); break;
    case B9:    GPIO_EXPAND[2].setGPIO(2, CONFIG); break;
    case B10:   GPIO_EXPAND[2].setGPIO(3, CONFIG); break;
    case C10:   GPIO_EXPAND[2].setGPIO(4, CONFIG); break;
    case C9:    GPIO_EXPAND[2].setGPIO(5, CONFIG); break;
    case C8:    GPIO_EXPAND[2].setGPIO(6, CONFIG); break;
    case C7:    GPIO_EXPAND[2].setGPIO(7, CONFIG); break;

    // Right Board 001
    case D9:    GPIO_EXPAND[1].setGPIO(0, CONFIG); break;
    case D10:   GPIO_EXPAND[1].setGPIO(1, CONFIG); break;
    case D11:   GPIO_EXPAND[1].setGPIO(2, CONFIG); break;
    case C11:   GPIO_EXPAND[1].setGPIO(3, CONFIG); break;
    case CD43:  GPIO_EXPAND[1].setGPIO(4, CONFIG); break;
    case CD65:  GPIO_EXPAND[1].setGPIO(5, CONFIG); break;
    case D7:    GPIO_EXPAND[1].setGPIO(6, CONFIG); break;
    case D8:    GPIO_EXPAND[1].setGPIO(7, CONFIG); break;

    default:
        qDebug() << "[ERROR] Unknown electrode matrix value";
        break;
    }
}

void setElectrodes(GPIOExpander *GPIO_EXPAND, ELECTRODE_CONFIG *CONFIG)
{
    qDebug() << "\n========== Setting All Electrodes ==========";
    
    std::bitset<8> CONFIG_GPIOA[3], CONFIG_GPIOB[3];
    
    // Initialize all to OFF (1 = OFF for active-low electrodes)
    for (int i = 0; i < 3; ++i) {
        CONFIG_GPIOA[i] = std::bitset<8>(0xFF);
        CONFIG_GPIOB[i] = std::bitset<8>(0xFF);
    }

    // Process all 24 electrodes
    int *electrodeSettings;
    for (int i = 0; i < 24; ++i) {
        electrodeSettings = ELECTRODE_MATRIX_CONFIG((ELECTRODE_MATRIX)i);
        int boardIdx = electrodeSettings[0];
        int pinIdx = electrodeSettings[1];

        if (boardIdx < 0 || boardIdx > 2 || pinIdx < 0 || pinIdx > 7) {
            qDebug() << "[ERROR] Invalid electrode config for index" << i;
            continue;
        }

        switch (CONFIG[i]) {
        case OFF:
            CONFIG_GPIOA[boardIdx][pinIdx] = 1; // High = OFF
            CONFIG_GPIOB[boardIdx][pinIdx] = 1;
            qDebug() << "  [" << i << "]" << ELECTRODE_MATRIX_NAMES[i] << "-> OFF";
            break;
            
        case ELECTRODE_A:
            CONFIG_GPIOA[boardIdx][pinIdx] = 0; // Low = ON
            CONFIG_GPIOB[boardIdx][pinIdx] = 1; // High = A polarity
            qDebug() << "  [" << i << "]" << ELECTRODE_MATRIX_NAMES[i] << "-> A";
            break;
            
        case ELECTRODE_B:
            CONFIG_GPIOA[boardIdx][pinIdx] = 0; // Low = ON
            CONFIG_GPIOB[boardIdx][pinIdx] = 0; // Low = B polarity
            qDebug() << "  [" << i << "]" << ELECTRODE_MATRIX_NAMES[i] << "-> B";
            break;
            
        default:
            CONFIG_GPIOA[boardIdx][pinIdx] = 1;
            CONFIG_GPIOB[boardIdx][pinIdx] = 1;
            break;
        }
    }

    // Send configurations to all 3 boards
    char regaddress[2];
    for (int i = 0; i < 3; ++i) {
        qDebug() << "\n[Board" << i << "] Sending configuration:";
        qDebug() << "  GPIOA:" << QString::fromStdString(CONFIG_GPIOA[i].to_string());
        qDebug() << "  GPIOB:" << QString::fromStdString(CONFIG_GPIOB[i].to_string());
        
        // Send GPIOA configuration
        regaddress[0] = GPIOA_REG;
        regaddress[1] = static_cast<char>(CONFIG_GPIOA[i].to_ulong());
        GPIO_EXPAND[i].setEConfig(regaddress);

        // Send GPIOB configuration
        regaddress[0] = GPIOB_REG;
        regaddress[1] = static_cast<char>(CONFIG_GPIOB[i].to_ulong());
        GPIO_EXPAND[i].setEConfig(regaddress);
    }
    
    qDebug() << "============================================\n";
}
