#include "mainwindow.h"
#include "electrodewindow.h"
#include "historywindow.h"
#include "spihandler.h"
#include "globals.h"

#include <QStringList>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QMouseEvent>
#include <QPixmap>
#include <QDebug>


extern double g_maxAmplitude;   // declaration
//extern SpiHandler *g_spiHandler;

// =============================================================
// ClickableLabel implementation
// =============================================================
void ClickableLabel::mousePressEvent(QMouseEvent *event) {
    emit clicked();
    QLabel::mousePressEvent(event);
}

// =============================================================
// MainWindow constructor & layout setup
// =============================================================
MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent),
    amplitude(1.0)
{
	//spiHandler = g_spiHandler;
    setWindowTitle("FES_2025");
    this->setCursor(Qt::BlankCursor);
     amplitude = HistoryWindow::lastAmplitude;
    // ---------------- Central Layout ----------------
    QWidget *central = new QWidget(this);
    QVBoxLayout *mainLayout = new QVBoxLayout(central);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);
    central->setStyleSheet("background: white; border: none;");
    setStyleSheet("QMainWindow { border: none; }");

    // ---------------- Top Bar ----------------
    topBar = new QWidget(central);
    topBar->setFixedHeight(60);
    topBar->setStyleSheet("background-color: rgb(141,25,36);");

    QHBoxLayout *topBarLayout = new QHBoxLayout(topBar);
    topBarLayout->setContentsMargins(20, 0, 20, 0);

    // Spacer pushes text to right
    topBarLayout->addStretch();

    titleLabel = new QLabel("Stimulation Settings", topBar);
    titleLabel->setStyleSheet(
        "color: white; font-size:20px; font-weight:600; "
        "background: transparent; border: none; font-weight: bold;"
        );
    titleLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);

    topBarLayout->addWidget(titleLabel);
    mainLayout->addWidget(topBar);

    // ---------------- Content Area ----------------
    QWidget *content = new QWidget(central);
    content->setStyleSheet("background-color: white;");
    QVBoxLayout *contentLayout = new QVBoxLayout(content);
    contentLayout->setContentsMargins(40, 20, 40, 20);
    contentLayout->addSpacing(10);

    // Middle row: LEFT (amplitude + freq) | RIGHT (electrode)
    QHBoxLayout *middleRow = new QHBoxLayout;

    // =============================================================
    // LEFT COLUMN: Amplitude Control + Frequencies
    // =============================================================
    QVBoxLayout *leftCol = new QVBoxLayout;

    // --- Heading ---
    QLabel *heading = new QLabel("Set Maximum Amplitude");
    heading->setAlignment(Qt::AlignCenter);
    heading->setStyleSheet("font-size:28px; font-weight:600; color: #222;");
    leftCol->addWidget(heading);
    leftCol->addSpacing(12);

    // --- Amplitude row (-  [value]  +) ---
    QHBoxLayout *ampRow = new QHBoxLayout;
    ampRow->addStretch();

    // Minus button
    minusBtn = new QPushButton("-");
    minusBtn->setFixedSize(60, 60);
    minusBtn->setStyleSheet(
        "QPushButton { background: rgb(214,61,61); color: white; "
        "border-radius:6px; font-size:28px; }"
        "QPushButton:pressed { background: rgb(180,40,40); }"
        );
    ampRow->addWidget(minusBtn);
    ampRow->addSpacing(24);

    // Amplitude display
    amplitudeDisplay = new QLineEdit("0.00 V");
    amplitudeDisplay->setAlignment(Qt::AlignCenter);
    amplitudeDisplay->setReadOnly(true);
    amplitudeDisplay->setFixedSize(150, 60);
    amplitudeDisplay->setStyleSheet(
        "QLineEdit { background: white; color: black; "
        "border-radius: 6px; font-size: 24px; font-weight: bold; }"
        );
    ampRow->addWidget(amplitudeDisplay);
    ampRow->addSpacing(24);

    // Plus button
    plusBtn = new QPushButton("+");
    plusBtn->setFixedSize(60, 60);
    plusBtn->setStyleSheet(
        "QPushButton { background: rgb(214,61,61); color: white; "
        "border-radius:6px; font-size:28px; }"
        "QPushButton:pressed { background: rgb(180,40,40); }"
        );
    ampRow->addWidget(plusBtn);
    ampRow->addStretch();

    leftCol->addLayout(ampRow);
    leftCol->addSpacing(20);

    // --- Frequencies Row ---
    QHBoxLayout *freqRow = new QHBoxLayout;

    // Frequency text
    QVBoxLayout *textCol = new QVBoxLayout;
    lblCarrier = new QLabel("Carrier Frequency\n10 kHz");
    lblCarrier->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    lblCarrier->setStyleSheet("font-size:20px; color: black;");

    lblAM = new QLabel("AM Frequency\n50 Hz");
    lblAM->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    lblAM->setStyleSheet("font-size:20px; color: black;");

    textCol->addWidget(lblCarrier);
    textCol->addSpacing(24);
    textCol->addWidget(lblAM);

    // Frequency images
    QVBoxLayout *imgCol = new QVBoxLayout;
    carrierPic = new ClickableLabel;
    carrierPic->setFixedSize(140, 80);
    carrierPic->setScaledContents(true);
    carrierPic->setStyleSheet("background: transparent;");

    amPic = new ClickableLabel;
    amPic->setFixedSize(140, 80);
    amPic->setScaledContents(true);
    amPic->setStyleSheet("background: transparent;");

    imgCol->addWidget(carrierPic, 0, Qt::AlignCenter);
    imgCol->addSpacing(10);
    imgCol->addWidget(amPic, 0, Qt::AlignCenter);

    // Add text + images
    freqRow->addLayout(textCol, 0);
    freqRow->addSpacing(24);
    freqRow->addLayout(imgCol, 0);
    freqRow->addStretch();

    leftCol->addLayout(freqRow);

    // =============================================================
    // RIGHT COLUMN: Electrode Matrix Button
    // =============================================================
    QVBoxLayout *rightCol = new QVBoxLayout;
    electrodeBtn = new QPushButton("Electrode Matrix");
    electrodeBtn->setFixedSize(300, 140);
    electrodeBtn->setStyleSheet(
        "QPushButton { background: rgb(214,61,61); color: white; "
        "border-radius:8px; font-size:32px; }"
        "QPushButton:pressed { background: rgb(180,40,40); }"
        );
    rightCol->addStretch();
    rightCol->addWidget(electrodeBtn, 0, Qt::AlignCenter);
    rightCol->addStretch();

    // Add left + right to middle row
    middleRow->addLayout(leftCol, 2);
    middleRow->addSpacing(40);
    middleRow->addLayout(rightCol, 1);

    contentLayout->addLayout(middleRow);
    contentLayout->addStretch();

    mainLayout->addWidget(content);
    setCentralWidget(central);

    // =============================================================
    // Timers (press-and-hold buttons)
    // =============================================================
    plusHoldTimer = new QTimer(this);
    minusHoldTimer = new QTimer(this);
    plusHoldTimer->setInterval(100);
    minusHoldTimer->setInterval(100);

    connect(plusHoldTimer, &QTimer::timeout, this, &MainWindow::onPlusHold);
    connect(minusHoldTimer, &QTimer::timeout, this, &MainWindow::onMinusHold);

    // =============================================================
    // Connections
    // =============================================================
    // Amplitude buttons
    connect(plusBtn, &QPushButton::clicked, this, &MainWindow::onPlusClicked);
    connect(minusBtn, &QPushButton::clicked, this, &MainWindow::onMinusClicked);
    connect(plusBtn, &QPushButton::pressed, plusHoldTimer, QOverload<>::of(&QTimer::start));
    connect(minusBtn, &QPushButton::pressed, minusHoldTimer, QOverload<>::of(&QTimer::start));
    connect(plusBtn, &QPushButton::released, this, &MainWindow::stopHold);
    connect(minusBtn, &QPushButton::released, this, &MainWindow::stopHold);

    // Electrode Matrix
    connect(electrodeBtn, &QPushButton::clicked, this, &MainWindow::onElectrodeMatrixClicked);

    // Image clicks
    connect(carrierPic, &ClickableLabel::clicked, this, &MainWindow::onCarrierClicked);
    connect(amPic, &ClickableLabel::clicked, this, &MainWindow::onAMClicked);

    // =============================================================
    // Load default images + init amplitude
    // =============================================================
    loadImages(":/Carrier_Frequency.png", ":/AM_Frequency.png");
   	
    updateAmplitudeDisplay();
}

