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
// Helpers: factory methods
// =============================================================
QPushButton* MainWindow::makeRedBtn(const QString &label) {
    auto *btn = new QPushButton(label);
    btn->setFixedSize(60, 60);
    btn->setStyleSheet(
        "QPushButton { background: rgb(214,61,61); color: white; "
        "border-radius:6px; font-size:28px; }"
        "QPushButton:pressed { background: rgb(180,40,40); }");
    return btn;
}

QLineEdit* MainWindow::makeDisplay(const QString &initText) {
    auto *d = new QLineEdit(initText);
    d->setAlignment(Qt::AlignCenter);
    d->setReadOnly(true);
    d->setFixedSize(160, 60);
    d->setStyleSheet(
        "QLineEdit { background: white; color: black; "
        "border-radius:6px; font-size:22px; font-weight:bold; }");
    return d;
}

QHBoxLayout* MainWindow::buildParamRow(QPushButton *&minusOut,
                                       QLineEdit   *&displayOut,
                                       QPushButton *&plusOut,
                                       const QString &initText)
{
    auto *row = new QHBoxLayout;
    row->addStretch();
    minusOut   = makeRedBtn("-");      row->addWidget(minusOut);
    row->addSpacing(20);
    displayOut = makeDisplay(initText); row->addWidget(displayOut);
    row->addSpacing(20);
    plusOut    = makeRedBtn("+");      row->addWidget(plusOut);
    row->addStretch();
    return row;
}

void MainWindow::wireHold(QPushButton *btn, QTimer *&holdTimer,
                          void (MainWindow::*holdSlot)(),
                          void (MainWindow::*clickSlot)())
{
    holdTimer = new QTimer(this);
    holdTimer->setInterval(100);
    connect(holdTimer, &QTimer::timeout, this, holdSlot);
    connect(btn, &QPushButton::clicked,  this, clickSlot);
    connect(btn, &QPushButton::pressed,  holdTimer, QOverload<>::of(&QTimer::start));
    connect(btn, &QPushButton::released, this, &MainWindow::stopAllHolds);
}

