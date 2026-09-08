#include "MainWindow.h"

#include "ImageCanvas.h"

#include <QFileDialog>
#include <QHBoxLayout>
#include <QLabel>
#include <QMessageBox>
#include <QProgressBar>
#include <QPushButton>
#include <QTextEdit>
#include <QVBoxLayout>
#include <QWidget>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , modelPath_(qEnvironmentVariable("PPPPS_MODEL_PATH", QStringLiteral(PPPPS_DEFAULT_MODEL_PATH)))
{
    setWindowTitle(QStringLiteral("PPPPS — 对象与 Mask POC"));
    resize(1080, 720);

    auto *central = new QWidget(this);
    auto *layout = new QVBoxLayout(central);
    auto *heading = new QLabel(QStringLiteral("PPPPS 本地对象识别与 Mask"), central);
    heading->setStyleSheet(QStringLiteral("font-size: 20px; font-weight: 600;"));

    auto *description = new QLabel(
        QStringLiteral("选择 JPEG 或 PNG 后，模型会自动找对象。移动鼠标可高亮轮廓，点击即可取得对象 Mask。"),
        central
    );
    description->setWordWrap(true);

    openButton_ = new QPushButton(QStringLiteral("选择图片…"), central);
    cancelButton_ = new QPushButton(QStringLiteral("取消"), central);
    cancelButton_->setEnabled(false);
    progressBar_ = new QProgressBar(central);
    progressBar_->setRange(0, 100);
    stageLabel_ = new QLabel(QStringLiteral("等待输入"), central);
    objectLabel_ = new QLabel(QStringLiteral("对象：尚未识别"), central);
    canvas_ = new ImageCanvas(central);
    details_ = new QTextEdit(central);
    details_->setReadOnly(true);
    details_->setPlaceholderText(QStringLiteral("运行结果和错误详情会显示在这里。"));

    layout->addWidget(heading);
    layout->addWidget(description);
    layout->addSpacing(8);
    auto *buttonLayout = new QHBoxLayout;
    buttonLayout->addWidget(openButton_, 1);
    buttonLayout->addWidget(cancelButton_);
    layout->addLayout(buttonLayout);
    layout->addWidget(progressBar_);
    layout->addWidget(stageLabel_);
    layout->addWidget(objectLabel_);
    auto *contentLayout = new QHBoxLayout;
    contentLayout->addWidget(canvas_, 1);
    details_->setMinimumWidth(300);
    details_->setMaximumWidth(380);
    contentLayout->addWidget(details_);
    layout->addLayout(contentLayout, 1);
    setCentralWidget(central);

    connect(openButton_, &QPushButton::clicked, this, &MainWindow::chooseImage);
    connect(
        cancelButton_,
        &QPushButton::clicked,
        &controller_,
        &pppps::segmentation::SegmentationTaskController::cancel
    );
    connect(
        &controller_,
        &pppps::segmentation::SegmentationTaskController::progressChanged,
        this,
        [this](const int percent, const QString &message) {
            progressBar_->setValue(percent);
            stageLabel_->setText(message);
        }
    );
    connect(
        &controller_,
        &pppps::segmentation::SegmentationTaskController::completed,
        this,
        &MainWindow::showResult
    );
    connect(&controller_, &pppps::segmentation::SegmentationTaskController::failed, this, &MainWindow::showError);
    connect(
        &controller_,
        &pppps::segmentation::SegmentationTaskController::runningChanged,
        this,
        &MainWindow::setRunning
    );
    connect(&controller_, &pppps::segmentation::SegmentationTaskController::canceled, this, [this] {
        stageLabel_->setText(QStringLiteral("任务已取消"));
        details_->append(QStringLiteral("后台任务已安全取消。"));
    });
    connect(canvas_, &ImageCanvas::hoveredObjectChanged, this, &MainWindow::showHoveredObject);
    connect(canvas_, &ImageCanvas::selectedObjectChanged, this, &MainWindow::showSelectedObject);
}

void MainWindow::chooseImage()
{
    const auto filePath = QFileDialog::getOpenFileName(
        this,
        QStringLiteral("选择 POC 图片"),
        {},
        QStringLiteral("图片 (*.jpg *.jpeg *.png)")
    );
    if (filePath.isEmpty()) {
        return;
    }

    progressBar_->setValue(0);
    canvas_->clearResult();
    objectLabel_->setText(QStringLiteral("对象：识别中…"));
    details_->clear();
    details_->append(QStringLiteral("输入：%1").arg(filePath));
    details_->append(QStringLiteral("模型：%1").arg(modelPath_));
    controller_.start(filePath, modelPath_);
}

void MainWindow::showResult(const pppps::segmentation::SegmentationResult &result)
{
    canvas_->setResult(result);
    details_->append(
        QStringLiteral(
            "完成\n尺寸：%1 × %2\n对象：%3 个\n执行后端：%4\n"
            "模型加载：%5 ms\n预处理：%6 ms\n推理：%7 ms\n后处理：%8 ms\n总耗时：%9 ms"
        )
            .arg(result.sourceImage.width())
            .arg(result.sourceImage.height())
            .arg(result.objects.size())
            .arg(result.executionProvider)
            .arg(result.modelLoadMs)
            .arg(result.preprocessingMs)
            .arg(result.inferenceMs)
            .arg(result.postprocessingMs)
            .arg(result.totalMs)
    );
    for (const auto &object : result.objects) {
        details_->append(
            QStringLiteral("#%1  %2  %3%  Mask %4 px")
                .arg(object.id)
                .arg(object.className)
                .arg(object.confidence * 100.0F, 0, 'f', 1)
                .arg(object.maskArea)
        );
    }
    stageLabel_->setText(QStringLiteral("识别完成：把鼠标移到对象上，再点击选择"));
    objectLabel_->setText(
        result.objects.isEmpty()
            ? QStringLiteral("对象：未找到置信度 ≥ 70% 的对象")
            : QStringLiteral("对象：已找到 %1 个，等待悬停").arg(result.objects.size())
    );
}

void MainWindow::showError(const QString &message)
{
    stageLabel_->setText(QStringLiteral("验证失败"));
    details_->append(QStringLiteral("错误：%1").arg(message));
    QMessageBox::critical(this, QStringLiteral("PPPPS POC 错误"), message);
}

void MainWindow::setRunning(const bool running)
{
    openButton_->setEnabled(!running);
    cancelButton_->setEnabled(running);
}

void MainWindow::showHoveredObject(const int objectIndex)
{
    const auto &objects = canvas_->result().objects;
    if (objectIndex < 0 || objectIndex >= objects.size()) {
        objectLabel_->setText(QStringLiteral("对象：等待悬停"));
        return;
    }
    const auto &object = objects[objectIndex];
    objectLabel_->setText(
        QStringLiteral("悬停：#%1 %2（%3%）")
            .arg(object.id)
            .arg(object.className)
            .arg(object.confidence * 100.0F, 0, 'f', 1)
    );
}

void MainWindow::showSelectedObject(const int objectIndex)
{
    const auto &objects = canvas_->result().objects;
    if (objectIndex < 0 || objectIndex >= objects.size()) {
        details_->append(QStringLiteral("点击未命中对象。"));
        return;
    }
    const auto &object = objects[objectIndex];
    details_->append(
        QStringLiteral("点击命中 #%1 %2：已获得 %3 × %4 Mask（%5 px）")
            .arg(object.id)
            .arg(object.className)
            .arg(object.mask.width())
            .arg(object.mask.height())
            .arg(object.maskArea)
    );
}
