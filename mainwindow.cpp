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
#include <cmath>

extern double g_maxAmplitude;

// =============================================================
// ClickableLabel
// =============================================================
void ClickableLabel::mousePressEvent(QMouseEvent *event) {
    emit clicked();
    QLabel::mousePressEvent(event);
}

// =============================================================
// Factory: red square button (36x36, fits 800x480)
// =============================================================
QPushButton* MainWindow::makeRedBtn(const QString &label) {
    auto *btn = new QPushButton(label);
    btn->setFixedSize(36, 36);
    btn->setStyleSheet(
        "QPushButton { background: rgb(214,61,61); color: white; "
        "border-radius:5px; font-size:18px; font-weight:bold; }"
        "QPushButton:pressed { background: rgb(180,40,40); }");
    return btn;
}

// =============================================================
// Factory: read-only value display (120x36, fits 800x480)
// =============================================================
QLineEdit* MainWindow::makeDisplay(const QString &initText) {
    auto *d = new QLineEdit(initText);
    d->setAlignment(Qt::AlignCenter);
    d->setReadOnly(true);
    d->setFixedSize(120, 36);
    d->setStyleSheet(
        "QLineEdit { background: white; color: black; "
        "border: 1px solid #ccc; border-radius:4px; "
        "font-size:13px; font-weight:bold; }");
    return d;
}

// =============================================================
// Factory: [−] [display] [+] row
// =============================================================
QHBoxLayout* MainWindow::buildParamRow(QPushButton *&minusOut,
                                       QLineEdit   *&displayOut,
                                       QPushButton *&plusOut,
                                       const QString &initText)
{
    auto *row = new QHBoxLayout;
    row->setSpacing(8);
    row->setContentsMargins(0, 0, 0, 0);
    minusOut   = makeRedBtn("-");        row->addWidget(minusOut);
    displayOut = makeDisplay(initText);  row->addWidget(displayOut);
    plusOut    = makeRedBtn("+");        row->addWidget(plusOut);
    row->addStretch();
    return row;
}

// =============================================================
// Wire a button to a hold timer + click slot
// =============================================================
void MainWindow::wireHold(QPushButton *btn, QTimer *&holdTimer,
                          void (MainWindow::*holdSlot)(),
                          void (MainWindow::*clickSlot)())
{
    holdTimer = new QTimer(this);
    holdTimer->setInterval(100);
    connect(holdTimer, &QTimer::timeout,    this, holdSlot);
    connect(btn, &QPushButton::clicked,     this, clickSlot);
    connect(btn, &QPushButton::pressed,     holdTimer, QOverload<>::of(&QTimer::start));
    connect(btn, &QPushButton::released,    this, &MainWindow::stopAllHolds);
}

