#include "pppps/segmentation/MaskRcnnEngine.h"

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

namespace pppps::segmentation {
namespace {

constexpr int kModelShortSide = 800;
constexpr int kModelLongSide = 1333;
constexpr int kMaskSize = 28;

void throwIfCanceled(const pppps::poc::CancellationFlag &cancellation)
{
    if (cancellation.isCanceled()) {
        throw pppps::poc::TaskCanceled{};
    }
}

void reportProgress(
    const MaskRcnnEngine::ProgressCallback &callback,
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
    std::array<std::int64_t, 3> shape{};
    float scale{1.0F};
};

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
    cv::Mat bgr;
    cv::cvtColor(rgbView, bgr, cv::COLOR_RGB2BGR);

    const auto shortSide = std::min(source.width(), source.height());
    const auto longSide = std::max(source.width(), source.height());
    const auto shortScale = static_cast<float>(kModelShortSide) / static_cast<float>(shortSide);
    const auto longScale = static_cast<float>(kModelLongSide) / static_cast<float>(longSide);
    const auto scale = std::min(shortScale, longScale);
    const auto resizedWidth = std::max(1, static_cast<int>(std::round(source.width() * scale)));
    const auto resizedHeight = std::max(1, static_cast<int>(std::round(source.height() * scale)));

    cv::Mat resized;
    cv::resize(bgr, resized, cv::Size(resizedWidth, resizedHeight), 0.0, 0.0, cv::INTER_LINEAR);

    const auto paddedWidth = ((resizedWidth + 31) / 32) * 32;
    const auto paddedHeight = ((resizedHeight + 31) / 32) * 32;
    const auto planeSize = static_cast<std::size_t>(paddedWidth) * paddedHeight;
    std::vector<float> values(planeSize * 3, 0.0F);
    constexpr std::array<float, 3> means{102.9801F, 115.9465F, 122.7717F};

    for (int y = 0; y < resizedHeight; ++y) {
        const auto *row = resized.ptr<cv::Vec3b>(y);
        for (int x = 0; x < resizedWidth; ++x) {
            const auto offset = static_cast<std::size_t>(y) * paddedWidth + x;
            values[offset] = static_cast<float>(row[x][0]) - means[0];
            values[planeSize + offset] = static_cast<float>(row[x][1]) - means[1];
            values[(planeSize * 2) + offset] = static_cast<float>(row[x][2]) - means[2];
        }
    }

    return PreprocessedImage{
        .values = std::move(values),
        .shape = {3, paddedHeight, paddedWidth},
        .scale = scale,
    };
}

QVector<QPolygonF> extractContours(const QImage &mask)
{
    const cv::Mat maskView(
        mask.height(),
        mask.width(),
        CV_8UC1,
        const_cast<uchar *>(mask.constBits()),
        static_cast<std::size_t>(mask.bytesPerLine())
    );
    std::vector<std::vector<cv::Point>> cvContours;
    cv::findContours(maskView.clone(), cvContours, cv::RETR_EXTERNAL, cv::CHAIN_APPROX_SIMPLE);

    QVector<QPolygonF> contours;
    contours.reserve(static_cast<qsizetype>(cvContours.size()));
    for (const auto &cvContour : cvContours) {
        if (cvContour.size() < 3) {
            continue;
        }
        QPolygonF contour;
        contour.reserve(static_cast<qsizetype>(cvContour.size()));
        for (const auto &point : cvContour) {
            contour.append(QPointF(point.x, point.y));
        }
        contours.append(std::move(contour));
    }
    return contours;
}

DetectedObject createObject(
    const int objectId,
    const int classId,
    const float confidence,
    const float *box,
    const float *smallMask,
    const float scale,
    const QSize &sourceSize,
    const float maskThreshold,
    const QStringList &classes
)
{
    const auto unscaledLeft = static_cast<int>(std::floor(box[0] / scale));
    const auto unscaledTop = static_cast<int>(std::floor(box[1] / scale));
    const auto unscaledRight = static_cast<int>(std::ceil(box[2] / scale));
    const auto unscaledBottom = static_cast<int>(std::ceil(box[3] / scale));
    const auto fullWidth = std::max(1, unscaledRight - unscaledLeft + 1);
    const auto fullHeight = std::max(1, unscaledBottom - unscaledTop + 1);

    const cv::Mat smallMaskView(kMaskSize, kMaskSize, CV_32FC1, const_cast<float *>(smallMask));
    cv::Mat resizedMask;
    cv::resize(
        smallMaskView,
        resizedMask,
        cv::Size(fullWidth, fullHeight),
        0.0,
        0.0,
        cv::INTER_LINEAR
    );

    QImage mask(sourceSize, QImage::Format_Grayscale8);
    mask.fill(0);
    qsizetype maskArea = 0;
    const auto left = std::clamp(unscaledLeft, 0, sourceSize.width());
    const auto top = std::clamp(unscaledTop, 0, sourceSize.height());
    const auto right = std::clamp(unscaledRight + 1, 0, sourceSize.width());
    const auto bottom = std::clamp(unscaledBottom + 1, 0, sourceSize.height());

    for (int y = top; y < bottom; ++y) {
        auto *destination = mask.scanLine(y);
        const auto *sourceRow = resizedMask.ptr<float>(y - unscaledTop);
        for (int x = left; x < right; ++x) {
            if (sourceRow[x - unscaledLeft] > maskThreshold) {
                destination[x] = 255;
                ++maskArea;
            }
        }
    }

    const auto className = classId >= 0 && classId < classes.size()
        ? classes[classId]
        : QStringLiteral("unknown-%1").arg(classId);
    const QRectF boundingBox(
        QPointF(box[0] / scale, box[1] / scale),
        QPointF(box[2] / scale, box[3] / scale)
    );

    return DetectedObject{
        .id = objectId,
        .classId = classId,
        .className = className,
        .confidence = confidence,
        .boundingBox = boundingBox.normalized(),
        .mask = mask,
        .contours = extractContours(mask),
        .maskArea = maskArea,
    };
}

void validateOutputs(const std::vector<Ort::Value> &outputs)
{
    if (outputs.size() != 4) {
        throw pppps::poc::PocError(
            QStringLiteral("Mask R-CNN 应返回 4 个输出，实际为 %1").arg(outputs.size()).toStdString()
        );
    }
    for (const auto &output : outputs) {
        if (!output.IsTensor()) {
            throw pppps::poc::PocError("Mask R-CNN output is not a tensor");
        }
    }

    const auto boxesShape = outputs[0].GetTensorTypeAndShapeInfo().GetShape();
    const auto labelsShape = outputs[1].GetTensorTypeAndShapeInfo().GetShape();
    const auto scoresShape = outputs[2].GetTensorTypeAndShapeInfo().GetShape();
    const auto masksShape = outputs[3].GetTensorTypeAndShapeInfo().GetShape();
    if (boxesShape.size() != 2 || boxesShape[1] != 4 || labelsShape.size() != 1
        || scoresShape.size() != 1 || masksShape.size() != 4 || masksShape[1] != 1
        || masksShape[2] != kMaskSize || masksShape[3] != kMaskSize
        || boxesShape[0] != labelsShape[0] || boxesShape[0] != scoresShape[0]
        || boxesShape[0] != masksShape[0]) {
        throw pppps::poc::PocError("Mask R-CNN output tensor shapes are incompatible");
    }
}

} // namespace

