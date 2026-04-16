#include "mainwindow.h"
#include "electrodewindow.h"
#include "historywindow.h"
#include "spihandler.h"
#include "globals.h"

#include <QStringList>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
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
// Factory: compact red button  (50 x 50)
// =============================================================
QPushButton* MainWindow::makeRedBtn(const QString &label) {
    auto *btn = new QPushButton(label);
    btn->setFixedSize(50, 50);
    btn->setStyleSheet(
        "QPushButton { background: rgb(214,61,61); color: white; "
        "border-radius:6px; font-size:22px; font-weight:bold; }"
        "QPushButton:pressed { background: rgb(180,40,40); }");
    return btn;
}

// =============================================================
// Factory: value display  (130 x 50)
// =============================================================
QLineEdit* MainWindow::makeDisplay(const QString &initText) {
    auto *d = new QLineEdit(initText);
    d->setAlignment(Qt::AlignCenter);
    d->setReadOnly(true);
    d->setFixedSize(130, 50);
    d->setStyleSheet(
        "QLineEdit { background: white; color: black; "
        "border-radius:5px; "
        "font-size:16px; font-weight:bold; }");
    return d;
}

// =============================================================
// Factory: [−] [display] [+]  row
// =============================================================
QHBoxLayout* MainWindow::buildParamRow(QPushButton *&minusOut,
                                       QLineEdit   *&displayOut,
                                       QPushButton *&plusOut,
                                       const QString &initText)
{
    auto *row = new QHBoxLayout;
    row->setSpacing(10);
    row->setContentsMargins(0, 0, 0, 0);
    minusOut   = makeRedBtn("-");        row->addWidget(minusOut);
    displayOut = makeDisplay(initText);  row->addWidget(displayOut);
    plusOut    = makeRedBtn("+");        row->addWidget(plusOut);
    row->addStretch();
    return row;
}

