#include "sessionwindow.h"
#include "historywindow.h"
#include "spihandler.h"
#include "globals.h"

#include <QTimer>
#include <QMessageBox>
#include <iostream>


// Static/global tracker (persists across windows)
int g_elapsedSeconds = 0;

// Global signal parameters for history display
double g_setAmplitude = 0.0;      // Original amplitude set by user
double g_latestAmplitude = 0.0;    // Latest amplitude received from Pico
double g_carrierFreq = 10000.0;    // Carrier frequency
double g_burstFreq = 50.0;         // Burst frequency

SessionWindow::SessionWindow(int autoStopMs, QWidget *parent)
    : QMainWindow(parent), elapsedSeconds(g_elapsedSeconds)   // resume previous time
{

    setWindowTitle("FES_2025");
    this->setCursor(Qt::BlankCursor);
    setStyleSheet("background-color: white;");

    QWidget *centralwidget = new QWidget(this);
    setCentralWidget(centralwidget);

    // Timer label
    timerLabel = new QLabel(centralwidget);
    timerLabel->setGeometry(0, 0, 801, 51);
    timerLabel->setAlignment(Qt::AlignCenter);
    timerLabel->setStyleSheet("background-color: rgb(141, 25, 36); color: white; font-weight: bold; font-size: 30px;");

    // ✅ Initialize label with resumed time
    int mins = elapsedSeconds / 60;
    int secs = elapsedSeconds % 60;
    timerLabel->setText(QString("Total Stimulation Time: %1:%2")
                            .arg(mins, 2, 10, QChar('0'))
                            .arg(secs, 2, 10, QChar('0')));

    // NEW: Amplitude display label
    amplitudeLabel = new QLabel(centralwidget);
    amplitudeLabel->setGeometry(50, 80, 701, 40);
    amplitudeLabel->setAlignment(Qt::AlignCenter);
    amplitudeLabel->setStyleSheet("background-color: white; color: black; font-weight: bold; font-size: 24px;");
    

    // STOP button
    stopButton = new QPushButton("STOP", centralwidget);
    stopButton->setGeometry(160, 130, 501, 221);
    stopButton->setStyleSheet("background-color: rgb(214, 61, 61); color: white; font-size: 80px; border-radius: 8px; font-weight:  bold");

    connect(stopButton, &QPushButton::clicked, this, &SessionWindow::onStopClicked);

    // Timer setup (for elapsed time)
    timer = new QTimer(this);
    connect(timer, &QTimer::timeout, this, &SessionWindow::updateTimer);
    timer->start(1000);  // every 1 sec

    // Auto-trigger STOP using the expected one-shot duration computed in GUI.
    // This avoids extra SPI polling traffic that can disturb command framing.
    autoStopTimer = new QTimer(this);
    autoStopTimer->setSingleShot(true);
    connect(autoStopTimer, &QTimer::timeout, this, &SessionWindow::onAutoStopTimeout);
    if (autoStopMs > 0) {
        autoStopTimer->start(autoStopMs);
        std::cout << "[SESSION] Auto STOP armed at " << autoStopMs << " ms" << std::endl;
    }

    // COMMENTED OUT: Continuous amplitude polling
    // Using simpler approach: capture final amplitude on emergency stop instead
    // amplitudeTimer = new QTimer(this);
    // connect(amplitudeTimer, &QTimer::timeout, this, &SessionWindow::requestAmplitudeData);
    // amplitudeTimer->start(500);  // 500ms initial polling rate
    
    // std::cout << "[SESSION] Amplitude polling started at 500ms interval" << std::endl;
    // std::cout << "[SESSION] Monitor timing diagnostics to optimize poll rate" << std::endl;
    
    std::cout << "[SESSION] Final amplitude will be captured on emergency stop" << std::endl;
}

void SessionWindow::updateTimer() {
    elapsedSeconds++;
    g_elapsedSeconds = elapsedSeconds;  // keep global sync

    int mins = elapsedSeconds / 60;
    int secs = elapsedSeconds % 60;
    timerLabel->setText(QString("Total Stimulation Time: %1:%2")
                            .arg(mins, 2, 10, QChar('0'))
                            .arg(secs, 2, 10, QChar('0')));

    if (elapsedSeconds == 900) {  // 15 minutes
        QMessageBox::warning(this, "Warning!", "15 minutes has been reached!");
        this->setCursor(Qt::BlankCursor);
    }
}

void SessionWindow::requestAmplitudeData() {
    // Request current amplitude from Pico via SPI
    float currentAmp = SpiHandler::instance()->requestCurrentAmplitude();
    
    // Store the latest amplitude globally
    g_latestAmplitude = currentAmp;
    
    // Update display
    amplitudeLabel->setText(QString("Current Output: %1 V")
                               .arg(currentAmp, 0, 'f', 3));
}

void SessionWindow::onStopClicked() {
    endSessionAndShowHistory(true);
}

void SessionWindow::onAutoStopTimeout() {
    // One-shot completion path: do not inject another SPI emergency stop command.
    endSessionAndShowHistory(false);
}

void SessionWindow::endSessionAndShowHistory(bool sendEmergencyStop)
{
    if (isTransitioning) {
        return;
    }
    isTransitioning = true;

    if (autoStopTimer) autoStopTimer->stop();
    if (timer) timer->stop();
    // amplitudeTimer remains disabled in current design.

    if (sendEmergencyStop) {
        // Manual STOP: enforce immediate signal stop + capture final amplitude.
        SpiHandler::instance()->sendEmergencyStop();
    }

    // Open HistoryWindow using the global elapsed time
    HistoryWindow *hw = new HistoryWindow();
    hw->setAttribute(Qt::WA_DeleteOnClose);
    hw->showFullScreen();
    this->close();
}