MaskRcnnEngine::MaskRcnnEngine(QString modelPath, const SegmentationOptions options)
    : modelPath_(std::move(modelPath))
    , options_(options)
{
    if (options_.scoreThreshold < 0.0F || options_.scoreThreshold > 1.0F
        || options_.maskThreshold < 0.0F || options_.maskThreshold > 1.0F) {
        throw pppps::poc::PocError("Segmentation thresholds must be between 0 and 1");
    }
}

SegmentationResult MaskRcnnEngine::run(
    const QString &imagePath,
    const pppps::poc::CancellationFlag &cancellation,
    const ProgressCallback &onProgress
) const
{
    QElapsedTimer totalTimer;
    totalTimer.start();
    throwIfCanceled(cancellation);
    reportProgress(onProgress, 5, QStringLiteral("检查图片与模型"));

    const QFileInfo modelInfo(modelPath_);
    if (!modelInfo.exists() || !modelInfo.isFile()) {
        throw pppps::poc::PocError(
            QStringLiteral("找不到实例分割模型：%1。请在项目根目录运行 make models。")
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
    reportProgress(onProgress, 15, QStringLiteral("加载 ONNX 实例分割模型"));
    QElapsedTimer stageTimer;
    stageTimer.start();

    try {
        Ort::Env environment(ORT_LOGGING_LEVEL_WARNING, "pppps-mask-rcnn");
        Ort::SessionOptions sessionOptions;
        const auto hardwareThreads = std::max(1U, std::thread::hardware_concurrency());
        sessionOptions.SetIntraOpNumThreads(static_cast<int>(std::min(4U, hardwareThreads)));
        sessionOptions.SetInterOpNumThreads(1);
        sessionOptions.SetGraphOptimizationLevel(GraphOptimizationLevel::ORT_ENABLE_ALL);

        const auto nativeModelPath = std::filesystem::path(modelPath_.toStdString());
        Ort::Session session(environment, nativeModelPath.c_str(), sessionOptions);
        const auto modelLoadMs = stageTimer.elapsed();

        throwIfCanceled(cancellation);
        reportProgress(onProgress, 30, QStringLiteral("OpenCV 预处理图片"));
        stageTimer.restart();
        auto input = preprocess(sourceImage);
        const auto preprocessingMs = stageTimer.elapsed();

        throwIfCanceled(cancellation);
        reportProgress(onProgress, 40, QStringLiteral("ONNX Runtime CPU 正在识别并生成 Mask"));
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
        std::vector<Ort::AllocatedStringPtr> allocatedOutputNames;
        std::vector<const char *> outputNames;
        const auto outputCount = session.GetOutputCount();
        allocatedOutputNames.reserve(outputCount);
        outputNames.reserve(outputCount);
        for (std::size_t index = 0; index < outputCount; ++index) {
            allocatedOutputNames.push_back(session.GetOutputNameAllocated(index, allocator));
            outputNames.push_back(allocatedOutputNames.back().get());
        }

        const std::array<const char *, 1> inputNames{inputName.get()};
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

        throwIfCanceled(cancellation);
        reportProgress(onProgress, 85, QStringLiteral("把模型输出转换为可点击对象 Mask"));
        stageTimer.restart();
        validateOutputs(outputs);

        const auto objectCount = outputs[0].GetTensorTypeAndShapeInfo().GetShape()[0];
        const auto *boxes = outputs[0].GetTensorData<float>();
        const auto *labels = outputs[1].GetTensorData<std::int64_t>();
        const auto *scores = outputs[2].GetTensorData<float>();
        const auto *masks = outputs[3].GetTensorData<float>();
        const auto classes = classNames();
        QVector<DetectedObject> objects;
        objects.reserve(static_cast<qsizetype>(objectCount));
        for (std::int64_t index = 0; index < objectCount; ++index) {
            if (scores[index] < options_.scoreThreshold) {
                continue;
            }
            auto object = createObject(
                objects.size() + 1,
                static_cast<int>(labels[index]),
                scores[index],
                boxes + (index * 4),
                masks + (index * kMaskSize * kMaskSize),
                input.scale,
                sourceImage.size(),
                options_.maskThreshold,
                classes
            );
            if (object.maskArea > 0) {
                objects.append(std::move(object));
            }
        }
        const auto postprocessingMs = stageTimer.elapsed();
        reportProgress(onProgress, 100, QStringLiteral("已生成可悬停、可点击的对象 Mask"));

        return SegmentationResult{
            .sourceImage = sourceImage,
            .objects = std::move(objects),
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
            QStringLiteral("ONNX Runtime 推理失败：%1").arg(QString::fromUtf8(error.what())).toStdString()
        );
    }
}

QStringList MaskRcnnEngine::classNames()
{
    return {
        QStringLiteral("__background"), QStringLiteral("person"), QStringLiteral("bicycle"),
        QStringLiteral("car"), QStringLiteral("motorcycle"), QStringLiteral("airplane"),
        QStringLiteral("bus"), QStringLiteral("train"), QStringLiteral("truck"),
        QStringLiteral("boat"), QStringLiteral("traffic light"), QStringLiteral("fire hydrant"),
        QStringLiteral("stop sign"), QStringLiteral("parking meter"), QStringLiteral("bench"),
        QStringLiteral("bird"), QStringLiteral("cat"), QStringLiteral("dog"),
        QStringLiteral("horse"), QStringLiteral("sheep"), QStringLiteral("cow"),
        QStringLiteral("elephant"), QStringLiteral("bear"), QStringLiteral("zebra"),
        QStringLiteral("giraffe"), QStringLiteral("backpack"), QStringLiteral("umbrella"),
        QStringLiteral("handbag"), QStringLiteral("tie"), QStringLiteral("suitcase"),
        QStringLiteral("frisbee"), QStringLiteral("skis"), QStringLiteral("snowboard"),
        QStringLiteral("sports ball"), QStringLiteral("kite"), QStringLiteral("baseball bat"),
        QStringLiteral("baseball glove"), QStringLiteral("skateboard"), QStringLiteral("surfboard"),
        QStringLiteral("tennis racket"), QStringLiteral("bottle"), QStringLiteral("wine glass"),
        QStringLiteral("cup"), QStringLiteral("fork"), QStringLiteral("knife"),
        QStringLiteral("spoon"), QStringLiteral("bowl"), QStringLiteral("banana"),
        QStringLiteral("apple"), QStringLiteral("sandwich"), QStringLiteral("orange"),
        QStringLiteral("broccoli"), QStringLiteral("carrot"), QStringLiteral("hot dog"),
        QStringLiteral("pizza"), QStringLiteral("donut"), QStringLiteral("cake"),
        QStringLiteral("chair"), QStringLiteral("couch"), QStringLiteral("potted plant"),
        QStringLiteral("bed"), QStringLiteral("dining table"), QStringLiteral("toilet"),
        QStringLiteral("tv"), QStringLiteral("laptop"), QStringLiteral("mouse"),
        QStringLiteral("remote"), QStringLiteral("keyboard"), QStringLiteral("cell phone"),
        QStringLiteral("microwave"), QStringLiteral("oven"), QStringLiteral("toaster"),
        QStringLiteral("sink"), QStringLiteral("refrigerator"), QStringLiteral("book"),
        QStringLiteral("clock"), QStringLiteral("vase"), QStringLiteral("scissors"),
        QStringLiteral("teddy bear"), QStringLiteral("hair drier"), QStringLiteral("toothbrush"),
    };
}

} // namespace pppps::segmentation

