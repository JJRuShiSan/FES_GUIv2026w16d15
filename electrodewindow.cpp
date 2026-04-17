#include "electrodewindow.h"
#include "sessionwindow.h"
#include "mainwindow.h"
#include "spihandler.h"
#include "globals.h"

// DEFINE the globals exactly once here
QVector<int> savedSelected;
QMap<int,int> savedClickState;

#include <QFont>
#include <QMessageBox>
#include <QApplication>
#include <cmath>
#include <pigpio.h>

extern double g_setAmplitude;
extern double g_carrierFreq;
extern double g_burstFreq;


// --- Styles ---
const QString styleBigOff   = "background:#273941; border-radius:12px;";
const QString styleSmallOff = "background:#273941; border-radius:6px;";
const QString styleBigA     = "background:#ff4d4f; color:white; font-size:32px; font-weight:bold; border-radius:12px;";
const QString styleSmallA   = "background:#ff4d4f; color:white; font-size:32px; font-weight:bold; border-radius:6px;";
const QString styleBigB     = "background:#007bff; color:white; font-size:32px; font-weight:bold; border-radius:12px;";
const QString styleSmallB   = "background:#007bff; color:white; font-size:32px; font-weight:bold; border-radius:6px;";


ElectrodeWindow::ElectrodeWindow(QWidget *parent)
    : QMainWindow(parent), gpioInitialized(false)
{
    setWindowTitle("FES_2025");
    this->setCursor(Qt::BlankCursor);
    setStyleSheet("background-color: white;");

    QWidget *centralWidget = new QWidget(this);
    setCentralWidget(centralWidget);

    // Title
    titleLabel = new QLabel("Electrode Matrix", centralWidget);
    titleLabel->setGeometry(0, 0, 801, 51);
    QFont titleFont; titleFont.setPointSize(16);
    titleLabel->setFont(titleFont);
    titleLabel->setStyleSheet("background-color: rgb(141, 25, 36); color: white; padding-right: 20px; font-weight: bold;");
    titleLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);

    // Helper for buttons
    QFont btnFont("Roboto", 24);
    auto makeButton = [&](QPushButton *&btn, int x, int y, int w, int h, const QString &text = "", const QString &style = styleSmallOff) {
        btn = new QPushButton(text, centralWidget);
        btn->setGeometry(x, y, w, h);
        btn->setFont(btnFont);
        btn->setStyleSheet(style);
    };

    // Big buttons (index 0-3)
    makeButton(pushButton,   50,  60, 150, 150);
    makeButton(pushButton_2, 210, 60, 150, 150);
    makeButton(pushButton_3, 210, 220,150, 150);
    makeButton(pushButton_4, 50,  220,150, 150);

    // Small buttons (index 4-23)
    makeButton(pushButton_5,  450, 140, 68, 68);
    makeButton(pushButton_13, 450, 220, 68, 68);
    makeButton(pushButton_6,  370, 140, 68, 68);
    makeButton(pushButton_7,  450, 60,  68, 68);
    makeButton(pushButton_8,  370, 60,  68, 68);
    makeButton(pushButton_9,  610, 60,  68, 68);
    makeButton(pushButton_10, 530, 140, 68, 68);
    makeButton(pushButton_11, 530, 60,  68, 68);
    makeButton(pushButton_12, 610, 140, 68, 68);
    makeButton(pushButton_14, 610, 300, 68, 68);
    makeButton(pushButton_15, 370, 300, 68, 68);
    makeButton(pushButton_16, 370, 220, 68, 68);
    makeButton(pushButton_17, 530, 300, 68, 68);
    makeButton(pushButton_18, 530, 220, 68, 68);
    makeButton(pushButton_19, 450, 300, 68, 68);
    makeButton(pushButton_20, 610, 220, 68, 68);
    makeButton(pushButton_21, 690, 140, 68, 68);
    makeButton(pushButton_22, 690, 300, 68, 68);
    makeButton(pushButton_23, 690, 220, 68, 68);
    makeButton(pushButton_24, 690, 60,  68, 68);

    // Control buttons (uniform size & spacing)
    makeButton(pushButton_25, 400, 380, 100, 90, "⚙",
               "background:#d63d3d; color:white; font-size:28px; border-radius:20px;");
    makeButton(pushButton_26, 520, 380, 100, 90, "⟳",
               "background:#d63d3d; color:white; font-size:28px; border-radius:20px;");
    makeButton(pushButton_27, 640, 380, 100 , 90, "▶",
               "background:#d63d3d; color:white; font-size:28px; border-radius:20px;");

    // Collect cells - mapped to match original electrode layout
    // Index mapping to electrode names:
    // [0]=AB43, [1]=AB65, [2]=CD65, [3]=CD43,
    // [4]=A7, [5]=A8, [6]=A9, [7]=A10, [8]=A11,
    // [9]=B7, [10]=B8, [11]=B9, [12]=B10, [13]=B11,
    // [14]=C7, [15]=C8, [16]=C9, [17]=C10, [18]=C11,
    // [19]=D7, [20]=D8, [21]=D9, [22]=D10, [23]=D11
    // cells = {
    //     pushButton,   // [0] AB43
    //     pushButton_2, // [1] AB65
    //     pushButton_3, // [2] CD65
    //     pushButton_4, // [3] CD43
    //     pushButton_8,  // [4] A7
    //     pushButton_7,  // [5] A8
    //     pushButton_11, // [6] A9
    //     pushButton_9,  // [7] A10
    //     pushButton_24, // [8] A11
    //     pushButton_6,  // [9] B7
    //     pushButton_5,  // [10] B8
    //     pushButton_10, // [11] B9
    //     pushButton_12, // [12] B10
    //     pushButton_21, // [13] B11
    //     pushButton_15, // [14] C7
    //     pushButton_13, // [15] C8
    //     pushButton_18, // [16] C9
    //     pushButton_20, // [17] C10
    //     pushButton_23, // [18] C11
    //     pushButton_16, // [19] D7
    //     pushButton_19, // [20] D8
    //     pushButton_17, // [21] D9
    //     pushButton_14, // [22] D10
    //     pushButton_22  // [23] D11
    // };

    cells = {
        pushButton,   // [0] AB43
        pushButton_2, // [1] AB65
        pushButton_8,  // [2] A7
        pushButton_7,  // [3] A8
        pushButton_11, // [4] A9
        pushButton_9,  // [5] A10
        pushButton_24, // [6] A11
        pushButton_21, // [7] B11
        pushButton_6, // [8] B10
        pushButton_5, // [9] B9
        pushButton_10,  // [10] B8
        pushButton_12,  // [11] B7
        pushButton_20, // [12] C7
        pushButton_18, // [13] C8
        pushButton_13, // [14] C9
        pushButton_16, // [15] C10
        pushButton_17, // [16] C11
        pushButton_14,  // [17] D11
        pushButton_22, // [18] D10
        pushButton_23, // [19] D9
        pushButton_4, // [20] D8
        pushButton_3, // [21] D7
        pushButton_15, // [22] CD65
        pushButton_19, // [23] CD43
    };

    // Assign indices
    for (int i = 0; i < cells.size(); ++i) {
        cells[i]->setProperty("idx", i);
        cells[i]->setStyleSheet(i < 4 ? styleBigOff : styleSmallOff);
        connect(cells[i], &QPushButton::clicked, this, &ElectrodeWindow::onCellClicked);
    }

    // Control connections
    connect(pushButton_25, &QPushButton::clicked, this, &ElectrodeWindow::onSettingsClicked);
    connect(pushButton_26, &QPushButton::clicked, this, &ElectrodeWindow::onResetClicked);
    connect(pushButton_27, &QPushButton::clicked, this, &ElectrodeWindow::onStartClicked);

    // Text label bottom-left
    statusLabel = new QLabel("Active Electrodes: 0", centralWidget);
    statusLabel->setGeometry(30, 400, 300, 50);
    statusLabel->setStyleSheet("font-size: 30px; color: black; border: 4px solid #d63d3d; ");
    statusLabel->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);

    // GPIO expanders auto-initialize when Pico boots - no command needed
    qDebug() << "[INFO] Electrode matrix ready (GPIO expanders initialized at Pico startup)";

    // Restore previous selection if available
    if (!savedClickState.isEmpty() || !savedSelected.isEmpty()) {
        selected = savedSelected;
        clickState = savedClickState;

        for (int i = 0; i < cells.size(); ++i) {
            int s = clickState.value(i, 0);
            applyStyle(cells[i], i, s);
        }
    }



    updateStatusLabel();
}

