#include "pppps/poc/PocTaskController.h"

#include <QMetaObject>
#include <QThread>

#include <exception>
#include <utility>

namespace pppps::poc {

PocTaskController::PocTaskController(QObject *parent)
    : QObject(parent)
{
    qRegisterMetaType<ProbeResult>();
}

PocTaskController::~PocTaskController()
{
    if (thread_ != nullptr) {
        cancellation_->cancel();
        thread_->quit();
        thread_->wait();
    }
}

bool PocTaskController::isRunning() const noexcept
{
    return running_;
}

void PocTaskController::start(const QString &filePath)
{
    if (running_) {
        emit failed(QStringLiteral("已有 POC 任务正在运行"));
        return;
    }

    cancellation_ = std::make_shared<CancellationFlag>();
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
    connect(thread, &QThread::started, workerContext, [this, thread, filePath, cancellation] {
        try {
            PocPipeline pipeline;
            auto result = pipeline.run(
                filePath,
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
        } catch (const TaskCanceled &) {
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
    });

    thread->start();
}

void PocTaskController::cancel()
{
    if (running_ && cancellation_) {
        cancellation_->cancel();
    }
}

} // namespace pppps::poc
