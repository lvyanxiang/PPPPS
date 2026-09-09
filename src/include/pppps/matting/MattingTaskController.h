#pragma once

#include "pppps/matting/MattingTypes.h"
#include "pppps/poc/PocPipeline.h"

#include <QObject>
#include <QString>

#include <memory>

class QThread;

namespace pppps::matting {

class MattingTaskController final : public QObject {
    Q_OBJECT

public:
    explicit MattingTaskController(QObject *parent = nullptr);
    ~MattingTaskController() override;

    [[nodiscard]] bool isRunning() const noexcept;

public slots:
    void start(const QString &imagePath, const QString &modelPath);
    void cancel();

signals:
    void progressChanged(int percent, const QString &message);
    void completed(const pppps::matting::MattingResult &result);
    void failed(const QString &message);
    void canceled();
    void runningChanged(bool running);

private:
    QThread *thread_{nullptr};
    std::shared_ptr<pppps::poc::CancellationFlag> cancellation_;
    bool running_{false};
};

} // namespace pppps::matting
