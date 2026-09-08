#include "pppps/segmentation/SegmentationTaskController.h"

#include "pppps/segmentation/MaskRcnnEngine.h"

#include <QMetaObject>
#include <QThread>

#include <exception>
#include <utility>

namespace pppps::segmentation {

SegmentationTaskController::SegmentationTaskController(QObject *parent)
    : QObject(parent)
{
    qRegisterMetaType<SegmentationResult>();
}

SegmentationTaskController::~SegmentationTaskController()
{
    if (thread_ != nullptr) {
        cancellation_->cancel();
        thread_->quit();
        thread_->wait();
    }
}

bool SegmentationTaskController::isRunning() const noexcept
{
    return running_;
}

void SegmentationTaskController::start(
    const QString &imagePath,
    const QString &modelPath
)
{
    if (running_) {
        emit failed(QStringLiteral("已有实例分割任务正在运行"));
        return;
    }

    cancellation_ = std::make_shared<pppps::poc::CancellationFlag>();
    auto *thread = new QThread(this);
    auto *workerContext = new QObject;
    workerContext->moveToThread(thread);
    thread_ = thread;
    running_ = true;
    emit runningChanged(true);

    connect(thread, &QThread::finished, workerContext, &QObject::deleteLater);
    connect(thread, &QThread::finished, this, [this, thread] {
        if (thread_ == thread) {
            thread_ = nullptr;
            running_ = false;
            emit runningChanged(false);
        }
        thread->deleteLater();
    });

    const auto cancellation = cancellation_;
    connect(
        thread,
        &QThread::started,
        workerContext,
        [this, thread, imagePath, modelPath, cancellation] {
            try {
                MaskRcnnEngine engine(modelPath);
                auto result = engine.run(
                    imagePath,
                    *cancellation,
                    [this](const int percent, const QString &message) {
                        QMetaObject::invokeMethod(
                            this,
                            [this, percent, message] { emit progressChanged(percent, message); },
                            Qt::QueuedConnection
                        );
                    }
                );
                QMetaObject::invokeMethod(
                    this,
                    [this, result = std::move(result)] { emit completed(result); },
                    Qt::QueuedConnection
                );
            } catch (const pppps::poc::TaskCanceled &) {
                QMetaObject::invokeMethod(
                    this,
                    [this] { emit canceled(); },
                    Qt::QueuedConnection
                );
            } catch (const std::exception &error) {
                const auto message = QString::fromUtf8(error.what());
                QMetaObject::invokeMethod(
                    this,
                    [this, message] { emit failed(message); },
                    Qt::QueuedConnection
                );
            }
            thread->quit();
        }
    );

    thread->start();
}

void SegmentationTaskController::cancel()
{
    if (running_ && cancellation_) {
        cancellation_->cancel();
    }
}

} // namespace pppps::segmentation

