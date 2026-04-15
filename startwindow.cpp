#include "startwindow.h"
#include "mainwindow.h"

#include <QPushButton>
#include <QLabel>
#include <QStatusBar>
#include <QPixmap>
#include <QFont>

StartWindow::StartWindow(QWidget *parent)
    : QMainWindow(parent),
    pushButton(new QPushButton(this)),
    label(new QLabel(this))
{
    this->setCursor(Qt::BlankCursor);
    this->setWindowTitle("FES_2025");
    this->setStyleSheet("background-color: rgb(255, 255, 255);");

    // QLabel (Logo)
    label->setGeometry(80, 20, 641, 351);
    QPixmap pixmap(":/FES.png");  // resource path from logo.qrc
    label->setPixmap(pixmap);
    label->setScaledContents(true);

    // QPushButton (Start Session)
    pushButton->setGeometry(260, 380, 261, 71);
    QFont font("Roboto", 24);
    pushButton->setFont(font);

    QString btnStyle =
        "QPushButton{ background:#d63d3b; border-radius:10px; font-size:35px; color: white; } "
        "QPushButton:pressed{ background:#4d4d4d; }";
    pushButton->setStyleSheet(btnStyle);

    pushButton->setText("Start Session");

    // connect button to open MainWindow
    connect(pushButton, &QPushButton::clicked, this, [this]() {
        MainWindow *mw = new MainWindow();
        mw->showFullScreen();
        mw->show();
        this->close();  // close StartWindow
    });

    // Status bar
    QStatusBar *statusbar = new QStatusBar(this);

    this->setStatusBar(statusbar);
}

StartWindow::~StartWindow() {}
