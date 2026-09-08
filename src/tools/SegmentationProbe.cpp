#include "pppps/poc/PocPipeline.h"
#include "pppps/segmentation/MaskRcnnEngine.h"
#include "pppps/segmentation/SegmentationTypes.h"

#include <QCommandLineParser>
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QPainter>
#include <QRegularExpression>
#include <QTextStream>

#include <exception>
#include <algorithm>

namespace {

QString safeFilePart(QString value)
{
    value.replace(QRegularExpression(QStringLiteral("[^A-Za-z0-9_-]+")), QStringLiteral("-"));
    return value;
}

QJsonObject objectToJson(const pppps::segmentation::DetectedObject &object)
{
    const auto &box = object.boundingBox;
    return QJsonObject{
        {QStringLiteral("id"), object.id},
        {QStringLiteral("class_id"), object.classId},
        {QStringLiteral("class_name"), object.className},
        {QStringLiteral("confidence"), object.confidence},
        {QStringLiteral("mask_area_px"), static_cast<double>(object.maskArea)},
        {QStringLiteral("box"), QJsonArray{box.left(), box.top(), box.right(), box.bottom()}},
    };
}

QJsonObject resultToJson(const pppps::segmentation::SegmentationResult &result)
{
    QJsonArray objects;
    for (const auto &object : result.objects) {
        objects.append(objectToJson(object));
    }
    return QJsonObject{
        {QStringLiteral("model"), result.modelName},
        {QStringLiteral("execution_provider"), result.executionProvider},
        {QStringLiteral("image_width"), result.sourceImage.width()},
        {QStringLiteral("image_height"), result.sourceImage.height()},
        {QStringLiteral("object_count"), result.objects.size()},
        {QStringLiteral("model_load_ms"), static_cast<double>(result.modelLoadMs)},
        {QStringLiteral("preprocessing_ms"), static_cast<double>(result.preprocessingMs)},
        {QStringLiteral("inference_ms"), static_cast<double>(result.inferenceMs)},
        {QStringLiteral("postprocessing_ms"), static_cast<double>(result.postprocessingMs)},
        {QStringLiteral("total_ms"), static_cast<double>(result.totalMs)},
        {QStringLiteral("objects"), objects},
    };
}

bool saveArtifacts(
    const QString &outputDirectory,
    const pppps::segmentation::SegmentationResult &result,
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

    QImage overlay = result.sourceImage.convertToFormat(QImage::Format_ARGB32_Premultiplied);
    QPainter painter(&overlay);
    const QList<QColor> colors{
        QColor(0, 180, 255), QColor(255, 190, 0), QColor(255, 70, 120), QColor(80, 220, 130),
    };
    for (qsizetype index = 0; index < result.objects.size(); ++index) {
        const auto &object = result.objects[index];
        const auto color = colors[index % colors.size()];
        QImage tint(object.mask.size(), QImage::Format_ARGB32_Premultiplied);
        tint.fill(color);
        tint.setAlphaChannel(object.mask);
        painter.save();
        painter.setOpacity(0.28);
        painter.drawImage(QPoint(0, 0), tint);
        painter.restore();
        QPen pen(color);
        pen.setWidth(3);
        painter.setPen(pen);
        painter.setBrush(Qt::NoBrush);
        for (const auto &contour : object.contours) {
            painter.drawPolygon(contour);
        }

        const auto maskName = QStringLiteral("mask-%1-%2.png")
                                  .arg(object.id, 3, 10, QLatin1Char('0'))
                                  .arg(safeFilePart(object.className));
        if (!object.mask.save(directory.filePath(maskName), "PNG")) {
            return false;
        }
    }
    painter.end();
    return overlay.save(directory.filePath(QStringLiteral("overlay.png")), "PNG");
}

} // namespace

