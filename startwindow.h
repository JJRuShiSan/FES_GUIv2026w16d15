#ifndef STARTWINDOW_H
#define STARTWINDOW_H

#include <QMainWindow>

class QPushButton;
class QLabel;

class StartWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit StartWindow(QWidget *parent = nullptr);
    ~StartWindow();

private:
    QPushButton *pushButton;
    QLabel *label;
};

#endif // STARTWINDOW_H
