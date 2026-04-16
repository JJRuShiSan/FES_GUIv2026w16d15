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
// Factory: red square button
// Sized to match the design photo on 800x480
// =============================================================
QPushButton* MainWindow::makeRedBtn(const QString &label) {
    auto *btn = new QPushButton(label);
    btn->setFixedSize(52, 52);
    btn->setStyleSheet(
        "QPushButton {"
        "  background: rgb(190,30,45);"
        "  color: white;"
        "  border-radius: 6px;"
        "  font-size: 26px;"
        "  font-weight: bold;"
        "}"
        "QPushButton:pressed { background: rgb(150,20,30); }");
    return btn;
}

// =============================================================
// Factory: plain value label (no box, just centred text)
// Matches the design: plain white background, bold black text
// =============================================================
QLineEdit* MainWindow::makeDisplay(const QString &initText) {
    auto *d = new QLineEdit(initText);
    d->setAlignment(Qt::AlignCenter);
    d->setReadOnly(true);
    d->setFixedSize(140, 52);
    d->setFrame(false);                      // NO border — matches design
    d->setStyleSheet(
        "QLineEdit {"
        "  background: transparent;"         // blends into white bg
        "  color: black;"
        "  font-size: 18px;"
        "  font-weight: normal;"
        "  border: none;"
        "}");
    return d;
}

// =============================================================
// Factory: [−]  value  [+]  row  (left-aligned, no stretch padding)
// =============================================================
QHBoxLayout* MainWindow::buildParamRow(QPushButton *&minusOut,
                                       QLineEdit   *&displayOut,
                                       QPushButton *&plusOut,
                                       const QString &initText)
{
    auto *row = new QHBoxLayout;
    row->setSpacing(0);
    row->setContentsMargins(0, 0, 0, 0);

    minusOut   = makeRedBtn("-");
    displayOut = makeDisplay(initText);
    plusOut    = makeRedBtn("+");

    row->addWidget(minusOut);
    row->addWidget(displayOut);
    row->addWidget(plusOut);
    row->addStretch();          // push everything to the left
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
    connect(holdTimer, &QTimer::timeout,  this,      holdSlot);
    connect(btn, &QPushButton::clicked,   this,      clickSlot);
    connect(btn, &QPushButton::pressed,   holdTimer, QOverload<>::of(&QTimer::start));
    connect(btn, &QPushButton::released,  this,      &MainWindow::stopAllHolds);
}

