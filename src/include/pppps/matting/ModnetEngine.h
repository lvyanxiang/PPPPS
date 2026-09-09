#pragma once

#include "pppps/matting/MattingTypes.h"
#include "pppps/poc/PocPipeline.h"

#include <QString>

#include <functional>

namespace pppps::matting {

class ModnetEngine final {
public:
    using ProgressCallback = std::function<void(int percent, const QString &message)>;

    explicit ModnetEngine(QString modelPath);

    [[nodiscard]] MattingResult run(
        const QString &imagePath,
        const pppps::poc::CancellationFlag &cancellation,
        const ProgressCallback &onProgress = {}
    ) const;

private:
    QString modelPath_;
};

} // namespace pppps::matting
