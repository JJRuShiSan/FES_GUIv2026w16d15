#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QPushButton>
#include <QLineEdit>
#include <QLabel>
#include <QTimer>

#include "spihandler.h"

class ClickableLabel : public QLabel {
    Q_OBJECT
public:
    explicit ClickableLabel(QWidget *parent = nullptr) : QLabel(parent) {}
signals:
    void clicked();
protected:
    void mousePressEvent(QMouseEvent *event) override;
};

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

    // Optional: set different image paths at runtime
    void setImagePaths(const QString &carrierPath, const QString &amPath);

signals:
    void amplitudeChanged(double newAmplitude);

public:
    double getAmplitude() const { return amplitude; }

    double getCarrierFreq() const {
        QString carrierText = lblCarrier->text();
        double carrierFreq = 10000.0;
        if (carrierText.contains("kHz")) {
            QStringList parts = carrierText.split('\n');
            if (parts.size() > 1)
                carrierFreq = parts[1].remove(" kHz").trimmed().toDouble() * 1000.0;
        }
        return carrierFreq;
    }

    double getBurstFreq() const {
        QString burstText = lblAM->text();
        double burstFreq = 50.0;
        if (burstText.contains("Hz")) {
            QStringList parts = burstText.split('\n');
            if (parts.size() > 1)
                burstFreq = parts[1].remove(" Hz").trimmed().toDouble();
        }
        return burstFreq;
    }

private slots:
    void onPlusClicked();
    void onMinusClicked();
    void onPlusHold();
    void onMinusHold();
    void stopHold();
    void onElectrodeMatrixClicked();
    void onCarrierClicked();
    void onAMClicked();

private:
    SpiHandler *spiHandler;

private:
    // UI
    QWidget    *topBar = nullptr;
    QLabel     *titleLabel = nullptr;
    QPushButton *plusBtn = nullptr;
    QPushButton *minusBtn = nullptr;
    QPushButton *electrodeBtn = nullptr;
    QLineEdit  *amplitudeDisplay = nullptr;
    ClickableLabel *carrierPic = nullptr;
    ClickableLabel *amPic = nullptr;
    QLabel     *lblCarrier = nullptr;
    QLabel     *lblAM = nullptr;

    // State
    double amplitude = 1.0;             // initial amplitude (clamped 1.0 - 5.0)

    // Hold timers
    QTimer *plusHoldTimer = nullptr;
    QTimer *minusHoldTimer = nullptr;

    // Helpers
    void updateAmplitudeDisplay();
    double clampAmplitude(double val);
    void loadImages(const QString &carrierPath, const QString &amPath);
};

#endif // MAINWINDOW_H
