#ifndef ELECTRODEWINDOW_H
#define ELECTRODEWINDOW_H

#include <QMainWindow>
#include <QPushButton>
#include <QLabel>
#include <QMap>
#include <QList>
#include "gpioexpander.h"

class ElectrodeWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit ElectrodeWindow(QWidget *parent = nullptr);
    ~ElectrodeWindow();

signals:
    void startSessionRequested(const QList<int> &selectedElectrodes);

private slots:
    void onCellClicked();
    void onStartClicked();
    void onResetClicked();
    void onSettingsClicked();

private:
    // UI Elements
    QLabel *titleLabel;
    QLabel *statusLabel;
    
    QPushButton *pushButton;
    QPushButton *pushButton_2;
    QPushButton *pushButton_3;
    QPushButton *pushButton_4;
    QPushButton *pushButton_5;
    QPushButton *pushButton_6;
    QPushButton *pushButton_7;
    QPushButton *pushButton_8;
    QPushButton *pushButton_9;
    QPushButton *pushButton_10;
    QPushButton *pushButton_11;
    QPushButton *pushButton_12;
    QPushButton *pushButton_13;
    QPushButton *pushButton_14;
    QPushButton *pushButton_15;
    QPushButton *pushButton_16;
    QPushButton *pushButton_17;
    QPushButton *pushButton_18;
    QPushButton *pushButton_19;
    QPushButton *pushButton_20;
    QPushButton *pushButton_21;
    QPushButton *pushButton_22;
    QPushButton *pushButton_23;
    QPushButton *pushButton_24;
    QPushButton *pushButton_25; // Settings
    QPushButton *pushButton_26; // Reset
    QPushButton *pushButton_27; // Start/Play

    QList<QPushButton*> cells;
    
    // Electrode state tracking
    QList<int> selected;
    QMap<int, int> clickState; // 0=OFF, 1=A, 2=B

    // GPIO Expander handling
    GPIOExpander gpioExpanders[3]; // Left (0), Right (1), Center (2)
    bool gpioInitialized;
    
    // Helper methods
    void applyStyle(QPushButton *btn, int idx, int state);
    void updateStatusLabel();
    void initializeGPIOExpanders();
    void updateElectrodeHardware();
    void testSPILoopback(); // For testing without hardware
};

#endif // ELECTRODEWINDOW_H
