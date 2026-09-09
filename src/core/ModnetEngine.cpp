#include "pppps/matting/ModnetEngine.h"

#include <QElapsedTimer>
#include <QFileInfo>
#include <QImageReader>

#include <opencv2/core.hpp>
#include <opencv2/imgproc.hpp>
#include <onnxruntime_cxx_api.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <string>
#include <thread>
#include <utility>
#include <vector>

namespace pppps::matting {
namespace {

constexpr int kReferenceSize = 512;

void throwIfCanceled(const pppps::poc::CancellationFlag &cancellation)
{
    if (cancellation.isCanceled()) {
        throw pppps::poc::TaskCanceled{};
    }
}

void reportProgress(
    const ModnetEngine::ProgressCallback &callback,
    const int percent,
    const QString &message
)
{
    if (callback) {
        callback(percent, message);
    }
}

struct PreprocessedImage {
    std::vector<float> values;
    std::array<std::int64_t, 4> shape{};
};

QSize modelInputSize(const QSize &sourceSize)
{
    auto width = sourceSize.width();
    auto height = sourceSize.height();
    if (std::max(width, height) < kReferenceSize || std::min(width, height) > kReferenceSize) {
        if (width >= height) {
            width = static_cast<int>(
                static_cast<double>(sourceSize.width()) / sourceSize.height() * kReferenceSize
            );
            height = kReferenceSize;
        } else {
            height = static_cast<int>(
                static_cast<double>(sourceSize.height()) / sourceSize.width() * kReferenceSize
            );
            width = kReferenceSize;
        }
    }
    width = std::max(32, width - (width % 32));
    height = std::max(32, height - (height % 32));
    return {width, height};
}

PreprocessedImage preprocess(const QImage &source)
{
    const auto rgb = source.convertToFormat(QImage::Format_RGB888);
    const cv::Mat rgbView(
        rgb.height(),
        rgb.width(),
        CV_8UC3,
        const_cast<uchar *>(rgb.constBits()),
        static_cast<std::size_t>(rgb.bytesPerLine())
    );
    const auto inputSize = modelInputSize(source.size());
    cv::Mat resized;
    cv::resize(rgbView, resized, cv::Size(inputSize.width(), inputSize.height()), 0.0, 0.0, cv::INTER_AREA);

    const auto planeSize = static_cast<std::size_t>(inputSize.width()) * inputSize.height();
    std::vector<float> values(planeSize * 3);
    for (int y = 0; y < inputSize.height(); ++y) {
        const auto *row = resized.ptr<cv::Vec3b>(y);
        for (int x = 0; x < inputSize.width(); ++x) {
            const auto offset = static_cast<std::size_t>(y) * inputSize.width() + x;
            values[offset] = (static_cast<float>(row[x][0]) - 127.5F) / 127.5F;
            values[planeSize + offset] = (static_cast<float>(row[x][1]) - 127.5F) / 127.5F;
            values[(planeSize * 2) + offset] = (static_cast<float>(row[x][2]) - 127.5F) / 127.5F;
        }
    }
    return PreprocessedImage{
        .values = std::move(values),
        .shape = {1, 3, inputSize.height(), inputSize.width()},
    };
}

QImage postprocess(const Ort::Value &output, const QSize &sourceSize)
{
    if (!output.IsTensor()) {
        throw pppps::poc::PocError("MODNet output is not a tensor");
    }
    const auto shape = output.GetTensorTypeAndShapeInfo().GetShape();
    if (shape.size() != 4 || shape[0] != 1 || shape[1] != 1 || shape[2] <= 0 || shape[3] <= 0) {
        throw pppps::poc::PocError("MODNet output tensor shape is incompatible");
    }

    const auto outputHeight = static_cast<int>(shape[2]);
    const auto outputWidth = static_cast<int>(shape[3]);
    const cv::Mat matteView(
        outputHeight,
        outputWidth,
        CV_32FC1,
        const_cast<float *>(output.GetTensorData<float>())
    );
    cv::Mat resized;
    cv::resize(
        matteView,
        resized,
        cv::Size(sourceSize.width(), sourceSize.height()),
        0.0,
        0.0,
        cv::INTER_AREA
    );

    QImage alpha(sourceSize, QImage::Format_Grayscale8);
    for (int y = 0; y < alpha.height(); ++y) {
        auto *destination = alpha.scanLine(y);
        const auto *source = resized.ptr<float>(y);
        for (int x = 0; x < alpha.width(); ++x) {
            destination[x] = static_cast<uchar>(std::clamp(
                static_cast<int>(std::lround(source[x] * 255.0F)),
                0,
                255
            ));
        }
    }
    return alpha;
}

} // namespace

ModnetEngine::ModnetEngine(QString modelPath)
    : modelPath_(std::move(modelPath))
{
}

MattingResult ModnetEngine::run(
    const QString &imagePath,
    const pppps::poc::CancellationFlag &cancellation,
    const ProgressCallback &onProgress
) const
{
    QElapsedTimer totalTimer;
    totalTimer.start();
    throwIfCanceled(cancellation);
    reportProgress(onProgress, 5, QStringLiteral("检查人物图片与 MODNet 模型"));

    const QFileInfo modelInfo(modelPath_);
    if (!modelInfo.exists() || !modelInfo.isFile()) {
        throw pppps::poc::PocError(
            QStringLiteral("找不到人物抠图模型：%1。请在项目根目录运行 make models。")
                .arg(modelPath_)
                .toStdString()
        );
    }

    QImageReader reader(imagePath);
    reader.setAutoTransform(true);
    if (!reader.canRead()) {
        throw pppps::poc::PocError(
            QStringLiteral("无法解码图片：%1").arg(reader.errorString()).toStdString()
        );
    }
    const auto sourceImage = reader.read();
    if (sourceImage.isNull()) {
        throw pppps::poc::PocError(
            QStringLiteral("图片解码失败：%1").arg(reader.errorString()).toStdString()
        );
    }

    throwIfCanceled(cancellation);
    reportProgress(onProgress, 15, QStringLiteral("加载 MODNet ONNX 人物抠图模型"));
    QElapsedTimer stageTimer;
    stageTimer.start();

    try {
        Ort::Env environment(ORT_LOGGING_LEVEL_WARNING, "pppps-modnet");
        Ort::SessionOptions sessionOptions;
        const auto hardwareThreads = std::max(1U, std::thread::hardware_concurrency());
        sessionOptions.SetIntraOpNumThreads(static_cast<int>(std::min(4U, hardwareThreads)));
        sessionOptions.SetInterOpNumThreads(1);
        sessionOptions.SetGraphOptimizationLevel(GraphOptimizationLevel::ORT_ENABLE_ALL);

        const auto nativeModelPath = std::filesystem::path(modelPath_.toStdString());
        Ort::Session session(environment, nativeModelPath.c_str(), sessionOptions);
        const auto modelLoadMs = stageTimer.elapsed();

        throwIfCanceled(cancellation);
        reportProgress(onProgress, 30, QStringLiteral("OpenCV 缩放并归一化 RGB 图片"));
        stageTimer.restart();
        auto input = preprocess(sourceImage);
        const auto preprocessingMs = stageTimer.elapsed();

        throwIfCanceled(cancellation);
        reportProgress(onProgress, 45, QStringLiteral("ONNX Runtime CPU 正在计算连续 Alpha"));
        auto memoryInfo = Ort::MemoryInfo::CreateCpu(OrtArenaAllocator, OrtMemTypeDefault);
        auto inputTensor = Ort::Value::CreateTensor<float>(
            memoryInfo,
            input.values.data(),
            input.values.size(),
            input.shape.data(),
            input.shape.size()
        );

        Ort::AllocatorWithDefaultOptions allocator;
        auto inputName = session.GetInputNameAllocated(0, allocator);
        auto outputName = session.GetOutputNameAllocated(0, allocator);
        const std::array<const char *, 1> inputNames{inputName.get()};
        const std::array<const char *, 1> outputNames{outputName.get()};
        Ort::RunOptions runOptions;
        stageTimer.restart();
        const auto outputs = session.Run(
            runOptions,
            inputNames.data(),
            &inputTensor,
            inputNames.size(),
            outputNames.data(),
            outputNames.size()
        );
        const auto inferenceMs = stageTimer.elapsed();
        if (outputs.size() != 1) {
            throw pppps::poc::PocError("MODNet should return exactly one alpha tensor");
        }

        throwIfCanceled(cancellation);
        reportProgress(onProgress, 88, QStringLiteral("生成原图尺寸的灰度 Alpha"));
        stageTimer.restart();
        auto alphaMatte = postprocess(outputs.front(), sourceImage.size());
        const auto statistics = analyzeAlpha(alphaMatte);
        const auto postprocessingMs = stageTimer.elapsed();
        reportProgress(onProgress, 100, QStringLiteral("人物 Alpha 已生成，可补回或擦除"));

        return MattingResult{
            .sourceImage = sourceImage,
            .alphaMatte = std::move(alphaMatte),
            .statistics = statistics,
            .modelName = modelInfo.completeBaseName(),
            .executionProvider = QStringLiteral("CPUExecutionProvider"),
            .modelLoadMs = modelLoadMs,
            .preprocessingMs = preprocessingMs,
            .inferenceMs = inferenceMs,
            .postprocessingMs = postprocessingMs,
            .totalMs = totalTimer.elapsed(),
        };
    } catch (const Ort::Exception &error) {
        throw pppps::poc::PocError(
            QStringLiteral("MODNet ONNX 推理失败：%1").arg(QString::fromUtf8(error.what())).toStdString()
        );
    }
}

} // namespace pppps::matting