// =============================================================
// MainWindow constructor
// =============================================================
MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent),
    amplitude(1.0), rampUp(1.0), coast(1.0), rampDown(-1.0)
{
    setWindowTitle("FES_2025");
    this->setCursor(Qt::BlankCursor);
    amplitude = HistoryWindow::lastAmplitude;

    // ── Central widget ──────────────────────────────────────────────────────
    QWidget *central = new QWidget(this);
    QVBoxLayout *mainLayout = new QVBoxLayout(central);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);
    central->setStyleSheet("background: white; border: none;");
    setStyleSheet("QMainWindow { border: none; }");

    // ── Top Bar ─────────────────────────────────────────────────────────────
    topBar = new QWidget(central);
    topBar->setFixedHeight(60);
    topBar->setStyleSheet("background-color: rgb(141,25,36);");
    QHBoxLayout *topBarLayout = new QHBoxLayout(topBar);
    topBarLayout->setContentsMargins(20, 0, 20, 0);
    topBarLayout->addStretch();
    titleLabel = new QLabel("Stimulation Settings", topBar);
    titleLabel->setStyleSheet(
        "color: white; font-size:20px; font-weight:600;"
        "background: transparent; border: none; font-weight: bold;");
    titleLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    topBarLayout->addWidget(titleLabel);
    mainLayout->addWidget(topBar);

    // ── Content Area ────────────────────────────────────────────────────────
    QWidget *content = new QWidget(central);
    content->setStyleSheet("background-color: white;");
    QVBoxLayout *contentLayout = new QVBoxLayout(content);
    contentLayout->setContentsMargins(40, 20, 40, 20);
    contentLayout->addSpacing(10);

    // Middle row: LEFT params | RIGHT electrode + FES signal
    QHBoxLayout *middleRow = new QHBoxLayout;

    // ── LEFT COLUMN ─────────────────────────────────────────────────────────
    QVBoxLayout *leftCol = new QVBoxLayout;
    leftCol->setSpacing(16);

    // ---- 1. Set Maximum Amplitude (1.0 V – 5.0 V) -------------------------
    auto *ampHeading = new QLabel("Set Maximum Amplitude");
    ampHeading->setStyleSheet("font-size:20px; font-weight:600; color:#222;");
    leftCol->addWidget(ampHeading);
    auto *ampRow = buildParamRow(ampMinusBtn, amplitudeDisplay, ampPlusBtn, "1.00 V");
    leftCol->addLayout(ampRow);
    leftCol->addSpacing(8);

    // ---- 2. Set Ramp Up Rate (0.1 – 3.0 V/s) ------------------------------
    auto *rampUpHeading = new QLabel("Set Ramp Up Rate");
    rampUpHeading->setStyleSheet("font-size:20px; font-weight:600; color:#222;");
    leftCol->addWidget(rampUpHeading);
    auto *rampUpRow = buildParamRow(rampUpMinusBtn, rampUpDisplay, rampUpPlusBtn, "1.00 V/s");
    leftCol->addLayout(rampUpRow);
    leftCol->addSpacing(8);

    // ---- 3. Set Coast Duration (0.0 – 10.0 s) -----------------------------
    auto *coastHeading = new QLabel("Set Coast Duration");
    coastHeading->setStyleSheet("font-size:20px; font-weight:600; color:#222;");
    leftCol->addWidget(coastHeading);
    auto *coastRow = buildParamRow(coastMinusBtn, coastDisplay, coastPlusBtn, "1.00 s");
    leftCol->addLayout(coastRow);
    leftCol->addSpacing(8);

    // ---- 4. Set Ramp Down Rate (-0.1 – -3.0 V/s) --------------------------
    auto *rampDownHeading = new QLabel("Set Ramp Down Rate");
    rampDownHeading->setStyleSheet("font-size:20px; font-weight:600; color:#222;");
    leftCol->addWidget(rampDownHeading);
    auto *rampDownRow = buildParamRow(rampDownMinusBtn, rampDownDisplay, rampDownPlusBtn, "-1.00 V/s");
    leftCol->addLayout(rampDownRow);

    leftCol->addStretch();

    // ── RIGHT COLUMN ────────────────────────────────────────────────────────
    QVBoxLayout *rightCol = new QVBoxLayout;
    rightCol->setSpacing(16);

    // Electrode Matrix button
    electrodeBtn = new QPushButton("Electrode Matrix");
    electrodeBtn->setFixedSize(300, 140);
    electrodeBtn->setStyleSheet(
        "QPushButton { background: rgb(214,61,61); color: white; "
        "border-radius:8px; font-size:32px; }"
        "QPushButton:pressed { background: rgb(180,40,40); }");
    rightCol->addWidget(electrodeBtn, 0, Qt::AlignCenter);
    rightCol->addSpacing(12);

    // FES Signal label
    auto *fesLabel = new QLabel("FES Signal");
    fesLabel->setStyleSheet("font-size:16px; font-weight:600; color:#222;");
    rightCol->addWidget(fesLabel, 0, Qt::AlignLeft);

    // FES Signal image placeholder (ClickableLabel – same pattern as carrier/AM pics)
    carrierPic = new ClickableLabel;
    carrierPic->setFixedSize(280, 130);
    carrierPic->setScaledContents(true);
    carrierPic->setStyleSheet(
        "background: #f0f0f0; border:1px solid #ccc; border-radius:6px;");
    rightCol->addWidget(carrierPic, 0, Qt::AlignCenter);

    // (amPic kept for API compatibility – hidden, zero-size)
    amPic = new ClickableLabel;
    amPic->setFixedSize(0, 0);
    amPic->hide();
    rightCol->addWidget(amPic);

    // Invisible labels kept so getCarrierFreq() / getBurstFreq() still work
    lblCarrier = new QLabel("Carrier Frequency\n10 kHz"); lblCarrier->hide();
    lblAM      = new QLabel("AM Frequency\n50 Hz");       lblAM->hide();
    rightCol->addWidget(lblCarrier);
    rightCol->addWidget(lblAM);

    rightCol->addStretch();

    // ── Assemble middle row ─────────────────────────────────────────────────
    middleRow->addLayout(leftCol, 2);
    middleRow->addSpacing(40);
    middleRow->addLayout(rightCol, 1);

    contentLayout->addLayout(middleRow);
    contentLayout->addStretch();
    mainLayout->addWidget(content);
    setCentralWidget(central);

    // ── Wire hold timers ────────────────────────────────────────────────────
    wireHold(ampPlusBtn,      ampPlusHoldTimer,      &MainWindow::onAmpPlusHold,      &MainWindow::onAmpPlusClicked);
    wireHold(ampMinusBtn,     ampMinusHoldTimer,     &MainWindow::onAmpMinusHold,     &MainWindow::onAmpMinusClicked);
    wireHold(rampUpPlusBtn,   rampUpPlusHoldTimer,   &MainWindow::onRampUpPlusHold,   &MainWindow::onRampUpPlusClicked);
    wireHold(rampUpMinusBtn,  rampUpMinusHoldTimer,  &MainWindow::onRampUpMinusHold,  &MainWindow::onRampUpMinusClicked);
    wireHold(coastPlusBtn,    coastPlusHoldTimer,    &MainWindow::onCoastPlusHold,    &MainWindow::onCoastPlusClicked);
    wireHold(coastMinusBtn,   coastMinusHoldTimer,   &MainWindow::onCoastMinusHold,   &MainWindow::onCoastMinusClicked);
    wireHold(rampDownPlusBtn, rampDownPlusHoldTimer, &MainWindow::onRampDownPlusHold, &MainWindow::onRampDownPlusClicked);
    wireHold(rampDownMinusBtn,rampDownMinusHoldTimer,&MainWindow::onRampDownMinusHold,&MainWindow::onRampDownMinusClicked);

    // Legacy aliases (plusBtn / minusBtn) still point to amp buttons
    plusBtn        = ampPlusBtn;
    minusBtn       = ampMinusBtn;
    plusHoldTimer  = ampPlusHoldTimer;
    minusHoldTimer = ampMinusHoldTimer;

    // Electrode Matrix
    connect(electrodeBtn, &QPushButton::clicked, this, &MainWindow::onElectrodeMatrixClicked);
    connect(carrierPic,   &ClickableLabel::clicked, this, &MainWindow::onCarrierClicked);
    connect(amPic,        &ClickableLabel::clicked, this, &MainWindow::onAMClicked);

    // Load default images
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
// Image Handling
// =============================================================
void MainWindow::setImagePaths(const QString &carrierPath, const QString &amPath) {
    loadImages(carrierPath, amPath);
}