ElectrodeWindow::~ElectrodeWindow() 
{
    qDebug() << "ElectrodeWindow destroyed";
}

void ElectrodeWindow::initializeGPIOExpanders()
{
    qDebug() << "\n========================================";
    qDebug() << "Initializing GPIO Expanders for Electrodes";
    qDebug() << "========================================";
    
    // Initialize pigpio library
    if (gpioInitialise() < 0) {
        qDebug() << "[ERROR] Failed to initialize pigpio!";
        qDebug() << "        Make sure to run: sudo pigpiod";
        gpioInitialized = false;
        
        QMessageBox::warning(this, "GPIO Error", 
            "Failed to initialize GPIO!\n\n"
            "Make sure pigpiod daemon is running:\n"
            "sudo pigpiod\n\n"
            "Electrode control will not work.");
        return;
    }
    
    qDebug() << "[OK] pigpio library initialized";
    
    
    gpioExpanders[0].initGPIOExpander(HWA_LEFT, 1);   // Left board
    gpioExpanders[1].initGPIOExpander(HWA_RIGHT, 1);  // Right board
    gpioExpanders[2].initGPIOExpander(HWA_CENTER, 1); // Center board
    
    // Check if all initialized successfully
    gpioInitialized = gpioExpanders[0].getInitStatus() &&
                      gpioExpanders[1].getInitStatus() &&
                      gpioExpanders[2].getInitStatus();
    
    if (gpioInitialized) {
        qDebug() << "\n[SUCCESS] All GPIO Expanders initialized!";
        qDebug() << "========================================\n";
        
        // Test SPI loopback if needed
        // testSPILoopback();
    } else {
        qDebug() << "\n[WARNING] Some GPIO Expanders failed to initialize";
        qDebug() << "          Electrode control may not work properly";
        qDebug() << "========================================\n";
    }
}