// =============================================================
// Destructor
// =============================================================
MainWindow::~MainWindow() = default;

// =============================================================
// Image Handling
// =============================================================
void MainWindow::setImagePaths(const QString &carrierPath, const QString &amPath) {
    loadImages(carrierPath, amPath);
}

void MainWindow::loadImages(const QString &carrierPath, const QString &amPath) {
    QPixmap cpix, apix;

    if (!cpix.load(carrierPath)) {
        qWarning() << "Carrier image not found at" << carrierPath;
        carrierPic->clear();
    } else {
        carrierPic->setPixmap(cpix.scaled(carrierPic->size(), Qt::KeepAspectRatio, Qt::SmoothTransformation));
    }

    if (!apix.load(amPath)) {
        qWarning() << "AM image not found at" << amPath;
        amPic->clear();
    } else {
        amPic->setPixmap(apix.scaled(amPic->size(), Qt::KeepAspectRatio, Qt::SmoothTransformation));
    }
}

// =============================================================
// Amplitude Handling
// =============================================================
void MainWindow::updateAmplitudeDisplay() {
    amplitudeDisplay->setText(QString::number(amplitude, 'f', 2) + " V");
    emit amplitudeChanged(amplitude);
    HistoryWindow::lastAmplitude = amplitude;  // store user input persistently
}

