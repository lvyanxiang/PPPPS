#pragma once

#include "pppps/matting/MattingTaskController.h"
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
    void runMatting();
    void showResult(const pppps::segmentation::SegmentationResult &result);
    void showMattingResult(const pppps::matting::MattingResult &result);
    void showError(const QString &message);
    void setRunning(bool running);
    void showHoveredObject(int objectIndex);
    void showSelectedObject(int objectIndex);

private:
    pppps::segmentation::SegmentationTaskController controller_;
    pppps::matting::MattingTaskController mattingController_;
    QString modelPath_;
    QString mattingModelPath_;
    QString currentImagePath_;
    ImageCanvas *canvas_{nullptr};
    QLabel *objectLabel_{nullptr};
    QLabel *stageLabel_{nullptr};
    QProgressBar *progressBar_{nullptr};
    QPushButton *openButton_{nullptr};
    QPushButton *mattingButton_{nullptr};
    QPushButton *viewButton_{nullptr};
    QPushButton *restoreButton_{nullptr};
    QPushButton *eraseButton_{nullptr};
    QPushButton *cancelButton_{nullptr};
    QTextEdit *details_{nullptr};
};