void ElectrodeWindow::testSPILoopback()
{
    qDebug() << "\n========== SPI Loopback Test ==========";
    qDebug() << "Testing SPI communication with GPIO expanders...";
    
    for (int i = 0; i < 3; i++) {
        if (!gpioExpanders[i].getInitStatus()) {
            qDebug() << "[SKIP] Board" << i << "not initialized";
            continue;
        }
        
        qDebug() << "\n[Test] Board" << i;
        
        // Read GPIOA register
        unsigned char gpioaVal = gpioExpanders[i].readRegister(GPIOA_REG);
        qDebug() << "  GPIOA value:" << QString::number(gpioaVal, 2).rightJustified(8, '0');
        
        // Read GPIOB register
        unsigned char gpiobVal = gpioExpanders[i].readRegister(GPIOB_REG);
        qDebug() << "  GPIOB value:" << QString::number(gpiobVal, 2).rightJustified(8, '0');
        
        // Read IODIRA (should be 0x00 - all outputs)
        unsigned char iodiraVal = gpioExpanders[i].readRegister(IODIRA_REG);
        qDebug() << "  IODIRA value:" << QString::number(iodiraVal, 2).rightJustified(8, '0')
                 << (iodiraVal == 0x00 ? "[OK - All outputs]" : "[ERROR]");
    }
    
    qDebug() << "=======================================\n";
}

void ElectrodeWindow::applyStyle(QPushButton *btn, int idx, int state)
{
    if (state == 0) {
        btn->setText("");
        btn->setStyleSheet(idx < 4 ? styleBigOff : styleSmallOff);
    } else if (state == 1) {
        btn->setText("A");
        btn->setStyleSheet(idx < 4 ? styleBigA : styleSmallA);
    } else {
        btn->setText("B");
        btn->setStyleSheet(idx < 4 ? styleBigB : styleSmallB);
    }
}

void ElectrodeWindow::onCellClicked()
{
    QPushButton *btn = qobject_cast<QPushButton*>(sender());
    if (!btn) return;
    int idx = btn->property("idx").toInt();

    int state = (clickState.value(idx, 0) + 1) % 3;
    clickState[idx] = state;

    applyStyle(btn, idx, state);

    if (state == 0) {
        selected.removeOne(idx);
    } else {
        if (!selected.contains(idx))
            selected.append(idx);
    }

    updateStatusLabel();

    savedSelected = selected;
    savedClickState = clickState;
}


