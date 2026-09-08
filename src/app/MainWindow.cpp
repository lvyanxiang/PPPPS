#include "MainWindow.h"

#include <QFileDialog>
#include <QLabel>
#include <QMessageBox>
#include <QProgressBar>
#include <QPushButton>
#include <QTextEdit>
#include <QVBoxLayout>
#include <QWidget>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
{
    setWindowTitle(QStringLiteral("PPPPS — Qt POC"));
    resize(720, 460);

    auto *central = new QWidget(this);
    auto *layout = new QVBoxLayout(central);
    auto *heading = new QLabel(QStringLiteral("PPPPS 本地图片处理依赖探针"), central);
    heading->setStyleSheet(QStringLiteral("font-size: 20px; font-weight: 600;"));

    auto *description = new QLabel(
        QStringLiteral("选择 JPEG 或 PNG，任务将在后台执行 Qt 解码、OpenCV 处理和 ONNX Runtime CPU 探针。"),
        central
    );
    description->setWordWrap(true);

    openButton_ = new QPushButton(QStringLiteral("选择图片…"), central);
    cancelButton_ = new QPushButton(QStringLiteral("取消"), central);
    cancelButton_->setEnabled(false);
    progressBar_ = new QProgressBar(central);
    progressBar_->setRange(0, 100);
    stageLabel_ = new QLabel(QStringLiteral("等待输入"), central);
    details_ = new QTextEdit(central);
    details_->setReadOnly(true);
    details_->setPlaceholderText(QStringLiteral("运行结果和错误详情会显示在这里。"));

    layout->addWidget(heading);
    layout->addWidget(description);
    layout->addSpacing(8);
    layout->addWidget(openButton_);
    layout->addWidget(cancelButton_);
    layout->addWidget(progressBar_);
    layout->addWidget(stageLabel_);
    layout->addWidget(details_, 1);
    setCentralWidget(central);

    connect(openButton_, &QPushButton::clicked, this, &MainWindow::chooseImage);
    connect(cancelButton_, &QPushButton::clicked, &controller_, &pppps::poc::PocTaskController::cancel);
    connect(
        &controller_,
        &pppps::poc::PocTaskController::progressChanged,
        this,
        [this](const int percent, const QString &message) {
            progressBar_->setValue(percent);
            stageLabel_->setText(message);
        }
    );
    connect(&controller_, &pppps::poc::PocTaskController::completed, this, &MainWindow::showResult);
    connect(&controller_, &pppps::poc::PocTaskController::failed, this, &MainWindow::showError);
    connect(&controller_, &pppps::poc::PocTaskController::runningChanged, this, &MainWindow::setRunning);
    connect(&controller_, &pppps::poc::PocTaskController::canceled, this, [this] {
        stageLabel_->setText(QStringLiteral("任务已取消"));
        details_->append(QStringLiteral("后台任务已安全取消。"));
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

    progressBar_->setValue(0);
    details_->clear();
    details_->append(QStringLiteral("输入：%1").arg(filePath));
    controller_.start(filePath);
}

void MainWindow::showResult(const pppps::poc::ProbeResult &result)
{
    details_->append(
        QStringLiteral(
            "完成\n格式：%1\n尺寸：%2 × %3\nOpenCV BGR 均值：%4, %5, %6\n"
            "ONNX Runtime：%7\n执行后端：%8"
        )
            .arg(result.imageFormat)
            .arg(result.imageSize.width())
            .arg(result.imageSize.height())
            .arg(result.meanBgr[0], 0, 'f', 2)
            .arg(result.meanBgr[1], 0, 'f', 2)
            .arg(result.meanBgr[2], 0, 'f', 2)
            .arg(result.onnxRuntimeVersion)
            .arg(result.executionProviders.join(QStringLiteral(", ")))
    );
    stageLabel_->setText(QStringLiteral("验证完成"));
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

