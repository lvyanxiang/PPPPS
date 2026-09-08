#pragma once

#include "pppps/poc/PocPipeline.h"
#include "pppps/segmentation/SegmentationTypes.h"

#include <QObject>
#include <QString>

#include <memory>

class QThread;

namespace pppps::segmentation {

class SegmentationTaskController final : public QObject {
    Q_OBJECT

public:
    explicit SegmentationTaskController(QObject *parent = nullptr);
    ~SegmentationTaskController() override;

    [[nodiscard]] bool isRunning() const noexcept;

public slots:
    void start(const QString &imagePath, const QString &modelPath);
    void cancel();

signals:
    void progressChanged(int percent, const QString &message);
    void completed(const pppps::segmentation::SegmentationResult &result);
    void failed(const QString &message);
    void canceled();
    void runningChanged(bool running);

private:
    QThread *thread_{nullptr};
    std::shared_ptr<pppps::poc::CancellationFlag> cancellation_;
    bool running_{false};
};

} // namespace pppps::segmentation

