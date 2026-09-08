#pragma once

#include "pppps/segmentation/SegmentationTaskController.h"

#include <QMainWindow>

class QLabel;
class QProgressBar;
class QPushButton;
class QTextEdit;
class ImageCanvas;

class MainWindow final : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);

private slots:
    void chooseImage();
    void showResult(const pppps::segmentation::SegmentationResult &result);
    void showError(const QString &message);
    void setRunning(bool running);
    void showHoveredObject(int objectIndex);
    void showSelectedObject(int objectIndex);

private:
    pppps::segmentation::SegmentationTaskController controller_;
    QString modelPath_;
    ImageCanvas *canvas_{nullptr};
    QLabel *objectLabel_{nullptr};
    QLabel *stageLabel_{nullptr};
    QProgressBar *progressBar_{nullptr};
    QPushButton *openButton_{nullptr};
    QPushButton *cancelButton_{nullptr};
    QTextEdit *details_{nullptr};
};
