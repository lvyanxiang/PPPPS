#include "pppps/poc/PocPipeline.h"

#include <QByteArray>
#include <QFileInfo>
#include <QImage>
#include <QImageReader>

#include <opencv2/core.hpp>
#include <opencv2/imgproc.hpp>
#include <onnxruntime_cxx_api.h>

#include <array>
#include <cstdint>
#include <string>

namespace pppps::poc {
namespace {

void throwIfCanceled(const CancellationFlag &cancellation)
{
    if (cancellation.isCanceled()) {
        throw TaskCanceled{};
    }
}

void reportProgress(
    const PocPipeline::ProgressCallback &callback,
    const int percent,
    const QString &message
)
{
    if (callback) {
        callback(percent, message);
    }
}

QString normalizedFormat(const QByteArray &format)
{
    const auto value = QString::fromLatin1(format).toLower();
    return value == QStringLiteral("jpg") ? QStringLiteral("jpeg") : value;
}

} // namespace

void CancellationFlag::cancel() noexcept
{
    canceled_.store(true, std::memory_order_release);
}

bool CancellationFlag::isCanceled() const noexcept
{
    return canceled_.load(std::memory_order_acquire);
}

TaskCanceled::TaskCanceled()
    : PocError("POC task canceled")
{
}

ProbeResult PocPipeline::run(
    const QString &filePath,
    const CancellationFlag &cancellation,
    const ProgressCallback &onProgress
) const
{
    throwIfCanceled(cancellation);
    reportProgress(onProgress, 5, QStringLiteral("检查输入文件"));

    const QFileInfo input(filePath);
    if (!input.exists() || !input.isFile()) {
        throw PocError(
            QStringLiteral("输入图片不存在：%1").arg(filePath).toStdString()
        );
    }

    QImageReader reader(filePath);
    reader.setAutoTransform(true);
    if (!reader.canRead()) {
        throw PocError(
            QStringLiteral("无法解码图片：%1").arg(reader.errorString()).toStdString()
        );
    }

    const auto format = normalizedFormat(reader.format());
    if (format != QStringLiteral("jpeg") && format != QStringLiteral("png")) {
        throw PocError(
            QStringLiteral("POC 当前只接受 JPEG/PNG，实际格式：%1").arg(format).toStdString()
        );
    }

    throwIfCanceled(cancellation);
    reportProgress(onProgress, 20, QStringLiteral("Qt 正在解码 JPEG/PNG"));

    QImage image = reader.read();
    if (image.isNull()) {
        throw PocError(
            QStringLiteral("图片解码失败：%1").arg(reader.errorString()).toStdString()
        );
    }

    throwIfCanceled(cancellation);
    reportProgress(onProgress, 45, QStringLiteral("OpenCV 正在执行颜色转换与模糊"));

    image = image.convertToFormat(QImage::Format_RGBA8888);
    const cv::Mat rgba(
        image.height(),
        image.width(),
        CV_8UC4,
        image.bits(),
        static_cast<std::size_t>(image.bytesPerLine())
    );
    cv::Mat bgr;
    cv::cvtColor(rgba, bgr, cv::COLOR_RGBA2BGR);
    cv::GaussianBlur(bgr, bgr, cv::Size(3, 3), 0.0);
    const cv::Scalar mean = cv::mean(bgr);

    throwIfCanceled(cancellation);
    reportProgress(onProgress, 70, QStringLiteral("ONNX Runtime 正在验证 CPU 张量路径"));

    Ort::Env environment(ORT_LOGGING_LEVEL_WARNING, "pppps-poc");
    Ort::SessionOptions sessionOptions;
    sessionOptions.SetIntraOpNumThreads(1);
    sessionOptions.SetGraphOptimizationLevel(GraphOptimizationLevel::ORT_ENABLE_BASIC);

    std::array<float, 4> values{0.0F, 1.0F, 2.0F, 3.0F};
    std::array<std::int64_t, 2> shape{1, 4};
    auto memoryInfo = Ort::MemoryInfo::CreateCpu(OrtArenaAllocator, OrtMemTypeDefault);
    auto tensor = Ort::Value::CreateTensor<float>(
        memoryInfo,
        values.data(),
        values.size(),
        shape.data(),
        shape.size()
    );
    if (!tensor.IsTensor() || tensor.GetTensorTypeAndShapeInfo().GetElementCount() != values.size()) {
        throw PocError("ONNX Runtime CPU tensor probe failed");
    }

    QStringList providers;
    for (const std::string &provider : Ort::GetAvailableProviders()) {
        providers.push_back(QString::fromStdString(provider));
    }

    throwIfCanceled(cancellation);
    reportProgress(onProgress, 100, QStringLiteral("POC 依赖探针完成"));

    return ProbeResult{
        .imageSize = image.size(),
        .imageFormat = format,
        .meanBgr = {mean[0], mean[1], mean[2]},
        .onnxRuntimeVersion = QString::fromLatin1(OrtGetApiBase()->GetVersionString()),
        .executionProviders = providers,
    };
}

QStringList PocPipeline::supportedImageFormats()
{
    QStringList formats;
    for (const QByteArray &format : QImageReader::supportedImageFormats()) {
        formats.push_back(normalizedFormat(format));
    }
    formats.removeDuplicates();
    formats.sort();
    return formats;
}

} // namespace pppps::poc