void MainWindow::loadImages(const QString &carrierPath, const QString &amPath) {
    QPixmap cpix, apix;
    if (!cpix.load(carrierPath)) {
        qWarning() << "Carrier/FES image not found at" << carrierPath;
        carrierPic->clear();
    } else {
        carrierPic->setPixmap(
            cpix.scaled(carrierPic->size(), Qt::KeepAspectRatio, Qt::SmoothTransformation));
    }
    if (!apix.load(amPath)) {
        qWarning() << "AM image not found at" << amPath;
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
void MainWindow::updateRampUpDisplay() {
    rampUpDisplay->setText(QString::number(rampUp, 'f', 2) + " V/s");
}
void MainWindow::updateCoastDisplay() {
    coastDisplay->setText(QString::number(coast, 'f', 2) + " s");
}
void MainWindow::updateRampDownDisplay() {
    rampDownDisplay->setText(QString::number(rampDown, 'f', 2) + " V/s");
}

// =============================================================
// Clamp helpers
// =============================================================
double MainWindow::clampAmplitude(double v) {
    v = std::max(1.0, std::min(5.0, v));
    return std::round(v * 10.0) / 10.0;
}
double MainWindow::clampRampUp(double v) {
    v = std::max(0.1, std::min(3.0, v));
    return std::round(v * 10.0) / 10.0;
}
double MainWindow::clampCoast(double v) {
    v = std::max(0.0, std::min(10.0, v));
    return std::round(v * 10.0) / 10.0;
}
double MainWindow::clampRampDown(double v) {
    // rampDown is always negative: -0.1 (closest to 0) … -3.0 (most negative)
    v = std::max(-3.0, std::min(-0.1, v));
    return std::round(v * 10.0) / 10.0;
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
// (+ button makes value less negative, i.e. rampDown += 0.1 toward -0.1)
// (- button makes value more negative, i.e. rampDown -= 0.1 toward -3.0)
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
// Image Clicks
// =============================================================
void MainWindow::onCarrierClicked() { qDebug() << "FES signal image clicked"; }
void MainWindow::onAMClicked()      { qDebug() << "AM image clicked"; }