// =============================================================
// Constructor
// =============================================================
MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent),
      amplitude(1.0), rampUp(1.0), coast(1.0), rampDown(-1.0)
{
    setWindowTitle("FES_2025");
    this->setCursor(Qt::BlankCursor);
    amplitude = HistoryWindow::lastAmplitude;

    // ── Central widget ────────────────────────────────────────────────────
    QWidget *central = new QWidget(this);
    QVBoxLayout *mainLayout = new QVBoxLayout(central);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);
    central->setStyleSheet("background: white; border: none;");
    setStyleSheet("QMainWindow { border: none; }");

    // ── Top Bar (40 px — tight for 800x480) ──────────────────────────────
    topBar = new QWidget(central);
    topBar->setFixedHeight(40);
    topBar->setStyleSheet("background-color: rgb(141,25,36);");
    QHBoxLayout *topBarLayout = new QHBoxLayout(topBar);
    topBarLayout->setContentsMargins(12, 0, 12, 0);
    topBarLayout->addStretch();
    titleLabel = new QLabel("Stimulation Settings", topBar);
    titleLabel->setStyleSheet(
        "color: white; font-size:15px; font-weight:600;"
        "background: transparent; border: none;");
    titleLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    topBarLayout->addWidget(titleLabel);
    mainLayout->addWidget(topBar);

    // ── Content area ──────────────────────────────────────────────────────
    // Available height = 480 - 40 (topbar) = 440 px
    QWidget *content = new QWidget(central);
    content->setStyleSheet("background-color: white;");
    QVBoxLayout *contentLayout = new QVBoxLayout(content);
    contentLayout->setContentsMargins(14, 8, 14, 8);
    contentLayout->setSpacing(0);

    QHBoxLayout *middleRow = new QHBoxLayout;
    middleRow->setSpacing(16);

    // ── LEFT COLUMN: 4 parameter controls ────────────────────────────────
    QVBoxLayout *leftCol = new QVBoxLayout;
    leftCol->setSpacing(0);
    leftCol->setContentsMargins(0, 0, 0, 0);

    // Lambda: add labelled +/- row
    auto addParam = [&](QVBoxLayout *col,
                        const QString &title,
                        QPushButton *&minusBtn,
                        QLineEdit   *&display,
                        QPushButton *&plusBtn,
                        const QString &initText)
    {
        auto *heading = new QLabel(title);
        heading->setStyleSheet("font-size:12px; font-weight:700; color:#222;");
        heading->setContentsMargins(0, 6, 0, 2);
        col->addWidget(heading);
        col->addLayout(buildParamRow(minusBtn, display, plusBtn, initText));
    };

    // 1. Amplitude   1.0 V – 5.0 V
    addParam(leftCol, "Set Maximum Amplitude",
             ampMinusBtn, amplitudeDisplay, ampPlusBtn, "1.00 V");

    // 2. Ramp Up     0.1 – 3.0 V/s
    addParam(leftCol, "Set Ramp Up Rate",
             rampUpMinusBtn, rampUpDisplay, rampUpPlusBtn, "1.00 V/s");

    // 3. Coast       0.0 – 10.0 s
    addParam(leftCol, "Set Coast Duration",
             coastMinusBtn, coastDisplay, coastPlusBtn, "1.00 s");

    // 4. Ramp Down   -0.1 – -3.0 V/s
    addParam(leftCol, "Set Ramp Down Rate",
             rampDownMinusBtn, rampDownDisplay, rampDownPlusBtn, "-1.00 V/s");

    leftCol->addStretch();

    // ── RIGHT COLUMN: Electrode Matrix + FES Signal image ─────────────────
    QVBoxLayout *rightCol = new QVBoxLayout;
    rightCol->setSpacing(6);
    rightCol->setContentsMargins(0, 0, 0, 0);

    // Electrode Matrix button
    electrodeBtn = new QPushButton("Electrode Matrix");
    electrodeBtn->setFixedSize(190, 80);
    electrodeBtn->setStyleSheet(
        "QPushButton { background: rgb(214,61,61); color: white; "
        "border-radius:6px; font-size:18px; font-weight:bold; }"
        "QPushButton:pressed { background: rgb(180,40,40); }");
    rightCol->addWidget(electrodeBtn, 0, Qt::AlignCenter);

    // FES Signal label
    auto *fesLabel = new QLabel("FES Signal");
    fesLabel->setStyleSheet("font-size:11px; font-weight:600; color:#222;");
    rightCol->addWidget(fesLabel, 0, Qt::AlignLeft);

    // FES Signal image placeholder (ClickableLabel)
    carrierPic = new ClickableLabel;
    carrierPic->setFixedSize(190, 200);
    carrierPic->setScaledContents(true);
    carrierPic->setStyleSheet(
        "background: #f0f0f0; border:1px solid #ccc; border-radius:4px;");
    rightCol->addWidget(carrierPic, 0, Qt::AlignCenter);

    // amPic kept for API compatibility (hidden, zero-size)
    amPic = new ClickableLabel;
    amPic->setFixedSize(0, 0);
    amPic->hide();
    rightCol->addWidget(amPic);

    // Hidden labels so getCarrierFreq() / getBurstFreq() still compile
    lblCarrier = new QLabel("Carrier Frequency\n10 kHz"); lblCarrier->hide();
    lblAM      = new QLabel("AM Frequency\n50 Hz");       lblAM->hide();
    rightCol->addWidget(lblCarrier);
    rightCol->addWidget(lblAM);

    rightCol->addStretch();

    // ── Assemble layout ───────────────────────────────────────────────────
    middleRow->addLayout(leftCol, 3);
    middleRow->addLayout(rightCol, 2);

    contentLayout->addLayout(middleRow);
    contentLayout->addStretch();
    mainLayout->addWidget(content);
    setCentralWidget(central);

    // ── Wire hold timers ──────────────────────────────────────────────────
    wireHold(ampPlusBtn,       ampPlusHoldTimer,       &MainWindow::onAmpPlusHold,      &MainWindow::onAmpPlusClicked);
    wireHold(ampMinusBtn,      ampMinusHoldTimer,      &MainWindow::onAmpMinusHold,     &MainWindow::onAmpMinusClicked);
    wireHold(rampUpPlusBtn,    rampUpPlusHoldTimer,    &MainWindow::onRampUpPlusHold,   &MainWindow::onRampUpPlusClicked);
    wireHold(rampUpMinusBtn,   rampUpMinusHoldTimer,   &MainWindow::onRampUpMinusHold,  &MainWindow::onRampUpMinusClicked);
    wireHold(coastPlusBtn,     coastPlusHoldTimer,     &MainWindow::onCoastPlusHold,    &MainWindow::onCoastPlusClicked);
    wireHold(coastMinusBtn,    coastMinusHoldTimer,     &MainWindow::onCoastMinusHold,   &MainWindow::onCoastMinusClicked);
    wireHold(rampDownPlusBtn,  rampDownPlusHoldTimer,  &MainWindow::onRampDownPlusHold, &MainWindow::onRampDownPlusClicked);
    wireHold(rampDownMinusBtn, rampDownMinusHoldTimer, &MainWindow::onRampDownMinusHold,&MainWindow::onRampDownMinusClicked);

    // Legacy aliases → amplitude buttons
    plusBtn        = ampPlusBtn;
    minusBtn       = ampMinusBtn;
    plusHoldTimer  = ampPlusHoldTimer;
    minusHoldTimer = ampMinusHoldTimer;

    // Navigation & image signals
    connect(electrodeBtn, &QPushButton::clicked,     this, &MainWindow::onElectrodeMatrixClicked);
    connect(carrierPic,   &ClickableLabel::clicked,  this, &MainWindow::onCarrierClicked);
    connect(amPic,        &ClickableLabel::clicked,  this, &MainWindow::onAMClicked);

    // Load FES signal image & initialise all displays
    loadImages("C:/Users/Admin/Desktop/School/Thesis/Projects/Codes/RPi Master/FES_GUIv2026w16d15/resources/FEScompleteSignal.png", "");
    updateAmplitudeDisplay();
    updateRampUpDisplay();
    updateCoastDisplay();
    updateRampDownDisplay();
}

