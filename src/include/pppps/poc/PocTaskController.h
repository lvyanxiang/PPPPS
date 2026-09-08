#pragma once

#include "pppps/poc/PocPipeline.h"

#include <QObject>

#include <memory>

class QThread;

namespace pppps::poc {

class PocTaskController final : public QObject {
    Q_OBJECT

public:
    explicit PocTaskController(QObject *parent = nullptr);
    ~PocTaskController() override;

    [[nodiscard]] bool isRunning() const noexcept;

public slots:
    void start(const QString &filePath);
    void cancel();

signals:
    void progressChanged(int percent, const QString &message);
    void completed(const pppps::poc::ProbeResult &result);
    void failed(const QString &message);
    void canceled();
    void runningChanged(bool running);

private:
    QThread *thread_{nullptr};
    std::shared_ptr<CancellationFlag> cancellation_;
    bool running_{false};
};

} // namespace pppps::poc

