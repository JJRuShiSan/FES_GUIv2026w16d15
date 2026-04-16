#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QHBoxLayout>
#include <QMainWindow>
#include <QPushButton>
#include <QLineEdit>
#include <QLabel>
#include <QTimer>

#include "spihandler.h"

// =============================================================
// ClickableLabel
// =============================================================
class ClickableLabel : public QLabel {
    Q_OBJECT
public:
    explicit ClickableLabel(QWidget *parent = nullptr) : QLabel(parent) {}
signals:
    void clicked();
protected:
    void mousePressEvent(QMouseEvent *event) override;
};

// =============================================================
// MainWindow
// =============================================================
class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

    void setImagePaths(const QString &carrierPath, const QString &amPath);

signals:
    void amplitudeChanged(double newAmplitude);

public:
    // Getters
    double getAmplitude() const { return amplitude; }
    double getRampUp()    const { return rampUp;    }
    double getCoast()     const { return coast;     }
    double getRampDown()  const { return rampDown;  }

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
    // --- Amplitude ---
    void onAmpPlusClicked();
    void onAmpMinusClicked();
    void onAmpPlusHold();
    void onAmpMinusHold();

    // --- Ramp Up ---
    void onRampUpPlusClicked();
    void onRampUpMinusClicked();
    void onRampUpPlusHold();
    void onRampUpMinusHold();

    // --- Coast ---
    void onCoastPlusClicked();
    void onCoastMinusClicked();
    void onCoastPlusHold();
    void onCoastMinusHold();

    // --- Ramp Down ---
    void onRampDownPlusClicked();
    void onRampDownMinusClicked();
    void onRampDownPlusHold();
    void onRampDownMinusHold();

    // --- General ---
    void stopAllHolds();
    void onElectrodeMatrixClicked();
    void onCarrierClicked();
    void onAMClicked();

    // Legacy aliases (backward compatibility)
    void onPlusClicked()  { onAmpPlusClicked();  }
    void onMinusClicked() { onAmpMinusClicked(); }
    void onPlusHold()     { onAmpPlusHold();     }
    void onMinusHold()    { onAmpMinusHold();    }
    void stopHold()       { stopAllHolds();      }

private:
    SpiHandler *spiHandler = nullptr;

    // --- Top bar ---
    QWidget *topBar     = nullptr;
    QLabel  *titleLabel = nullptr;

    // --- Amplitude controls ---
    QPushButton *ampPlusBtn       = nullptr;
    QPushButton *ampMinusBtn      = nullptr;
    QLineEdit   *amplitudeDisplay = nullptr;

    // --- Ramp Up controls ---
    QPushButton *rampUpPlusBtn  = nullptr;
    QPushButton *rampUpMinusBtn = nullptr;
    QLineEdit   *rampUpDisplay  = nullptr;

    // --- Coast controls ---
    QPushButton *coastPlusBtn  = nullptr;
    QPushButton *coastMinusBtn = nullptr;
    QLineEdit   *coastDisplay  = nullptr;

    // --- Ramp Down controls ---
    QPushButton *rampDownPlusBtn  = nullptr;
    QPushButton *rampDownMinusBtn = nullptr;
    QLineEdit   *rampDownDisplay  = nullptr;

    // --- Right column ---
    QPushButton    *electrodeBtn = nullptr;
    ClickableLabel *carrierPic   = nullptr;   // used as FES signal image
    ClickableLabel *amPic        = nullptr;   // hidden, kept for API compat
    QLabel         *lblCarrier   = nullptr;   // hidden, kept for getCarrierFreq()
    QLabel         *lblAM        = nullptr;   // hidden, kept for getBurstFreq()

    // --- State values ---
    double amplitude = 1.0;    // range: 1.0 V  .. 5.0 V
    double rampUp    = 1.0;    // range: 0.1 V/s .. 3.0 V/s
    double coast     = 1.0;    // range: 0.0 s  .. 10.0 s
    double rampDown  = -1.0;   // range: -0.1 V/s .. -3.0 V/s

    // --- Hold timers (one +/- pair per parameter) ---
    QTimer *ampPlusHoldTimer       = nullptr;
    QTimer *ampMinusHoldTimer      = nullptr;
    QTimer *rampUpPlusHoldTimer    = nullptr;
    QTimer *rampUpMinusHoldTimer   = nullptr;
    QTimer *coastPlusHoldTimer     = nullptr;
    QTimer *coastMinusHoldTimer    = nullptr;
    QTimer *rampDownPlusHoldTimer  = nullptr;
    QTimer *rampDownMinusHoldTimer = nullptr;

    // --- Helper methods ---
    QPushButton* makeRedBtn(const QString &label);
    QLineEdit*   makeDisplay(const QString &initText);
    QHBoxLayout* buildParamRow(QPushButton *&minusOut, QLineEdit *&displayOut,
                               QPushButton *&plusOut,  const QString &initText);
    void wireHold(QPushButton *btn, QTimer *&holdTimer,
                  void (MainWindow::*holdSlot)(),
                  void (MainWindow::*clickSlot)());

    void updateAmplitudeDisplay();
    void updateRampUpDisplay();
    void updateCoastDisplay();
    void updateRampDownDisplay();

    double clampAmplitude(double v);
    double clampRampUp(double v);
    double clampCoast(double v);
    double clampRampDown(double v);

    void loadImages(const QString &carrierPath, const QString &amPath);

    // Legacy pointers (alias to amp buttons)
    QPushButton *plusBtn       = nullptr;
    QPushButton *minusBtn      = nullptr;
    QTimer      *plusHoldTimer  = nullptr;
    QTimer      *minusHoldTimer = nullptr;
};

#endif // MAINWINDOW_H
