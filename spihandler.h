#pragma once
#include <QObject>
#include <pigpio.h>
#include <iostream>
#include <iomanip>

class SpiHandler : public QObject
{
    Q_OBJECT

public:
    static SpiHandler* instance()
    {
        static SpiHandler instance;  // created only once
        return &instance;
    }

    bool init();
    void initializeGPIOExpanders();  // NEW: Initialize GPIO expanders via Pico
    void sendParameters(double amplitude, double carrierFreq, double burstFreq);
    void sendEmergencyStop();
    float requestCurrentAmplitude();  // NEW: Request current amplitude from Pico
    void sendElectrodeConfiguration(unsigned char* electrodeData, int dataSize);  // DEPRECATED: Use sendCombinedConfiguration instead
    void sendCombinedConfiguration(unsigned char* electrodeData, int electrodeDataSize, 
                                   double amplitude, double carrierFreq, double burstFreq);  // NEW: Send electrode + signal params together

private:
    explicit SpiHandler(QObject *parent = nullptr);
    ~SpiHandler();

    SpiHandler(const SpiHandler&) = delete;
    SpiHandler& operator=(const SpiHandler&) = delete;

    int spiHandle = -1;
    const int BUFFER_SIZE = 12; // 3 floats × 4 bytes = 12 bytes
};