void ElectrodeWindow::updateElectrodeHardware()
{
    qDebug() << "\n[UPDATE] Updating electrode hardware configuration via SPI to Pico...";
    
    // Create configuration array for all 24 electrodes across 3 MCP23S17 boards
    // Format: 6 bytes = [GPIOB_Board0][GPIOA_Board0][GPIOB_Board1][GPIOA_Board1][GPIOB_Board2][GPIOA_Board2]
    // GPIOB = A/B electrode selection (bit=1 for A, bit=0 for B)
    // GPIOA = ON/OFF control (bit=0 for ON, bit=1 for OFF)
    // Each board handles 8 electrodes (0-7, 8-15, 16-23)
    unsigned char electrodeData[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};  // All OFF by default
    
    // Pack electrode states into 6 bytes (2 bytes per board)
    for (int idx = 0; idx < 24; idx++) {
        int state = clickState.value(idx, 0); // 0=OFF, 1=A, 2=B
        
        int board = idx / 8;       // Board 0, 1, or 2
        int bit = idx % 8;         // Bit position 0-7
        int gpiobIdx = board * 2;  // GPIOB byte index
        int gpioaIdx = board * 2 + 1;  // GPIOA byte index
        
        if (state == 0) {
            // OFF: GPIOA bit = 1 (electrode OFF), GPIOB bit doesn't matter (keep default)
            // Default is already 0xFF (all OFF)
        } else if (state == 1) {
            // A selected: GPIOA bit = 0 (ON), GPIOB bit = 1 (A)
            electrodeData[gpioaIdx] &= ~(1 << bit);  // Clear bit = ON
            electrodeData[gpiobIdx] |= (1 << bit);   // Set bit = A
        } else if (state == 2) {
            // B selected: GPIOA bit = 0 (ON), GPIOB bit = 0 (B)
            electrodeData[gpioaIdx] &= ~(1 << bit);  // Clear bit = ON
            electrodeData[gpiobIdx] &= ~(1 << bit); // Clear bit = B
        }
    }
    
    qDebug() << "[ELECTRODE] Packed data (6 bytes):";
    QString dataStr = "  ";
    for (int i = 0; i < 6; i++) {
        dataStr += QString("0x%1 ").arg(electrodeData[i], 2, 16, QChar('0'));
    }
    qDebug() << dataStr;
    
    // Send electrode configuration to Pico via SPI
    SpiHandler::instance()->sendElectrodeConfiguration(electrodeData, 6);
    
    qDebug() << "[UPDATE] Electrode configuration sent to Pico";
}

void ElectrodeWindow::updateStatusLabel()
{
    QString msg = QString("Active Electrodes: %1").arg(selected.size());
    statusLabel->setText(msg);
}