int main(int argc, char *argv[])
{
    QCoreApplication application(argc, argv);
    QCoreApplication::setApplicationName(QStringLiteral("pppps_segmentation_probe"));

    QCommandLineParser parser;
    parser.setApplicationDescription(QStringLiteral("运行 PPPPS Mask R-CNN 真实推理并导出 POC 证据。"));
    parser.addHelpOption();
    const QCommandLineOption modelOption(
        {QStringLiteral("m"), QStringLiteral("model")},
        QStringLiteral("Mask R-CNN ONNX 模型路径。"),
        QStringLiteral("path")
    );
    const QCommandLineOption imageOption(
        {QStringLiteral("i"), QStringLiteral("image")},
        QStringLiteral("待识别的 JPEG/PNG 路径。"),
        QStringLiteral("path")
    );
    const QCommandLineOption outputOption(
        {QStringLiteral("o"), QStringLiteral("output-dir")},
        QStringLiteral("可选的 JSON、Mask 与叠加图输出目录。"),
        QStringLiteral("directory")
    );
    const QCommandLineOption expectedLabelOption(
        QStringLiteral("expect-label"),
        QStringLiteral("若未检测到该类别则返回失败，用于真实模型集成测试。"),
        QStringLiteral("class")
    );
    const QCommandLineOption probeXOption(
        QStringLiteral("probe-x"),
        QStringLiteral("点击探针的归一化 X 坐标（0 到 1）。"),
        QStringLiteral("value")
    );
    const QCommandLineOption probeYOption(
        QStringLiteral("probe-y"),
        QStringLiteral("点击探针的归一化 Y 坐标（0 到 1）。"),
        QStringLiteral("value")
    );
    parser.addOptions(
        {modelOption, imageOption, outputOption, expectedLabelOption, probeXOption, probeYOption}
    );
    parser.process(application);

    if (!parser.isSet(modelOption) || !parser.isSet(imageOption)) {
        QTextStream(stderr) << "错误：--model 与 --image 都是必填参数。\n";
        return 2;
    }

    try {
        pppps::poc::CancellationFlag cancellation;
        const pppps::segmentation::MaskRcnnEngine engine(parser.value(modelOption));
        const auto result = engine.run(parser.value(imageOption), cancellation);
        auto root = resultToJson(result);
        std::optional<int> clickedIndex;
        if (parser.isSet(probeXOption) != parser.isSet(probeYOption)) {
            QTextStream(stderr) << "错误：--probe-x 与 --probe-y 必须一起提供。\n";
            return 2;
        }
        if (parser.isSet(probeXOption)) {
            bool xOk = false;
            bool yOk = false;
            const auto normalizedX = parser.value(probeXOption).toDouble(&xOk);
            const auto normalizedY = parser.value(probeYOption).toDouble(&yOk);
            if (!xOk || !yOk || normalizedX < 0.0 || normalizedX > 1.0
                || normalizedY < 0.0 || normalizedY > 1.0) {
                QTextStream(stderr) << "错误：点击探针坐标必须是 0 到 1 之间的数字。\n";
                return 2;
            }
            const QPoint imagePoint(
                std::clamp(
                    static_cast<int>(normalizedX * result.sourceImage.width()),
                    0,
                    result.sourceImage.width() - 1
                ),
                std::clamp(
                    static_cast<int>(normalizedY * result.sourceImage.height()),
                    0,
                    result.sourceImage.height() - 1
                )
            );
            clickedIndex = pppps::segmentation::findObjectAt(result.objects, imagePoint);
            QJsonObject clickProbe{
                {QStringLiteral("normalized_x"), normalizedX},
                {QStringLiteral("normalized_y"), normalizedY},
                {QStringLiteral("image_x"), imagePoint.x()},
                {QStringLiteral("image_y"), imagePoint.y()},
                {QStringLiteral("hit"), clickedIndex.has_value()},
            };
            if (clickedIndex) {
                clickProbe.insert(
                    QStringLiteral("actual_label"),
                    result.objects[*clickedIndex].className
                );
                clickProbe.insert(QStringLiteral("object_id"), result.objects[*clickedIndex].id);
            }
            if (parser.isSet(expectedLabelOption)) {
                clickProbe.insert(
                    QStringLiteral("expected_label"),
                    parser.value(expectedLabelOption)
                );
                clickProbe.insert(
                    QStringLiteral("matches_expected"),
                    clickedIndex
                        && result.objects[*clickedIndex].className == parser.value(expectedLabelOption)
                );
            }
            root.insert(QStringLiteral("click_probe"), clickProbe);
        }
        const QJsonDocument json(root);
        QTextStream(stdout) << json.toJson(QJsonDocument::Compact) << '\n';

        if (parser.isSet(outputOption)
            && !saveArtifacts(parser.value(outputOption), result, json)) {
            QTextStream(stderr) << "错误：无法写入 POC 结果目录。\n";
            return 3;
        }

        if (parser.isSet(expectedLabelOption)) {
            const auto expected = parser.value(expectedLabelOption);
            const auto found = clickedIndex
                ? result.objects[*clickedIndex].className == expected
                : std::any_of(
                      result.objects.cbegin(),
                      result.objects.cend(),
                      [&expected](const auto &object) { return object.className == expected; }
                  );
            if (!found) {
                QTextStream(stderr) << "错误：真实推理没有检测到预期类别 " << expected << "。\n";
                return 4;
            }
        }
        return 0;
    } catch (const std::exception &error) {
        QTextStream(stderr) << "错误：" << error.what() << '\n';
        return 3;
    }
}
