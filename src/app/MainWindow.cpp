#include "MainWindow.h"

#include "ImageCanvas.h"
#include "pppps/matting/MattingTypes.h"

#include <QFileDialog>
#include <QHBoxLayout>
#include <QLabel>
#include <QMessageBox>
#include <QProgressBar>
#include <QPushButton>
#include <QTextEdit>
#include <QVBoxLayout>
#include <QWidget>

#include <algorithm>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , modelPath_(qEnvironmentVariable("PPPPS_MODEL_PATH", QStringLiteral(PPPPS_DEFAULT_MODEL_PATH)))
    , mattingModelPath_(
          qEnvironmentVariable("PPPPS_MODNET_MODEL_PATH", QStringLiteral(PPPPS_DEFAULT_MODNET_MODEL_PATH))
      )
{
    setWindowTitle(QStringLiteral("PPPPS — 对象 Mask 与 Alpha Matting POC"));
    resize(1080, 720);

    auto *central = new QWidget(this);
    auto *layout = new QVBoxLayout(central);
    auto *heading = new QLabel(QStringLiteral("PPPPS 本地对象 Mask 与人物精细抠图"), central);
    heading->setStyleSheet(QStringLiteral("font-size: 20px; font-weight: 600;"));

    auto *description = new QLabel(
        QStringLiteral(
            "先用对象模型取得硬边 Mask，再用人物模型生成保留发丝的连续 Alpha；"
            "Alpha 可用画笔补回或擦除。两次推理都在本机 CPU 完成。"
        ),
        central
    );
    description->setWordWrap(true);

    openButton_ = new QPushButton(QStringLiteral("选择图片…"), central);
    mattingButton_ = new QPushButton(QStringLiteral("运行人物精细抠图"), central);
    mattingButton_->setEnabled(false);
    viewButton_ = new QPushButton(QStringLiteral("查看 Alpha"), central);
    viewButton_->setEnabled(false);
    restoreButton_ = new QPushButton(QStringLiteral("补回画笔"), central);
    restoreButton_->setCheckable(true);
    restoreButton_->setEnabled(false);
    eraseButton_ = new QPushButton(QStringLiteral("擦除画笔"), central);
    eraseButton_->setCheckable(true);
    eraseButton_->setEnabled(false);
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
    buttonLayout->addWidget(mattingButton_);
    buttonLayout->addWidget(viewButton_);
    buttonLayout->addWidget(restoreButton_);
    buttonLayout->addWidget(eraseButton_);
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
    connect(mattingButton_, &QPushButton::clicked, this, &MainWindow::runMatting);
    connect(viewButton_, &QPushButton::clicked, this, [this] {
        const auto showAlpha = !canvas_->isShowingAlphaMatte();
        canvas_->setShowAlphaMatte(showAlpha);
        viewButton_->setText(showAlpha ? QStringLiteral("查看对象 Mask") : QStringLiteral("查看 Alpha"));
        restoreButton_->setEnabled(showAlpha);
        eraseButton_->setEnabled(showAlpha);
        if (!showAlpha) {
            restoreButton_->setChecked(false);
            eraseButton_->setChecked(false);
        }
    });
    connect(restoreButton_, &QPushButton::toggled, this, [this](const bool checked) {
        if (checked) {
            eraseButton_->setChecked(false);
        }
        canvas_->setAlphaBrushMode(
            checked ? ImageCanvas::AlphaBrushMode::Restore : ImageCanvas::AlphaBrushMode::Disabled
        );
    });
    connect(eraseButton_, &QPushButton::toggled, this, [this](const bool checked) {
        if (checked) {
            restoreButton_->setChecked(false);
        }
        canvas_->setAlphaBrushMode(
            checked ? ImageCanvas::AlphaBrushMode::Erase : ImageCanvas::AlphaBrushMode::Disabled
        );
    });
    connect(
        cancelButton_,
        &QPushButton::clicked,
        this,
        [this] {
            controller_.cancel();
            mattingController_.cancel();
        }
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
    connect(
        &mattingController_,
        &pppps::matting::MattingTaskController::progressChanged,
        this,
        [this](const int percent, const QString &message) {
            progressBar_->setValue(percent);
            stageLabel_->setText(message);
        }
    );
    connect(
        &mattingController_,
        &pppps::matting::MattingTaskController::completed,
        this,
        &MainWindow::showMattingResult
    );
    connect(&mattingController_, &pppps::matting::MattingTaskController::failed, this, &MainWindow::showError);
    connect(
        &mattingController_,
        &pppps::matting::MattingTaskController::runningChanged,
        this,
        &MainWindow::setRunning
    );
    connect(&mattingController_, &pppps::matting::MattingTaskController::canceled, this, [this] {
        stageLabel_->setText(QStringLiteral("人物抠图任务已取消"));
        details_->append(QStringLiteral("人物抠图后台任务已安全取消。"));
    });
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

    currentImagePath_ = filePath;
    progressBar_->setValue(0);
    canvas_->clearResult();
    objectLabel_->setText(QStringLiteral("对象：识别中…"));
    details_->clear();
    details_->append(QStringLiteral("输入：%1").arg(filePath));
    details_->append(QStringLiteral("模型：%1").arg(modelPath_));
    mattingButton_->setEnabled(false);
    viewButton_->setEnabled(false);
    restoreButton_->setEnabled(false);
    eraseButton_->setEnabled(false);
    viewButton_->setText(QStringLiteral("查看 Alpha"));
    controller_.start(filePath, modelPath_);
}

void MainWindow::runMatting()
{
    if (currentImagePath_.isEmpty()) {
        return;
    }
    progressBar_->setValue(0);
    details_->append(QStringLiteral("\n人物抠图模型：%1").arg(mattingModelPath_));
    mattingController_.start(currentImagePath_, mattingModelPath_);
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
    mattingButton_->setEnabled(true);
}

void MainWindow::showMattingResult(const pppps::matting::MattingResult &result)
{
    canvas_->setAlphaMatte(result.alphaMatte);
    viewButton_->setEnabled(true);
    viewButton_->setText(QStringLiteral("查看对象 Mask"));
    restoreButton_->setEnabled(true);
    eraseButton_->setEnabled(true);

    const auto pixelCount = std::max<qsizetype>(
        1,
        static_cast<qsizetype>(result.sourceImage.width()) * result.sourceImage.height()
    );
    details_->append(
        QStringLiteral(
            "人物 Alpha 完成\n软透明像素：%1（%2%）\n平均 Alpha：%3\n执行后端：%4\n"
            "模型加载：%5 ms\n预处理：%6 ms\n推理：%7 ms\n后处理：%8 ms\n总耗时：%9 ms"
        )
            .arg(result.statistics.softPixels)
            .arg(result.statistics.softPixels * 100.0 / pixelCount, 0, 'f', 2)
            .arg(result.statistics.meanAlpha, 0, 'f', 3)
            .arg(result.executionProvider)
            .arg(result.modelLoadMs)
            .arg(result.preprocessingMs)
            .arg(result.inferenceMs)
            .arg(result.postprocessingMs)
            .arg(result.totalMs)
    );

    const auto person = std::find_if(
        canvas_->result().objects.cbegin(),
        canvas_->result().objects.cend(),
        [](const auto &object) { return object.className == QStringLiteral("person"); }
    );
    if (person != canvas_->result().objects.cend()) {
        const auto comparison = pppps::matting::compareBinaryMaskWithAlpha(
            person->mask,
            result.alphaMatte
        );
        details_->append(
            QStringLiteral("人物硬边 Mask 与 Alpha：阈值 IoU %1，平均差异 %2")
                .arg(comparison.thresholdIoU, 0, 'f', 3)
                .arg(comparison.meanAbsoluteDifference, 0, 'f', 3)
        );
    } else {
        details_->append(QStringLiteral("当前对象模型未找到 person，无法做同图人物 Mask 对比。"));
    }
    stageLabel_->setText(QStringLiteral("Alpha 完成：棋盘格表示透明；可用补回/擦除画笔修边"));
    objectLabel_->setText(QStringLiteral("模式：连续 Alpha Matting（非二值 Mask）"));
}

void MainWindow::showError(const QString &message)
{
    stageLabel_->setText(QStringLiteral("验证失败"));
    details_->append(QStringLiteral("错误：%1").arg(message));
    QMessageBox::critical(this, QStringLiteral("PPPPS POC 错误"), message);
}

void MainWindow::setRunning(const bool running)
{
    Q_UNUSED(running)
    const auto anyRunning = controller_.isRunning() || mattingController_.isRunning();
    openButton_->setEnabled(!anyRunning);
    mattingButton_->setEnabled(
        !anyRunning && !currentImagePath_.isEmpty() && !canvas_->result().sourceImage.isNull()
    );
    cancelButton_->setEnabled(anyRunning);
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
