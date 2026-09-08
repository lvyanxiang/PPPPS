#pragma once

#include <QMetaType>
#include <QSize>
#include <QString>
#include <QStringList>

#include <array>
#include <atomic>
#include <functional>
#include <stdexcept>

namespace pppps::poc {

struct ProbeResult {
    QSize imageSize;
    QString imageFormat;
    std::array<double, 3> meanBgr{};
    QString onnxRuntimeVersion;
    QStringList executionProviders;

    friend bool operator==(const ProbeResult &, const ProbeResult &) = default;
};

class CancellationFlag final {
public:
    void cancel() noexcept;
    [[nodiscard]] bool isCanceled() const noexcept;

private:
    std::atomic_bool canceled_{false};
};

class PocError : public std::runtime_error {
public:
    using std::runtime_error::runtime_error;
};

class TaskCanceled final : public PocError {
public:
    TaskCanceled();
};

class PocPipeline final {
public:
    using ProgressCallback = std::function<void(int percent, const QString &message)>;

    [[nodiscard]] ProbeResult run(
        const QString &filePath,
        const CancellationFlag &cancellation,
        const ProgressCallback &onProgress = {}
    ) const;

    [[nodiscard]] static QStringList supportedImageFormats();
};

} // namespace pppps::poc

Q_DECLARE_METATYPE(pppps::poc::ProbeResult)

