#include "pppps/matting/MattingTypes.h"
#include "pppps/matting/ModnetEngine.h"
#include "pppps/poc/PocPipeline.h"
#include "pppps/segmentation/MaskRcnnEngine.h"

#include <QCommandLineParser>
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QImageReader>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTextStream>

#include <algorithm>
#include <exception>
#include <optional>

namespace {

QJsonObject statisticsToJson(const pppps::matting::AlphaStatistics &statistics)
{
    return {
        {QStringLiteral("transparent_pixels"), static_cast<double>(statistics.transparentPixels)},
        {QStringLiteral("soft_pixels"), static_cast<double>(statistics.softPixels)},
        {QStringLiteral("opaque_pixels"), static_cast<double>(statistics.opaquePixels)},
        {QStringLiteral("mean_alpha"), statistics.meanAlpha},
    };
}

QJsonObject comparisonToJson(const pppps::matting::MaskAlphaComparison &comparison)
{
    return {
        {QStringLiteral("compared_pixels"), static_cast<double>(comparison.comparedPixels)},
        {QStringLiteral("disagreement_pixels"), static_cast<double>(comparison.disagreementPixels)},
        {QStringLiteral("threshold_iou"), comparison.thresholdIoU},
        {QStringLiteral("mean_absolute_difference"), comparison.meanAbsoluteDifference},
    };
}

bool saveArtifacts(
    const QString &outputDirectory,
    const pppps::matting::MattingResult &result,
    const QJsonDocument &json
)
{
    QDir directory;
    if (!directory.mkpath(outputDirectory)) {
        return false;
    }
    directory.setPath(outputDirectory);

    QFile resultFile(directory.filePath(QStringLiteral("result.json")));
    if (!resultFile.open(QIODevice::WriteOnly | QIODevice::Truncate)
        || resultFile.write(json.toJson(QJsonDocument::Indented)) < 0) {
        return false;
    }
    return result.alphaMatte.save(directory.filePath(QStringLiteral("alpha.png")), "PNG")
        && pppps::matting::composeOnCheckerboard(result.sourceImage, result.alphaMatte)
               .save(directory.filePath(QStringLiteral("checkerboard.png")), "PNG");
}

} // namespace