// =============================================================
// Constructor
// Screen: 800 x 480 px
//
// Pixel budget (height):
//   Top bar        :  48 px
//   Top padding    :  12 px
//   4 × (label 20 + row 52 + gap 14) = 4 × 86 = 344 px
//   Bottom padding :  12 px
//   Stretch        :  ~12 px
//   TOTAL          :  428 px  ✓ fits in 480
// =============================================================
MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent),
      amplitude(1.0), rampUp(1.0), coast(1.0), rampDown(-1.0)
{
    setWindowTitle("FES_2025");
    this->setCursor(Qt::BlankCursor);
    amplitude = HistoryWindow::lastAmplitude;

    // ── Root ─────────────────────────────────────────────────────────────
    QWidget *central = new QWidget(this);
    QVBoxLayout *rootLayout = new QVBoxLayout(central);
    rootLayout->setContentsMargins(0, 0, 0, 0);
    rootLayout->setSpacing(0);
    central->setStyleSheet("background: white;");
    setStyleSheet("QMainWindow { border: none; }");

    // ── Top bar  48 px — dark maroon, "Stimulation Settings" right ───────
    topBar = new QWidget(central);
    topBar->setFixedHeight(48);
    topBar->setStyleSheet("background-color: rgb(110, 18, 28);");

    QHBoxLayout *topBarLayout = new QHBoxLayout(topBar);
    topBarLayout->setContentsMargins(20, 0, 20, 0);
    topBarLayout->addStretch();

    titleLabel = new QLabel("Stimulation Settings", topBar);
    titleLabel->setStyleSheet(
        "color: white;"
        "font-size: 18px;"
        "font-weight: 700;"
        "background: transparent;"
        "border: none;");
    titleLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    topBarLayout->addWidget(titleLabel);

    rootLayout->addWidget(topBar);

    // ── Content area ──────────────────────────────────────────────────────
    QWidget *content = new QWidget(central);
    content->setStyleSheet("background: white;");
    QHBoxLayout *contentLayout = new QHBoxLayout(content);
    // Left margin 50, top 12, right 20, bottom 12 — mirrors the design photo
    contentLayout->setContentsMargins(50, 12, 20, 12);
    contentLayout->setSpacing(0);

    // ── LEFT COLUMN: 4 parameter blocks ───────────────────────────────────
    QVBoxLayout *leftCol = new QVBoxLayout;
    leftCol->setSpacing(0);
    leftCol->setContentsMargins(0, 0, 0, 0);

    // Helper lambda — adds one labelled +/- block
    auto addParam = [&](const QString &title,
                        QPushButton *&minusBtn,
                        QLineEdit   *&display,
                        QPushButton *&plusBtn,
                        const QString &initText)
    {
        // Bold label above the row
        QLabel *lbl = new QLabel(title);
        lbl->setFixedHeight(24);
        lbl->setStyleSheet(
            "font-size: 15px;"
            "font-weight: 800;"
            "color: #111;"
            "background: transparent;");
        leftCol->addWidget(lbl);

        // [−]  value  [+]
        leftCol->addLayout(buildParamRow(minusBtn, display, plusBtn, initText));

        // Gap between parameters (matches design spacing)
        QWidget *gap = new QWidget;
        gap->setFixedHeight(14);
        leftCol->addWidget(gap);
    };

    addParam("Set Maximum Amplitude",
             ampMinusBtn, amplitudeDisplay, ampPlusBtn,    "1.00 V");
    addParam("Set Ramp Up Rate",
             rampUpMinusBtn, rampUpDisplay, rampUpPlusBtn, "1.00 V/s");
    addParam("Set Coast Duration",
             coastMinusBtn, coastDisplay, coastPlusBtn,    "1.00 s");
    addParam("Set Ramp Down Rate",
             rampDownMinusBtn, rampDownDisplay, rampDownPlusBtn, "1.00 V/s");

    leftCol->addStretch();

    // ── RIGHT COLUMN: Electrode Matrix button + FES Signal image ──────────
    QVBoxLayout *rightCol = new QVBoxLayout;
    rightCol->setSpacing(0);
    rightCol->setContentsMargins(30, 0, 0, 0);

    // Push content down so Electrode Matrix aligns with ~2nd row in left col
    rightCol->addSpacing(8);

    // Electrode Matrix button — large red rounded rect, centred text
    electrodeBtn = new QPushButton("Electrode Matrix");
    electrodeBtn->setFixedSize(260, 110);
    electrodeBtn->setStyleSheet(
        "QPushButton {"
        "  background: rgb(190,30,45);"
        "  color: white;"
        "  border-radius: 10px;"
        "  font-size: 22px;"
        "  font-weight: 600;"
        "}"
        "QPushButton:pressed { background: rgb(150,20,30); }");
    rightCol->addWidget(electrodeBtn, 0, Qt::AlignLeft);

    rightCol->addSpacing(12);

    // "FES Signal" label — bold, left-aligned
    QLabel *fesLabel = new QLabel("FES Signal");
    fesLabel->setFixedHeight(22);
    fesLabel->setStyleSheet(
        "font-size: 15px;"
        "font-weight: 800;"
        "color: #111;"
        "background: transparent;");
    rightCol->addWidget(fesLabel, 0, Qt::AlignLeft);

    rightCol->addSpacing(4);

    // FES signal image (ClickableLabel placeholder)
    carrierPic = new ClickableLabel;
    carrierPic->setFixedSize(260, 145);
    carrierPic->setScaledContents(true);
    carrierPic->setStyleSheet(
        "background: #f5f5f5;"
        "border: none;");
    rightCol->addWidget(carrierPic, 0, Qt::AlignLeft);

    rightCol->addStretch();

    // Hidden compat widgets
    amPic = new ClickableLabel;
    amPic->setFixedSize(0, 0);
    amPic->hide();
    rightCol->addWidget(amPic);

    lblCarrier = new QLabel("Carrier Frequency\n10 kHz"); lblCarrier->hide();
    lblAM      = new QLabel("AM Frequency\n50 Hz");       lblAM->hide();
    rightCol->addWidget(lblCarrier);
    rightCol->addWidget(lblAM);

    // ── Assemble content ──────────────────────────────────────────────────
    contentLayout->addLayout(leftCol,  1);    // left takes remaining space
    contentLayout->addLayout(rightCol, 0);    // right uses fixed-size children

    rootLayout->addWidget(content, 1);
    setCentralWidget(central);

    // ── Wire hold timers ──────────────────────────────────────────────────
    wireHold(ampPlusBtn,       ampPlusHoldTimer,       &MainWindow::onAmpPlusHold,       &MainWindow::onAmpPlusClicked);
    wireHold(ampMinusBtn,      ampMinusHoldTimer,      &MainWindow::onAmpMinusHold,      &MainWindow::onAmpMinusClicked);
    wireHold(rampUpPlusBtn,    rampUpPlusHoldTimer,    &MainWindow::onRampUpPlusHold,    &MainWindow::onRampUpPlusClicked);
    wireHold(rampUpMinusBtn,   rampUpMinusHoldTimer,   &MainWindow::onRampUpMinusHold,   &MainWindow::onRampUpMinusClicked);
    wireHold(coastPlusBtn,     coastPlusHoldTimer,     &MainWindow::onCoastPlusHold,     &MainWindow::onCoastPlusClicked);
    wireHold(coastMinusBtn,    coastMinusHoldTimer,    &MainWindow::onCoastMinusHold,    &MainWindow::onCoastMinusClicked);
    wireHold(rampDownPlusBtn,  rampDownPlusHoldTimer,  &MainWindow::onRampDownPlusHold,  &MainWindow::onRampDownPlusClicked);
    wireHold(rampDownMinusBtn, rampDownMinusHoldTimer, &MainWindow::onRampDownMinusHold, &MainWindow::onRampDownMinusClicked);

    // Legacy aliases
    plusBtn        = ampPlusBtn;
    minusBtn       = ampMinusBtn;
    plusHoldTimer  = ampPlusHoldTimer;
    minusHoldTimer = ampMinusHoldTimer;

    // Signals
    connect(electrodeBtn, &QPushButton::clicked,    this, &MainWindow::onElectrodeMatrixClicked);
    connect(carrierPic,   &ClickableLabel::clicked, this, &MainWindow::onCarrierClicked);
    connect(amPic,        &ClickableLabel::clicked, this, &MainWindow::onAMClicked);

    // Load FES signal image & init displays
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
    QPixmap cpix;
    if (!cpix.load(carrierPath)) {
        qWarning() << "FES image not found:" << carrierPath;
        carrierPic->clear();
    } else {
        carrierPic->setPixmap(
            cpix.scaled(carrierPic->size(), Qt::KeepAspectRatio, Qt::SmoothTransformation));
    }
    QPixmap apix;
    if (apix.load(amPath))
        amPic->setPixmap(
            apix.scaled(amPic->size(), Qt::KeepAspectRatio, Qt::SmoothTransformation));
}

