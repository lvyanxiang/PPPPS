#pragma once

#include "pppps/poc/PocPipeline.h"
#include "pppps/segmentation/SegmentationTypes.h"

#include <QString>

#include <functional>

namespace pppps::segmentation {

struct SegmentationOptions {
    float scoreThreshold{0.70F};
    float maskThreshold{0.50F};
};

class MaskRcnnEngine final {
public:
    using ProgressCallback = std::function<void(int percent, const QString &message)>;

    explicit MaskRcnnEngine(QString modelPath, SegmentationOptions options = {});

    [[nodiscard]] SegmentationResult run(
        const QString &imagePath,
        const pppps::poc::CancellationFlag &cancellation,
        const ProgressCallback &onProgress = {}
    ) const;

    [[nodiscard]] static QStringList classNames();

private:
    QString modelPath_;
    SegmentationOptions options_;
};

} // namespace pppps::segmentation

