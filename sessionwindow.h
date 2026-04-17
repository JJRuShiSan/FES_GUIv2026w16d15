#ifndef SESSIONWINDOW_H
#define SESSIONWINDOW_H

#include <QMainWindow>
#include <QTimer>
#include <QLabel>
#include <QPushButton>

class SessionWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit SessionWindow(QWidget *parent = nullptr);

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
    int elapsedSeconds;      // store total time
};

#endif // SESSIONWINDOW_H
