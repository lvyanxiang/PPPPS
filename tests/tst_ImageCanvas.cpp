#include "ImageCanvas.h"

#include <QSignalSpy>
#include <QtTest>

namespace {

pppps::segmentation::SegmentationResult sampleResult()
{
    QImage source(100, 100, QImage::Format_RGB32);
    source.fill(QColor(30, 30, 30));
    QImage mask(100, 100, QImage::Format_Grayscale8);
    mask.fill(0);
    for (int y = 20; y < 80; ++y) {
        auto *row = mask.scanLine(y);
        for (int x = 20; x < 80; ++x) {
            row[x] = 255;
        }
    }
    return pppps::segmentation::SegmentationResult{
        .sourceImage = source,
        .objects = {pppps::segmentation::DetectedObject{
            .id = 1,
            .classId = 16,
            .className = QStringLiteral("cat"),
            .confidence = 0.98F,
            .boundingBox = QRectF(20, 20, 60, 60),
            .mask = mask,
            .contours = {QPolygonF{
                QPointF(20, 20), QPointF(79, 20), QPointF(79, 79), QPointF(20, 79),
            }},
            .maskArea = 3600,
        }},
        .modelName = QStringLiteral("test"),
        .executionProvider = QStringLiteral("CPUExecutionProvider"),
    };
}

} // namespace

class ImageCanvasTest final : public QObject {
    Q_OBJECT

private slots:
    void hoverHighlightsAndClickSelectsMask();
};

void ImageCanvasTest::hoverHighlightsAndClickSelectsMask()
{
    ImageCanvas canvas;
    canvas.resize(520, 360);
    canvas.setResult(sampleResult());
    canvas.show();
    QCoreApplication::processEvents();

    QSignalSpy hovered(&canvas, &ImageCanvas::hoveredObjectChanged);
    QSignalSpy selected(&canvas, &ImageCanvas::selectedObjectChanged);
    QTest::mouseMove(&canvas, QPoint(260, 180));
    QTRY_COMPARE(hovered.count(), 1);
    QCOMPARE(hovered.constFirst().constFirst().toInt(), 0);
    QCOMPARE(canvas.hoveredObjectIndex(), std::optional<int>(0));

    QTest::mouseClick(&canvas, Qt::LeftButton, Qt::NoModifier, QPoint(260, 180));
    QTRY_COMPARE(selected.count(), 1);
    QCOMPARE(selected.constFirst().constFirst().toInt(), 0);
    QCOMPARE(canvas.selectedObjectIndex(), std::optional<int>(0));
    QCOMPARE(canvas.result().objects[0].mask.pixelColor(50, 50).value(), 255);

    QTest::mouseMove(&canvas, QPoint(5, 5));
    QTRY_COMPARE(hovered.count(), 2);
    QCOMPARE(hovered.constLast().constFirst().toInt(), -1);
}

QTEST_MAIN(ImageCanvasTest)

#include "tst_ImageCanvas.moc"

