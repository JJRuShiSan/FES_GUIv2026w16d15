#include "historywindow.h"
#include "mainwindow.h"
#include "startwindow.h"
#include "electrodewindow.h"
#include "globals.h"

#include <QLabel>
#include <QPushButton>
#include <QStatusBar>
#include <QFont>
#include <QPixmap>
#include <QDebug>

// Use the global values from sessionwindow.cpp
extern int g_elapsedSeconds;
extern double g_setAmplitude;      // Original amplitude set by user
extern double g_latestAmplitude;   // Latest amplitude received from Pico
extern double g_carrierFreq;       // Carrier frequency
extern double g_burstFreq;         // Burst frequency

// Static amplitude tracker (DEPRECATED - use g_latestAmplitude instead)
double HistoryWindow::lastAmplitude = 1.0;

HistoryWindow::HistoryWindow(QWidget *parent)
    : QMainWindow(parent)
{
    setWindowTitle("FES_2025");
    this->setCursor(Qt::BlankCursor);
    setStyleSheet("background-color: white;");

    QWidget *centralWidget = new QWidget(this);
    setCentralWidget(centralWidget);

    // Title label
    QLabel *label = new QLabel("History", centralWidget);
    label->setGeometry(0, 0, 801, 51);
    QFont titleFont;
    titleFont.setPointSize(16);
    label->setFont(titleFont);
    label->setStyleSheet("background-color: rgb(141, 25, 36); color: white; padding-right: 20px; font-weight: bold;");
    label->setAlignment(Qt::AlignRight | Qt::AlignVCenter);

    // Buttons
    QPushButton *btnSettings = new QPushButton("Settings", centralWidget);
    btnSettings->setGeometry(450, 110, 321, 70);
    btnSettings->setFont(QFont("Arial", 28));
    btnSettings->setStyleSheet("background:#b21219; color: white;");

    QPushButton *btnMatrix = new QPushButton("Electrode Matrix", centralWidget);
    btnMatrix->setGeometry(450, 210, 321, 70);
    btnMatrix->setFont(QFont("Arial", 28));
    btnMatrix->setStyleSheet("background:#b21219; color: white;");

    QPushButton *btnEnd = new QPushButton("End Session", centralWidget);
    btnEnd->setGeometry(450, 310, 321, 70);
    btnEnd->setFont(QFont("Arial", 28));
    btnEnd->setStyleSheet("background:#b21219; color: white;");

    // Logo
    QLabel *logoLabel = new QLabel(centralWidget);
    logoLabel->setGeometry(90, 60, 261, 131);
    logoLabel->setPixmap(QPixmap(":/FES.png"));
    logoLabel->setScaledContents(true);

    // Session info label
    int mins = g_elapsedSeconds / 60;
    int secs = g_elapsedSeconds % 60;
    QString timeStr = QString("%1:%2")
                          .arg(mins, 2, 10, QChar('0'))
                          .arg(secs, 2, 10, QChar('0'));

    QLabel *sessionInfo = new QLabel(centralWidget);
    sessionInfo->setGeometry(90, 220, 320, 180);
    sessionInfo->setStyleSheet("font-size: 22px; color: black;");
    sessionInfo->setAlignment(Qt::AlignVCenter | Qt::AlignTop);
    
    // Format carrier and burst frequencies
    QString carrierStr = (g_carrierFreq >= 1000) 
                         ? QString::number(g_carrierFreq / 1000.0, 'f', 1) + "k Hz"
                         : QString::number(g_carrierFreq, 'f', 0) + " Hz";
    
    QString burstStr = QString::number(g_burstFreq, 'f', 0) + " Hz";
    
    sessionInfo->setText(
        QString("Total Stimulation Time: %1\n"
                "Set Amplitude: %2 V\n"
                // "Current Amplitude: %3 V\n"
                "Carrier Frequency: %4\n"
                "Burst Frequency: %5")
            .arg(timeStr)
            .arg(QString::number(g_setAmplitude, 'f', 2))
            // .arg(QString::number(g_latestAmplitude, 'f', 1))
            .arg(carrierStr)
            .arg(burstStr));

    // Status bar
    QStatusBar *statusBar = new QStatusBar(this);
    setStatusBar(statusBar);
    statusBar->setSizeGripEnabled(true);

    // Connect buttons
    connect(btnSettings, &QPushButton::clicked, this, &HistoryWindow::onSettingsClicked);
    connect(btnMatrix, &QPushButton::clicked, this, &HistoryWindow::onElectrodeMatrixClicked);
    connect(btnEnd, &QPushButton::clicked, this, &HistoryWindow::onEndSessionClicked);
}

HistoryWindow::~HistoryWindow() {}

void HistoryWindow::onSettingsClicked() {
    this->hide();
    MainWindow *sw = new MainWindow();
    sw->setAttribute(Qt::WA_DeleteOnClose);
    sw->showFullScreen();
}

void HistoryWindow::onElectrodeMatrixClicked() {
    this->hide();
    ElectrodeWindow *ew = new ElectrodeWindow();
    ew->setAttribute(Qt::WA_DeleteOnClose);
    ew->showFullScreen();
}

void HistoryWindow::onEndSessionClicked() {

    // Reset session values
    g_elapsedSeconds = 0;
    g_setAmplitude = 0.0;
    g_latestAmplitude = 0.0;
    lastAmplitude = 1.0;

    savedClickState.clear();
    savedSelected.clear();

    this->hide();
    StartWindow *sw = new StartWindow();
    sw->setAttribute(Qt::WA_DeleteOnClose);
    sw->showFullScreen();
}