// =============================================================
// Wire button → hold timer + click slot
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
// Constructor  —  target screen: 800 x 480 px
// Layout budget:
//   Top bar      :  42 px
//   Content area : 438 px  (margins 10 top/bottom = 418 usable)
//   4 rows × (label 18 + row 50 + gap 14) = 4 × 82 = 328 px
//   Remaining stretch: ~90 px shared between top & bottom
// =============================================================
MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent),
      amplitude(1.0), rampUp(1.0), coast(1.0), rampDown(1.0)
{
    setWindowTitle("FES_2025");
    this->setCursor(Qt::BlankCursor);
    amplitude = HistoryWindow::lastAmplitude;

    // ── Root widget ───────────────────────────────────────────────────────
    QWidget *central = new QWidget(this);
    QVBoxLayout *mainLayout = new QVBoxLayout(central);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);
    central->setStyleSheet("background: white; border: none;");
    setStyleSheet("QMainWindow { border: none; }");

    // ── Top bar  42 px ────────────────────────────────────────────────────
    topBar = new QWidget(central);
    topBar->setFixedHeight(42);
    topBar->setStyleSheet("background-color: rgb(141,25,36);");
    QHBoxLayout *topBarLayout = new QHBoxLayout(topBar);
    topBarLayout->setContentsMargins(16, 0, 16, 0);
    topBarLayout->addStretch();
    titleLabel = new QLabel("Stimulation Settings", topBar);
    titleLabel->setStyleSheet(
        "color: white; font-size:16px; font-weight:700;"
        "background: transparent; border: none;");
    titleLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    topBarLayout->addWidget(titleLabel);
    mainLayout->addWidget(topBar);

    // ── Content widget ────────────────────────────────────────────────────
    QWidget *content = new QWidget(central);
    content->setStyleSheet("background-color: white;");
    QHBoxLayout *contentLayout = new QHBoxLayout(content);
    contentLayout->setContentsMargins(24, 10, 24, 10);
    contentLayout->setSpacing(10);

    // ── LEFT COLUMN  ──────────────────────────────────────────────────────
    // Uses a QGridLayout so labels and control rows are pixel-aligned
    QGridLayout *grid = new QGridLayout;
    grid->setVerticalSpacing(4);     // gap between label row and btn row
    grid->setHorizontalSpacing(0);
    grid->setContentsMargins(0, 0, 0, 0);

    // Row index helper
    int r = 0;

    auto addRow = [&](const QString &title,
                      QPushButton *&minusBtn,
                      QLineEdit   *&display,
                      QPushButton *&plusBtn,
                      const QString &initText)
    {
        // Label
        QLabel *lbl = new QLabel(title);
        lbl->setStyleSheet("font-size:14px; font-weight:700; color:#222;");
        lbl->setFixedHeight(20);
        grid->addWidget(lbl, r, 0, 1, 3, Qt::AlignVCenter);
        r++;

        // Buttons + display in one HBox, placed in one cell
        QWidget *cell = new QWidget;
        cell->setStyleSheet("background: transparent;");
        QHBoxLayout *h = new QHBoxLayout(cell);
        h->setContentsMargins(0, 0, 0, 0);
        h->setSpacing(10);
        minusBtn = makeRedBtn("-"); h->addWidget(minusBtn);
        display  = makeDisplay(initText); h->addWidget(display);
        plusBtn  = makeRedBtn("+"); h->addWidget(plusBtn);
        h->addStretch();

        grid->addWidget(cell, r, 0, 1, 3, Qt::AlignVCenter);
        r++;

        // Spacer row between parameters
        QWidget *spacer = new QWidget;
        spacer->setFixedHeight(14);
        grid->addWidget(spacer, r, 0);
        r++;
    };

    addRow("Set Maximum Amplitude",
           ampMinusBtn, amplitudeDisplay, ampPlusBtn, "1.00 V");
    addRow("Set Ramp Up Rate",
           rampUpMinusBtn, rampUpDisplay, rampUpPlusBtn, "1.00 V/s");
    addRow("Set Coast Duration",
           coastMinusBtn, coastDisplay, coastPlusBtn, "1.00 s");
    addRow("Set Ramp Down Rate",
           rampDownMinusBtn, rampDownDisplay, rampDownPlusBtn, "1.00 V/s");

    // Wrap grid in a VBox so it stays top-aligned inside the left column
    QVBoxLayout *leftWrapper = new QVBoxLayout;
    leftWrapper->setContentsMargins(0, 0, 0, 0);
    leftWrapper->setSpacing(0);
    leftWrapper->addLayout(grid);
    leftWrapper->addStretch();

    // ── RIGHT COLUMN ──────────────────────────────────────────────────────
    QVBoxLayout *rightCol = new QVBoxLayout;
    rightCol->setContentsMargins(20, 0, 0, 0);
    rightCol->setSpacing(8);
    rightCol->addStretch();   // push content to vertical centre

    // Electrode Matrix button
    electrodeBtn = new QPushButton("Electrode Matrix");
    electrodeBtn->setFixedSize(240, 100);
    electrodeBtn->setStyleSheet(
        "QPushButton { background: rgb(214,61,61); color: white; "
        "border-radius:8px; font-size:22px; font-weight:bold; }"
        "QPushButton:pressed { background: rgb(180,40,40); }");
    rightCol->addWidget(electrodeBtn, 0, Qt::AlignHCenter);

    rightCol->addSpacing(10);

    // FES Signal label
    QLabel *fesLabel = new QLabel("FES Signal");
    fesLabel->setStyleSheet("font-size:13px; font-weight:700; color:#222;");
    rightCol->addWidget(fesLabel, 0, Qt::AlignLeft);

    // FES Signal image placeholder
    carrierPic = new ClickableLabel;
    carrierPic->setFixedSize(240, 160);
    carrierPic->setScaledContents(true);
    carrierPic->setStyleSheet(
        "background: #ebebeb; border:1px solid #bbb; border-radius:6px;");
    rightCol->addWidget(carrierPic, 0, Qt::AlignHCenter);

    rightCol->addStretch();   // symmetric bottom stretch

    // Hidden compat widgets
    amPic = new ClickableLabel;
    amPic->setFixedSize(0,0);
    amPic->hide();
    rightCol->addWidget(amPic);

    lblCarrier = new QLabel("Carrier Frequency\n10 kHz"); lblCarrier->hide();
    lblAM      = new QLabel("AM Frequency\n50 Hz");       lblAM->hide();
    rightCol->addWidget(lblCarrier);
    rightCol->addWidget(lblAM);

    // ── Assemble content ──────────────────────────────────────────────────
    contentLayout->addLayout(leftWrapper, 55);   // 55 % width
    contentLayout->addLayout(rightCol,    45);   // 45 % width

    mainLayout->addWidget(content, 1);
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

    // Init
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
        amPic->setPixmap(apix.scaled(amPic->size(), Qt::KeepAspectRatio, Qt::SmoothTransformation));
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
double MainWindow::clampAmplitude(double v) { return std::round(std::max(1.0, std::min(5.0,  v))*10.0)/10.0; }
double MainWindow::clampRampUp(double v)    { return std::round(std::max(0.1, std::min(3.0,  v))*10.0)/10.0; }
double MainWindow::clampCoast(double v)     { return std::round(std::max(0.0, std::min(10.0, v))*10.0)/10.0; }
double MainWindow::clampRampDown(double v) { return std::round(std::max(0.1, std::min(3.0, v))*10.0)/10.0; }

