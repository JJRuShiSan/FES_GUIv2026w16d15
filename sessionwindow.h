#ifndef SESSIONWINDOW_H
#define SESSIONWINDOW_H

#include <QMainWindow>
#include <QTimer>
#include <QLabel>
#include <QPushButton>

class SessionWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit SessionWindow(int autoStopMs = 0, QWidget *parent = nullptr);

private slots:
    void updateTimer();
    void onStopClicked();
    void requestAmplitudeData();  // NEW: Request amplitude from Pico

private:
    QLabel *timerLabel;
    QLabel *amplitudeLabel;  // NEW: Display current amplitude
    QPushButton *stopButton;
    QTimer *timer;
    QTimer *amplitudeTimer;  // NEW: Timer for amplitude polling
    QTimer *autoStopTimer;   // NEW: Auto-trigger STOP after one-shot duration
    int elapsedSeconds;      // store total time
    bool isTransitioning = false;

    void endSessionAndShowHistory(bool sendEmergencyStop);
};

#endif // SESSIONWINDOW_H
