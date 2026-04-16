#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QHBoxLayout>
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

    void setImagePaths(const QString &carrierPath, const QString &amPath);

signals:
    void amplitudeChanged(double newAmplitude);

public:
    double getAmplitude()  const { return amplitude; }
    double getRampUp()     const { return rampUp; }
    double getCoast()      const { return coast; }
    double getRampDown()   const { return rampDown; }

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
    // Amplitude
    void onAmpPlusClicked();    void onAmpMinusClicked();
    void onAmpPlusHold();       void onAmpMinusHold();
    // Ramp Up
    void onRampUpPlusClicked(); void onRampUpMinusClicked();
    void onRampUpPlusHold();    void onRampUpMinusHold();
    // Coast
    void onCoastPlusClicked();  void onCoastMinusClicked();
    void onCoastPlusHold();     void onCoastMinusHold();
    // Ramp Down
    void onRampDownPlusClicked(); void onRampDownMinusClicked();
    void onRampDownPlusHold();    void onRampDownMinusHold();

    void stopAllHolds();
    void onElectrodeMatrixClicked();
    void onCarrierClicked();
    void onAMClicked();

    // Legacy slot aliases kept for backward compat
    void onPlusClicked()  { onAmpPlusClicked(); }
    void onMinusClicked() { onAmpMinusClicked(); }
    void onPlusHold()     { onAmpPlusHold(); }
    void onMinusHold()    { onAmpMinusHold(); }
    void stopHold()       { stopAllHolds(); }

private:
    SpiHandler *spiHandler;

    // UI – top bar
    QWidget *topBar    = nullptr;
    QLabel  *titleLabel = nullptr;

    // UI – Amplitude
    QPushButton *ampPlusBtn   = nullptr;
    QPushButton *ampMinusBtn  = nullptr;
    QLineEdit   *amplitudeDisplay = nullptr;

    // UI – Ramp Up
    QPushButton *rampUpPlusBtn  = nullptr;
    QPushButton *rampUpMinusBtn = nullptr;
    QLineEdit   *rampUpDisplay  = nullptr;

    // UI – Coast
    QPushButton *coastPlusBtn  = nullptr;
    QPushButton *coastMinusBtn = nullptr;
    QLineEdit   *coastDisplay  = nullptr;

    // UI – Ramp Down
    QPushButton *rampDownPlusBtn  = nullptr;
    QPushButton *rampDownMinusBtn = nullptr;
    QLineEdit   *rampDownDisplay  = nullptr;

    // UI – right column
    QPushButton    *electrodeBtn = nullptr;
    ClickableLabel *carrierPic   = nullptr;
    ClickableLabel *amPic        = nullptr;
    QLabel         *lblCarrier   = nullptr;
    QLabel         *lblAM        = nullptr;

    // State
    double amplitude = 1.0;   // 1.0 V  – 5.0 V
    double rampUp    = 1.0;   // 0.1 V/s – 3.0 V/s
    double coast     = 1.0;   // 0.0 s   – 10.0 s
    double rampDown  = -1.0;  // -0.1 V/s – -3.0 V/s

    // Hold timers (one pair per parameter)
    QTimer *ampPlusHoldTimer      = nullptr;  QTimer *ampMinusHoldTimer      = nullptr;
    QTimer *rampUpPlusHoldTimer   = nullptr;  QTimer *rampUpMinusHoldTimer   = nullptr;
    QTimer *coastPlusHoldTimer    = nullptr;  QTimer *coastMinusHoldTimer    = nullptr;
    QTimer *rampDownPlusHoldTimer = nullptr;  QTimer *rampDownMinusHoldTimer = nullptr;

    // Helpers
    void updateAmplitudeDisplay();
    void updateRampUpDisplay();
    void updateCoastDisplay();
    void updateRampDownDisplay();

    double clampAmplitude(double v);
    double clampRampUp(double v);
    double clampCoast(double v);
    double clampRampDown(double v);

    // Convenience: build a standard +/- row
    QHBoxLayout* buildParamRow(QPushButton *&minusOut, QLineEdit *&displayOut,
                               QPushButton *&plusOut,  const QString &initText);
    // Convenience: make a standard red square button
    QPushButton* makeRedBtn(const QString &label);
    // Convenience: make a read-only display field
    QLineEdit*   makeDisplay(const QString &initText);
    // Wire a hold timer to a button
    void wireHold(QPushButton *btn, QTimer *&holdTimer,
                  void (MainWindow::*holdSlot)(),
                  void (MainWindow::*clickSlot)());

    void loadImages(const QString &carrierPath, const QString &amPath);

    // kept for legacy
    QPushButton *plusBtn  = nullptr;
    QPushButton *minusBtn = nullptr;
    QTimer *plusHoldTimer  = nullptr;
    QTimer *minusHoldTimer = nullptr;
};

#endif // MAINWINDOW_H