// =============================================================
// AMPLITUDE
// =============================================================
void MainWindow::onAmpPlusClicked()  { amplitude = clampAmplitude(amplitude+0.1); updateAmplitudeDisplay(); }
void MainWindow::onAmpMinusClicked() { amplitude = clampAmplitude(amplitude-0.1); updateAmplitudeDisplay(); }
void MainWindow::onAmpPlusHold()     { amplitude = clampAmplitude(amplitude+0.2); updateAmplitudeDisplay(); }
void MainWindow::onAmpMinusHold()    { amplitude = clampAmplitude(amplitude-0.2); updateAmplitudeDisplay(); }

// =============================================================
// RAMP UP
// =============================================================
void MainWindow::onRampUpPlusClicked()  { rampUp = clampRampUp(rampUp+0.1); updateRampUpDisplay(); }
void MainWindow::onRampUpMinusClicked() { rampUp = clampRampUp(rampUp-0.1); updateRampUpDisplay(); }
void MainWindow::onRampUpPlusHold()     { rampUp = clampRampUp(rampUp+0.2); updateRampUpDisplay(); }
void MainWindow::onRampUpMinusHold()    { rampUp = clampRampUp(rampUp-0.2); updateRampUpDisplay(); }

// =============================================================
// COAST
// =============================================================
void MainWindow::onCoastPlusClicked()  { coast = clampCoast(coast+0.1); updateCoastDisplay(); }
void MainWindow::onCoastMinusClicked() { coast = clampCoast(coast-0.1); updateCoastDisplay(); }
void MainWindow::onCoastPlusHold()     { coast = clampCoast(coast+0.2); updateCoastDisplay(); }
void MainWindow::onCoastMinusHold()    { coast = clampCoast(coast-0.2); updateCoastDisplay(); }

// =============================================================
// RAMP DOWN   (+) toward -0.1  |  (-) toward -3.0
// =============================================================
void MainWindow::onRampDownPlusClicked()  { rampDown = clampRampDown(rampDown+0.1); updateRampDownDisplay(); }
void MainWindow::onRampDownMinusClicked() { rampDown = clampRampDown(rampDown-0.1); updateRampDownDisplay(); }
void MainWindow::onRampDownPlusHold()     { rampDown = clampRampDown(rampDown+0.2); updateRampDownDisplay(); }
void MainWindow::onRampDownMinusHold()    { rampDown = clampRampDown(rampDown-0.2); updateRampDownDisplay(); }

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
    ElectrodeWindow *ew = new ElectrodeWindow(this);
    ew->showFullScreen();
}

// =============================================================
// Image clicks
// =============================================================
void MainWindow::onCarrierClicked() { qDebug() << "FES signal image clicked"; }
void MainWindow::onAMClicked()      { qDebug() << "AM image clicked"; }
