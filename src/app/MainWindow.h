#pragma once

#include "pppps/poc/PocTaskController.h"

#include <QMainWindow>

class QLabel;
class QProgressBar;
class QPushButton;
class QTextEdit;

class MainWindow final : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);

private slots:
    void chooseImage();
    void showResult(const pppps::poc::ProbeResult &result);
    void showError(const QString &message);
    void setRunning(bool running);

private:
    pppps::poc::PocTaskController controller_;
    QLabel *stageLabel_{nullptr};
    QProgressBar *progressBar_{nullptr};
    QPushButton *openButton_{nullptr};
    QPushButton *cancelButton_{nullptr};
    QTextEdit *details_{nullptr};
};