double MainWindow::clampAmplitude(double val) {
    if (val < 1.0) return 1.0;
    if (val > 5.0) return 5.0;
    return std::round(val * 10.0) / 10.0; // round to 1 decimal place
}

// =============================================================
// Button Clicks & Hold Events
// =============================================================
void MainWindow::onPlusClicked() {
    if (amplitude < 5.0) {
        amplitude = clampAmplitude(amplitude + 0.1);
        updateAmplitudeDisplay();
    }
}

void MainWindow::onMinusClicked() {
    if (amplitude > 1.0) {
        amplitude = clampAmplitude(amplitude - 0.1);
        updateAmplitudeDisplay();
    }
}

void MainWindow::onPlusHold() {
    if (amplitude < 5.0) {
        amplitude = clampAmplitude(amplitude + 0.2);
        updateAmplitudeDisplay();
    }
}

void MainWindow::onMinusHold() {
    if (amplitude > 1.0) {
        amplitude = clampAmplitude(amplitude - 0.2);
        updateAmplitudeDisplay();
    }
}

void MainWindow::stopHold() {
    plusHoldTimer->stop();
    minusHoldTimer->stop();
}

// =============================================================
// Navigation
// =============================================================
void MainWindow::onElectrodeMatrixClicked() {
    this->hide();
    ElectrodeWindow *ew = new ElectrodeWindow();
    ew->showFullScreen();
}

// =============================================================
// Image Clicks
// =============================================================
void MainWindow::onCarrierClicked() {
    qDebug() << "Carrier image clicked";
}

void MainWindow::onAMClicked() {
    qDebug() << "AM image clicked";
}