// =============================================================
// Destructor
// =============================================================
MainWindow::~MainWindow() = default;

// =============================================================
// Image handling
// =============================================================
void MainWindow::setImagePaths(const QString &carrierPath, const QString &amPath) {
    loadImages(carrierPath, amPath);
}

void MainWindow::loadImages(const QString &carrierPath, const QString &amPath) {
    QPixmap cpix, apix;

    if (!cpix.load(carrierPath)) {
        qWarning() << "FES image not found at" << carrierPath;
        carrierPic->clear();
    } else {
        carrierPic->setPixmap(
            cpix.scaled(carrierPic->size(), Qt::KeepAspectRatio, Qt::SmoothTransformation));
    }

    if (!apix.load(amPath)) {
        // amPath may be empty intentionally — no warning needed
        amPic->clear();
    } else {
        amPic->setPixmap(
            apix.scaled(amPic->size(), Qt::KeepAspectRatio, Qt::SmoothTransformation));
    }
}

// =============================================================
// Display updaters
// =============================================================
void MainWindow::updateAmplitudeDisplay() {
    amplitudeDisplay->setText(QString::number(amplitude, 'f', 2) + " V");
    emit amplitudeChanged(amplitude);
    HistoryWindow::lastAmplitude = amplitude;
}
void MainWindow::updateRampUpDisplay()   { rampUpDisplay->setText(QString::number(rampUp,   'f', 2) + " V/s"); }
void MainWindow::updateCoastDisplay()    { coastDisplay->setText(QString::number(coast,      'f', 2) + " s");   }
void MainWindow::updateRampDownDisplay() { rampDownDisplay->setText(QString::number(rampDown,'f', 2) + " V/s"); }

