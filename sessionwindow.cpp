#include "sessionwindow.h"
#include "historywindow.h"
#include "spihandler.h"

#include <QMessageBox>
#include <QTimer>
#include <iostream>

// Global trackers shared with history/spi views.
int g_elapsedSeconds = 0;
double g_setAmplitude = 0.0;
double g_latestAmplitude = 0.0;
double g_carrierFreq = 10000.0;
double g_burstFreq = 50.0;

SessionWindow::SessionWindow(int autoStopMs, QWidget *parent)
    : QMainWindow(parent), elapsedSeconds(g_elapsedSeconds)
{
    setWindowTitle("FES_2025");
    this->setCursor(Qt::BlankCursor);
    setStyleSheet("background-color: white;");

    QWidget *centralwidget = new QWidget(this);
    setCentralWidget(centralwidget);

    timerLabel = new QLabel(centralwidget);
    timerLabel->setGeometry(0, 0, 801, 51);
    timerLabel->setAlignment(Qt::AlignCenter);
    timerLabel->setStyleSheet("background-color: rgb(141, 25, 36); color: white; font-weight: bold; font-size: 30px;");

    int mins = elapsedSeconds / 60;
    int secs = elapsedSeconds % 60;
    timerLabel->setText(QString("Total Stimulation Time: %1:%2")
                            .arg(mins, 2, 10, QChar('0'))
                            .arg(secs, 2, 10, QChar('0')));

    amplitudeLabel = new QLabel(centralwidget);
    amplitudeLabel->setGeometry(50, 80, 701, 40);
    amplitudeLabel->setAlignment(Qt::AlignCenter);
    amplitudeLabel->setStyleSheet("background-color: white; color: black; font-weight: bold; font-size: 24px;");

    stopButton = new QPushButton("STOP", centralwidget);
    stopButton->setGeometry(160, 130, 501, 221);
    stopButton->setStyleSheet("background-color: rgb(214, 61, 61); color: white; font-size: 80px; border-radius: 8px; font-weight:  bold");

    connect(stopButton, &QPushButton::clicked, this, &SessionWindow::onStopClicked);

    timer = new QTimer(this);
    connect(timer, &QTimer::timeout, this, &SessionWindow::updateTimer);
    timer->start(1000);

    amplitudeTimer = nullptr;

    autoStopTimer = new QTimer(this);
    autoStopTimer->setSingleShot(true);
    connect(autoStopTimer, &QTimer::timeout, this, &SessionWindow::onAutoStopTimeout);
    if (autoStopMs > 0) {
        autoStopTimer->start(autoStopMs);
        std::cout << "[SESSION] Auto STOP armed at " << autoStopMs << " ms" << std::endl;
    }

    std::cout << "[SESSION] Emergency stop uses low-traffic mode (no amplitude readback)" << std::endl;
}

void SessionWindow::updateTimer()
{
    elapsedSeconds++;
    g_elapsedSeconds = elapsedSeconds;

    int mins = elapsedSeconds / 60;
    int secs = elapsedSeconds % 60;
    timerLabel->setText(QString("Total Stimulation Time: %1:%2")
                            .arg(mins, 2, 10, QChar('0'))
                            .arg(secs, 2, 10, QChar('0')));

    if (elapsedSeconds == 900) {
        QMessageBox::warning(this, "Warning!", "15 minutes has been reached!");
        this->setCursor(Qt::BlankCursor);
    }
}

void SessionWindow::requestAmplitudeData()
{
    float currentAmp = SpiHandler::instance()->requestCurrentAmplitude();
    g_latestAmplitude = currentAmp;
    amplitudeLabel->setText(QString("Current Output: %1 V").arg(currentAmp, 0, 'f', 3));
}

void SessionWindow::onStopClicked()
{
    endSessionAndShowHistory(true);
}

void SessionWindow::onAutoStopTimeout()
{
    // Auto one-shot completion path: avoid extra SPI traffic for STOP.
    endSessionAndShowHistory(false);
}

void SessionWindow::endSessionAndShowHistory(bool sendEmergencyStop)
{
    if (isTransitioning) {
        return;
    }
    isTransitioning = true;

    if (timer) {
        timer->stop();
    }
    if (autoStopTimer) {
        autoStopTimer->stop();
    }

    if (sendEmergencyStop) {
        SpiHandler::instance()->sendEmergencyStop();
    }

    HistoryWindow *hw = new HistoryWindow();
    hw->setAttribute(Qt::WA_DeleteOnClose);
    hw->showFullScreen();
    this->close();
}
