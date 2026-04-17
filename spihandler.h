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
    void sendParameters(double amplitude, double carrierFreq, double burstFreq,
                        double rampUpRate, double coastDuration, double rampDownRate);
    void sendEmergencyStop();
    float requestCurrentAmplitude();  // NEW: Request current amplitude from Pico
    bool requestSignalRunning();      // NEW: Query whether one-shot signal is still active
    void sendElectrodeConfiguration(unsigned char* electrodeData, int dataSize);  // DEPRECATED: Use sendCombinedConfiguration instead
    void sendCombinedConfiguration(unsigned char* electrodeData, int electrodeDataSize,
                                   double amplitude, double carrierFreq, double burstFreq,
                                   double rampUpRate, double coastDuration, double rampDownRate);  // NEW: Send electrode + signal params together

private:
    explicit SpiHandler(QObject *parent = nullptr);
    ~SpiHandler();

    SpiHandler(const SpiHandler&) = delete;
    SpiHandler& operator=(const SpiHandler&) = delete;

    int spiHandle = -1;
    const int BUFFER_SIZE = 24; // 6 floats × 4 bytes = 24 bytes
};
