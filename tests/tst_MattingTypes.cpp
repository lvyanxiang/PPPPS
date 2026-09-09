#include "pppps/matting/AlphaMaskEditor.h"
#include "pppps/matting/MattingTypes.h"

#include <QtTest>

class MattingTypesTest final : public QObject {
    Q_OBJECT

private slots:
    void analyzesContinuousAlpha();
    void comparesBinaryMaskWithAlpha();
    void brushEditsOnlyAlpha();
    void composesWithoutChangingSource();
};

void MattingTypesTest::analyzesContinuousAlpha()
{
    QImage alpha(3, 1, QImage::Format_Grayscale8);
    auto *row = alpha.scanLine(0);
    row[0] = 0;
    row[1] = 127;
    row[2] = 255;

    const auto statistics = pppps::matting::analyzeAlpha(alpha);
    QCOMPARE(statistics.transparentPixels, 1);
    QCOMPARE(statistics.softPixels, 1);
    QCOMPARE(statistics.opaquePixels, 1);
    QVERIFY(qAbs(statistics.meanAlpha - (382.0 / 765.0)) < 0.0001);
}

void MattingTypesTest::comparesBinaryMaskWithAlpha()
{
    QImage mask(2, 1, QImage::Format_Grayscale8);
    mask.scanLine(0)[0] = 0;
    mask.scanLine(0)[1] = 255;
    QImage alpha(2, 1, QImage::Format_Grayscale8);
    alpha.scanLine(0)[0] = 130;
    alpha.scanLine(0)[1] = 255;

    const auto comparison = pppps::matting::compareBinaryMaskWithAlpha(mask, alpha);
    QCOMPARE(comparison.comparedPixels, 2);
    QCOMPARE(comparison.disagreementPixels, 1);
    QCOMPARE(comparison.thresholdIoU, 0.5);
}

void MattingTypesTest::brushEditsOnlyAlpha()
{
    QImage source(20, 20, QImage::Format_RGB32);
    source.fill(QColor(20, 40, 60));
    const auto originalSource = source;
    QImage alpha(20, 20, QImage::Format_Grayscale8);
    alpha.fill(0);

    pppps::matting::applyAlphaBrush(alpha, QPoint(10, 10), 5, 255);
    QCOMPARE(alpha.pixelColor(10, 10).value(), 255);
    QCOMPARE(alpha.pixelColor(0, 0).value(), 0);
    QCOMPARE(source, originalSource);

    pppps::matting::applyAlphaBrush(alpha, QPoint(10, 10), 3, 0);
    QCOMPARE(alpha.pixelColor(10, 10).value(), 0);
}

void MattingTypesTest::composesWithoutChangingSource()
{
    QImage source(4, 4, QImage::Format_RGB32);
    source.fill(Qt::red);
    const auto originalSource = source;
    QImage alpha(4, 4, QImage::Format_Grayscale8);
    alpha.fill(128);

    const auto composed = pppps::matting::composeOnCheckerboard(source, alpha, 2);
    QCOMPARE(composed.size(), source.size());
    QCOMPARE(source, originalSource);
    QVERIFY(composed.pixelColor(0, 0) != source.pixelColor(0, 0));
}

QTEST_MAIN(MattingTypesTest)

#include "tst_MattingTypes.moc"