// =============================================================
// Clamp helpers
// =============================================================
double MainWindow::clampAmplitude(double v) {
    return std::round(std::max(1.0, std::min(5.0,  v)) * 10.0) / 10.0;
}
double MainWindow::clampRampUp(double v) {
    return std::round(std::max(0.1, std::min(3.0,  v)) * 10.0) / 10.0;
}
double MainWindow::clampCoast(double v) {
    return std::round(std::max(0.0, std::min(10.0, v)) * 10.0) / 10.0;
}
double MainWindow::clampRampDown(double v) {
    // always negative: -0.1 (least steep) … -3.0 (steepest)
    return std::round(std::max(-3.0, std::min(-0.1, v)) * 10.0) / 10.0;
}

// =============================================================
// AMPLITUDE slots
// =============================================================
void MainWindow::onAmpPlusClicked()  { amplitude = clampAmplitude(amplitude + 0.1); updateAmplitudeDisplay(); }
void MainWindow::onAmpMinusClicked() { amplitude = clampAmplitude(amplitude - 0.1); updateAmplitudeDisplay(); }
void MainWindow::onAmpPlusHold()     { amplitude = clampAmplitude(amplitude + 0.2); updateAmplitudeDisplay(); }
void MainWindow::onAmpMinusHold()    { amplitude = clampAmplitude(amplitude - 0.2); updateAmplitudeDisplay(); }

// =============================================================
// RAMP UP slots
// =============================================================
void MainWindow::onRampUpPlusClicked()  { rampUp = clampRampUp(rampUp + 0.1); updateRampUpDisplay(); }
void MainWindow::onRampUpMinusClicked() { rampUp = clampRampUp(rampUp - 0.1); updateRampUpDisplay(); }
void MainWindow::onRampUpPlusHold()     { rampUp = clampRampUp(rampUp + 0.2); updateRampUpDisplay(); }
void MainWindow::onRampUpMinusHold()    { rampUp = clampRampUp(rampUp - 0.2); updateRampUpDisplay(); }

// =============================================================
// COAST slots
// =============================================================
void MainWindow::onCoastPlusClicked()  { coast = clampCoast(coast + 0.1); updateCoastDisplay(); }
void MainWindow::onCoastMinusClicked() { coast = clampCoast(coast - 0.1); updateCoastDisplay(); }
void MainWindow::onCoastPlusHold()     { coast = clampCoast(coast + 0.2); updateCoastDisplay(); }
void MainWindow::onCoastMinusHold()    { coast = clampCoast(coast - 0.2); updateCoastDisplay(); }

// =============================================================
// RAMP DOWN slots
// (+) moves toward -0.1  |  (-) moves toward -3.0
// =============================================================
void MainWindow::onRampDownPlusClicked()  { rampDown = clampRampDown(rampDown + 0.1); updateRampDownDisplay(); }
void MainWindow::onRampDownMinusClicked() { rampDown = clampRampDown(rampDown - 0.1); updateRampDownDisplay(); }
void MainWindow::onRampDownPlusHold()     { rampDown = clampRampDown(rampDown + 0.2); updateRampDownDisplay(); }
void MainWindow::onRampDownMinusHold()    { rampDown = clampRampDown(rampDown - 0.2); updateRampDownDisplay(); }

// =============================================================
// Stop ALL hold timers
// =============================================================
void MainWindow::stopAllHolds() {
    if (ampPlusHoldTimer)       ampPlusHoldTimer->stop();
    if (ampMinusHoldTimer)      ampMinusHoldTimer->stop();
    if (rampUpPlusHoldTimer)    rampUpPlusHoldTimer->stop();
    if (rampUpMinusHoldTimer)   rampUpMinusHoldTimer->stop();
    if (coastPlusHoldTimer)     coastPlusHoldTimer->stop();
    if (coastMinusHoldTimer)    coastMinusHoldTimer->stop();
    if (rampDownPlusHoldTimer)  rampDownPlusHoldTimer->stop();
    if (rampDownMinusHoldTimer) rampDownMinusHoldTimer->stop();
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
// Image clicks
// =============================================================
void MainWindow::onCarrierClicked() { qDebug() << "FES signal image clicked"; }
void MainWindow::onAMClicked()      { qDebug() << "AM image clicked"; }
