#include "pppps/segmentation/SegmentationTypes.h"

#include <QtTest>

using pppps::segmentation::DetectedObject;
using pppps::segmentation::findObjectAt;

namespace {

DetectedObject rectangleObject(
    const int id,
    const QRect &rectangle,
    const qsizetype area,
    const float confidence
)
{
    QImage mask(100, 100, QImage::Format_Grayscale8);
    mask.fill(0);
    for (int y = rectangle.top(); y <= rectangle.bottom(); ++y) {
        auto *row = mask.scanLine(y);
        for (int x = rectangle.left(); x <= rectangle.right(); ++x) {
            row[x] = 255;
        }
    }
    return DetectedObject{
        .id = id,
        .classId = id,
        .className = QStringLiteral("test"),
        .confidence = confidence,
        .boundingBox = rectangle,
        .mask = mask,
        .contours = {},
        .maskArea = area,
    };
}

} // namespace

class SegmentationTypesTest final : public QObject {
    Q_OBJECT

private slots:
    void missesBackground();
    void hitsMaskPixel();
    void choosesSmallestOverlappingObject();
    void usesConfidenceToBreakAreaTie();
};

void SegmentationTypesTest::missesBackground()
{
    const QVector objects{rectangleObject(1, QRect(20, 20, 60, 60), 3600, 0.9F)};
    QVERIFY(!findObjectAt(objects, QPoint(5, 5)).has_value());
}

void SegmentationTypesTest::hitsMaskPixel()
{
    const QVector objects{rectangleObject(1, QRect(20, 20, 60, 60), 3600, 0.9F)};
    QCOMPARE(findObjectAt(objects, QPoint(40, 40)), std::optional<int>(0));
}

void SegmentationTypesTest::choosesSmallestOverlappingObject()
{
    const QVector objects{
        rectangleObject(1, QRect(10, 10, 80, 80), 6400, 0.99F),
        rectangleObject(2, QRect(30, 30, 20, 20), 400, 0.80F),
    };
    QCOMPARE(findObjectAt(objects, QPoint(35, 35)), std::optional<int>(1));
}

void SegmentationTypesTest::usesConfidenceToBreakAreaTie()
{
    const QVector objects{
        rectangleObject(1, QRect(20, 20, 40, 40), 1600, 0.75F),
        rectangleObject(2, QRect(20, 20, 40, 40), 1600, 0.95F),
    };
    QCOMPARE(findObjectAt(objects, QPoint(30, 30)), std::optional<int>(1));
}

QTEST_GUILESS_MAIN(SegmentationTypesTest)

#include "tst_SegmentationTypes.moc"

