#ifndef HISTORYWINDOW_H
#define HISTORYWINDOW_H

#include <QMainWindow>

class HistoryWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit HistoryWindow(QWidget *parent = nullptr);
    ~HistoryWindow();

    static double lastAmplitude;   // Tracks the last session's amplitude

private slots:
    void onSettingsClicked();
    void onElectrodeMatrixClicked();
    void onEndSessionClicked();
};

#endif // HISTORYWINDOW_H