int main(int argc, char *argv[])
{
    QCoreApplication application(argc, argv);
    QCoreApplication::setApplicationName(QStringLiteral("pppps_matting_probe"));

    QCommandLineParser parser;
    parser.setApplicationDescription(QStringLiteral("运行 PPPPS MODNet 真实人物 Alpha 推理并导出证据。"));
    parser.addHelpOption();
    const QCommandLineOption modelOption(
        {QStringLiteral("m"), QStringLiteral("model")},
        QStringLiteral("MODNet ONNX 模型路径。"),
        QStringLiteral("path")
    );
    const QCommandLineOption imageOption(
        {QStringLiteral("i"), QStringLiteral("image")},
        QStringLiteral("待抠图的 JPEG/PNG 路径。"),
        QStringLiteral("path")
    );
    const QCommandLineOption outputOption(
        {QStringLiteral("o"), QStringLiteral("output-dir")},
        QStringLiteral("可选的 JSON、Alpha 与棋盘格合成图输出目录。"),
        QStringLiteral("directory")
    );
    const QCommandLineOption binaryMaskOption(
        QStringLiteral("binary-mask"),
        QStringLiteral("可选的同尺寸二值 Mask，用于和连续 Alpha 对比。"),
        QStringLiteral("path")
    );
    const QCommandLineOption maskModelOption(
        QStringLiteral("mask-model"),
        QStringLiteral("可选的 Mask R-CNN 模型；自动合并 person Mask 后与 Alpha 对比。"),
        QStringLiteral("path")
    );
    const QCommandLineOption maskLabelOption(
        QStringLiteral("mask-label"),
        QStringLiteral("与 Alpha 对比的 Mask R-CNN 类别，默认 person。"),
        QStringLiteral("class"),
        QStringLiteral("person")
    );
    const QCommandLineOption expectedSoftPixelsOption(
        QStringLiteral("expect-min-soft-pixels"),
        QStringLiteral("软透明像素少于该值时返回失败，用于真实模型测试。"),
        QStringLiteral("count")
    );
    parser.addOptions(
        {
            modelOption,
            imageOption,
            outputOption,
            binaryMaskOption,
            maskModelOption,
            maskLabelOption,
            expectedSoftPixelsOption,
        }
    );
    parser.process(application);

    if (!parser.isSet(modelOption) || !parser.isSet(imageOption)) {
        QTextStream(stderr) << "错误：--model 与 --image 都是必填参数。\n";
        return 2;
    }

    try {
        pppps::poc::CancellationFlag cancellation;
        const pppps::matting::ModnetEngine engine(parser.value(modelOption));
        const auto result = engine.run(parser.value(imageOption), cancellation);
        QJsonObject root{
            {QStringLiteral("model"), result.modelName},
            {QStringLiteral("execution_provider"), result.executionProvider},
            {QStringLiteral("image_width"), result.sourceImage.width()},
            {QStringLiteral("image_height"), result.sourceImage.height()},
            {QStringLiteral("model_load_ms"), static_cast<double>(result.modelLoadMs)},
            {QStringLiteral("preprocessing_ms"), static_cast<double>(result.preprocessingMs)},
            {QStringLiteral("inference_ms"), static_cast<double>(result.inferenceMs)},
            {QStringLiteral("postprocessing_ms"), static_cast<double>(result.postprocessingMs)},
            {QStringLiteral("total_ms"), static_cast<double>(result.totalMs)},
            {QStringLiteral("alpha"), statisticsToJson(result.statistics)},
        };

        std::optional<QImage> binaryMask;
        if (parser.isSet(binaryMaskOption)) {
            QImageReader reader(parser.value(binaryMaskOption));
            const auto mask = reader.read();
            if (mask.isNull()) {
                QTextStream(stderr) << "错误：无法读取 --binary-mask。\n";
                return 2;
            }
            binaryMask = mask.convertToFormat(QImage::Format_Grayscale8);
        } else if (parser.isSet(maskModelOption)) {
            const pppps::segmentation::MaskRcnnEngine maskEngine(parser.value(maskModelOption));
            const auto segmentation = maskEngine.run(parser.value(imageOption), cancellation);
            QImage unionMask(result.sourceImage.size(), QImage::Format_Grayscale8);
            unionMask.fill(0);
            const auto maskLabel = parser.value(maskLabelOption);
            int maskCount = 0;
            for (const auto &object : segmentation.objects) {
                if (object.className != maskLabel) {
                    continue;
                }
                ++maskCount;
                for (int y = 0; y < unionMask.height(); ++y) {
                    auto *destination = unionMask.scanLine(y);
                    const auto *source = object.mask.constScanLine(y);
                    for (int x = 0; x < unionMask.width(); ++x) {
                        destination[x] = std::max(destination[x], source[x]);
                    }
                }
            }
            root.insert(QStringLiteral("binary_mask_label"), maskLabel);
            root.insert(QStringLiteral("binary_mask_count"), maskCount);
            if (maskCount > 0) {
                binaryMask = std::move(unionMask);
            }
        }
        if (binaryMask) {
            root.insert(
                QStringLiteral("binary_mask_comparison"),
                comparisonToJson(
                    pppps::matting::compareBinaryMaskWithAlpha(*binaryMask, result.alphaMatte)
                )
            );
        }

        const QJsonDocument json(root);
        QTextStream(stdout) << json.toJson(QJsonDocument::Compact) << '\n';
        if (parser.isSet(outputOption)
            && !saveArtifacts(parser.value(outputOption), result, json)) {
            QTextStream(stderr) << "错误：无法写入人物抠图 POC 结果目录。\n";
            return 3;
        }
        if (binaryMask && parser.isSet(outputOption)
            && !binaryMask->save(
                QDir(parser.value(outputOption)).filePath(QStringLiteral("binary-reference-mask.png")),
                "PNG"
            )) {
            QTextStream(stderr) << "错误：无法写入人物二值 Mask。\n";
            return 3;
        }

        if (parser.isSet(expectedSoftPixelsOption)) {
            bool parsed = false;
            const auto minimum = parser.value(expectedSoftPixelsOption).toLongLong(&parsed);
            if (!parsed || minimum < 0) {
                QTextStream(stderr) << "错误：--expect-min-soft-pixels 必须是非负整数。\n";
                return 2;
            }
            if (result.statistics.softPixels < minimum) {
                QTextStream(stderr) << "错误：真实 Alpha 的软透明像素少于预期。\n";
                return 4;
            }
        }
        return 0;
    } catch (const std::exception &error) {
        QTextStream(stderr) << "错误：" << error.what() << '\n';
        return 3;
    }
}