void ElectrodeWindow::onStartClicked()
{
	
    if (selected.size() < 2) {
        QMessageBox msgBox(this);
        msgBox.setText("Please select at least 1 pair of electrodes A & B");
        msgBox.setStyleSheet("QMessageBox {background-color: #white; font-size: 18px; color: black;} "
                            "QLabel {color: black; font-size: 18px;} "
                            "QPushButton {background-color: #d63d3d; color: white; font-size: 16px; "
                            "padding: 6px 14px; border-radius: 8px; min-width: 80px;} "
                            "QPushButton:hover {background-color: #a62c2c;}");
        msgBox.setIcon(QMessageBox::Warning);
        msgBox.exec();
        return;
    }

    // Check that at least 1 "A" and 1 "B" are selected
    bool hasA = false;
    bool hasB = false;
    for (int idx : selected) {
        int state = clickState.value(idx, 0);
        if (state == 1) hasA = true;
        if (state == 2) hasB = true;
    }

    if (!hasA || !hasB) {
        QMessageBox msgBox(this);
        msgBox.setWindowTitle("Warning!");
        msgBox.setText("Please select at least 1 pair of electrodes A & B");
        msgBox.setStyleSheet("QMessageBox {background-color: #white; font-size: 18px; color: black;} "
                            "QLabel {color: black; font-size: 18px;} "
                            "QPushButton {background-color: #d63d3d; color: white; font-size: 16px; "
                            "padding: 6px 14px; border-radius: 8px; min-width: 80px;} "
                            "QPushButton:hover {background-color: #a62c2c;}");
        msgBox.setIcon(QMessageBox::Warning);
        msgBox.setCursor(Qt::BlankCursor);
        msgBox.exec();
        return;
    }

    MainWindow *mw = qobject_cast<MainWindow*>(parentWidget());
    if (!mw) {
        const auto topWidgets = QApplication::topLevelWidgets();
        for (QWidget *w : topWidgets) {
            MainWindow *candidate = qobject_cast<MainWindow*>(w);
            if (candidate) {
                mw = candidate;
                break;
            }
        }
    }

    // Use live MainWindow values when available; otherwise keep existing amplitude/frequency globals
    // and safe defaults for ramp/coast/ramp-down to avoid creating a default MainWindow instance.
    double amp = (mw != nullptr) ? mw->getAmplitude() : g_setAmplitude;
    double carrier = (mw != nullptr) ? mw->getCarrierFreq() : g_carrierFreq;
    double burst = (mw != nullptr) ? mw->getBurstFreq() : g_burstFreq;
    double rampUp = (mw != nullptr) ? mw->getRampUp() : 1.0;
    double coast = (mw != nullptr) ? mw->getCoast() : 1.0;
    double rampDown = (mw != nullptr) ? mw->getRampDown() : 1.0;

    g_setAmplitude = amp;
    g_carrierFreq = carrier;
    g_burstFreq = burst;

    // Prepare electrode configuration data (6 bytes)
    // Format: [GPIOB_Board0][GPIOA_Board0][GPIOB_Board1][GPIOA_Board1][GPIOB_Board2][GPIOA_Board2]
    unsigned char electrodeData[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};  // All OFF by default
    for (int idx = 0; idx < 24; idx++) {
        int state = clickState.value(idx, 0); // 0=OFF, 1=A, 2=B
        
        int board = idx / 8;       // Board 0, 1, or 2
        int bit = idx % 8;         // Bit position 0-7
        int gpiobIdx = board * 2;  // GPIOB byte index
        int gpioaIdx = board * 2 + 1;  // GPIOA byte index
        
        if (state == 1) {
            // A selected: GPIOA bit = 0 (ON), GPIOB bit = 1 (A)
            electrodeData[gpioaIdx] &= ~(1 << bit);  // Clear bit = ON
            electrodeData[gpiobIdx] |= (1 << bit);   // Set bit = A
        } else if (state == 2) {
            // B selected: GPIOA bit = 0 (ON), GPIOB bit = 0 (B)
            electrodeData[gpioaIdx] &= ~(1 << bit);  // Clear bit = ON
            electrodeData[gpiobIdx] &= ~(1 << bit); // Clear bit = B
        }
        // state == 0: OFF (default 0xFF already set)
    }

    // Send combined electrode configuration + signal parameters in one SPI transaction
    SpiHandler::instance()->sendCombinedConfiguration(electrodeData, 6, amp, carrier, burst,
                                                      rampUp, coast, rampDown);
    qDebug() << "[COMBINED] Electrode config + signal parameters sent to Pico";
    qDebug() << "  Amplitude:" << amp << "V, Carrier:" << carrier << "Hz, Burst:" << burst
             << "Hz, RampUp:" << rampUp << "V/s, Coast:" << coast << "s, RampDown:" << rampDown << "V/s";
    
    qDebug() << "\n[SESSION] Starting session with" << selected.size() << "active electrodes";

    // Compute one-shot total duration from current parameters:
    // total = (amp / rampUpRate) + coast + (amp / rampDownRate)
    const double safeRampUp = (rampUp > 0.001) ? rampUp : 0.001;
    const double safeRampDown = (rampDown > 0.001) ? rampDown : 0.001;
    const double safeCoast = (coast >= 0.0) ? coast : 0.0;
    const double totalSec = (amp / safeRampUp) + safeCoast + (amp / safeRampDown);
    const int autoStopMs = static_cast<int>(std::ceil(totalSec * 1000.0));

    qDebug() << "[SESSION] Computed one-shot duration:" << totalSec << "s (" << autoStopMs << "ms)";

    // All checks passed, start the session
    SessionWindow *sw = new SessionWindow(autoStopMs, nullptr);
    sw->setAttribute(Qt::WA_DeleteOnClose);

    emit startSessionRequested(selected);

    sw->showFullScreen();
    this->close(); // hide current electrode window
}

void ElectrodeWindow::onResetClicked()
{
    qDebug() << "\n[RESET] Resetting all electrodes (local state only)";
    
    selected.clear();
    clickState.clear();

    savedSelected.clear();
    savedClickState.clear();

    for (int i = 0; i < cells.size(); ++i)
        applyStyle(cells[i], i, 0);

    updateStatusLabel();
    
    // Note: Electrode configuration will be sent when Play is pressed
    // No immediate SPI transmission - this reduces unnecessary traffic
    qDebug() << "[RESET] Local electrode state cleared (will apply on next Play)";
}

void ElectrodeWindow::onSettingsClicked()
{
    MainWindow *mw = new MainWindow();
    mw->setAttribute(Qt::WA_DeleteOnClose);
    mw->showFullScreen();
    this->close();
}