// =============================================================
// Display updaters
// =============================================================
void MainWindow::updateAmplitudeDisplay() {
    amplitudeDisplay->setText(QString::number(amplitude, 'f', 2) + " V");
    emit amplitudeChanged(amplitude);
    HistoryWindow::lastAmplitude = amplitude;
}
void MainWindow::updateRampUpDisplay()   { rampUpDisplay->setText(  QString::number(rampUp,   'f', 2) + " V/s"); }
void MainWindow::updateCoastDisplay()    { coastDisplay->setText(   QString::number(coast,     'f', 2) + " s");   }
void MainWindow::updateRampDownDisplay() { rampDownDisplay->setText(QString::number(rampDown,  'f', 2) + " V/s"); }

// =============================================================
// Clamp helpers
// =============================================================
double MainWindow::clampAmplitude(double v) { return std::round(std::max(1.0,  std::min(5.0,  v)) * 10.0) / 10.0; }
double MainWindow::clampRampUp(double v)    { return std::round(std::max(0.1,  std::min(3.0,  v)) * 10.0) / 10.0; }
double MainWindow::clampCoast(double v)     { return std::round(std::max(0.0,  std::min(10.0, v)) * 10.0) / 10.0; }
double MainWindow::clampRampDown(double v)  { return std::round(std::max(-3.0, std::min(-0.1, v)) * 10.0) / 10.0; }

// =============================================================
// AMPLITUDE
// =============================================================
void MainWindow::onAmpPlusClicked()  { amplitude = clampAmplitude(amplitude + 0.1); updateAmplitudeDisplay(); }
void MainWindow::onAmpMinusClicked() { amplitude = clampAmplitude(amplitude - 0.1); updateAmplitudeDisplay(); }
void MainWindow::onAmpPlusHold()     { amplitude = clampAmplitude(amplitude + 0.2); updateAmplitudeDisplay(); }
void MainWindow::onAmpMinusHold()    { amplitude = clampAmplitude(amplitude - 0.2); updateAmplitudeDisplay(); }

// =============================================================
// RAMP UP
// =============================================================
void MainWindow::onRampUpPlusClicked()  { rampUp = clampRampUp(rampUp + 0.1); updateRampUpDisplay(); }
void MainWindow::onRampUpMinusClicked() { rampUp = clampRampUp(rampUp - 0.1); updateRampUpDisplay(); }
void MainWindow::onRampUpPlusHold()     { rampUp = clampRampUp(rampUp + 0.2); updateRampUpDisplay(); }
void MainWindow::onRampUpMinusHold()    { rampUp = clampRampUp(rampUp - 0.2); updateRampUpDisplay(); }

// =============================================================
// COAST
// =============================================================
void MainWindow::onCoastPlusClicked()  { coast = clampCoast(coast + 0.1); updateCoastDisplay(); }
void MainWindow::onCoastMinusClicked() { coast = clampCoast(coast - 0.1); updateCoastDisplay(); }
void MainWindow::onCoastPlusHold()     { coast = clampCoast(coast + 0.2); updateCoastDisplay(); }
void MainWindow::onCoastMinusHold()    { coast = clampCoast(coast - 0.2); updateCoastDisplay(); }

// =============================================================
// RAMP DOWN   (+) toward -0.1  |  (-) toward -3.0
// =============================================================
void MainWindow::onRampDownPlusClicked()  { rampDown = clampRampDown(rampDown + 0.1); updateRampDownDisplay(); }
void MainWindow::onRampDownMinusClicked() { rampDown = clampRampDown(rampDown - 0.1); updateRampDownDisplay(); }
void MainWindow::onRampDownPlusHold()     { rampDown = clampRampDown(rampDown + 0.2); updateRampDownDisplay(); }
void MainWindow::onRampDownMinusHold()    { rampDown = clampRampDown(rampDown - 0.2); updateRampDownDisplay(); }

// =============================================================
// Stop all timers
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
