#include "pppps/matting/MattingTypes.h"

#include "pppps/poc/PocPipeline.h"

#include <QColor>
#include <QPainter>

#include <algorithm>
#include <cmath>

namespace pppps::matting {
namespace {

QImage toGray8(const QImage &image)
{
    return image.format() == QImage::Format_Grayscale8
        ? image
        : image.convertToFormat(QImage::Format_Grayscale8);
}

void requireCompatibleImages(const QImage &first, const QImage &second)
{
    if (first.isNull() || second.isNull() || first.size() != second.size()) {
        throw pppps::poc::PocError("Alpha operation requires two non-empty images of equal size");
    }
}

} // namespace

AlphaStatistics analyzeAlpha(const QImage &alphaMatte)
{
    if (alphaMatte.isNull()) {
        throw pppps::poc::PocError("Cannot analyze an empty alpha matte");
    }

    const auto alpha = toGray8(alphaMatte);
    AlphaStatistics statistics;
    double sum = 0.0;
    for (int y = 0; y < alpha.height(); ++y) {
        const auto *row = alpha.constScanLine(y);
        for (int x = 0; x < alpha.width(); ++x) {
            const auto value = row[x];
            sum += value;
            if (value == 0) {
                ++statistics.transparentPixels;
            } else if (value == 255) {
                ++statistics.opaquePixels;
            } else {
                ++statistics.softPixels;
            }
        }
    }
    const auto pixels = static_cast<double>(alpha.width()) * alpha.height();
    statistics.meanAlpha = sum / (pixels * 255.0);
    return statistics;
}

MaskAlphaComparison compareBinaryMaskWithAlpha(
    const QImage &binaryMask,
    const QImage &alphaMatte,
    const quint8 threshold
)
{
    requireCompatibleImages(binaryMask, alphaMatte);
    const auto mask = toGray8(binaryMask);
    const auto alpha = toGray8(alphaMatte);

    MaskAlphaComparison comparison;
    qsizetype intersection = 0;
    qsizetype unionPixels = 0;
    double difference = 0.0;
    comparison.comparedPixels = static_cast<qsizetype>(alpha.width()) * alpha.height();
    for (int y = 0; y < alpha.height(); ++y) {
        const auto *maskRow = mask.constScanLine(y);
        const auto *alphaRow = alpha.constScanLine(y);
        for (int x = 0; x < alpha.width(); ++x) {
            const auto maskInside = maskRow[x] >= 128;
            const auto alphaInside = alphaRow[x] >= threshold;
            if (maskInside != alphaInside) {
                ++comparison.disagreementPixels;
            }
            if (maskInside && alphaInside) {
                ++intersection;
            }
            if (maskInside || alphaInside) {
                ++unionPixels;
            }
            const auto binaryValue = maskInside ? 255 : 0;
            difference += std::abs(binaryValue - static_cast<int>(alphaRow[x]));
        }
    }
    comparison.thresholdIoU = unionPixels == 0
        ? 1.0
        : static_cast<double>(intersection) / static_cast<double>(unionPixels);
    comparison.meanAbsoluteDifference = difference
        / (static_cast<double>(comparison.comparedPixels) * 255.0);
    return comparison;
}

QImage composeOnCheckerboard(
    const QImage &sourceImage,
    const QImage &alphaMatte,
    const int tileSize
)
{
    requireCompatibleImages(sourceImage, alphaMatte);
    if (tileSize <= 0) {
        throw pppps::poc::PocError("Checkerboard tile size must be positive");
    }

    QImage result(sourceImage.size(), QImage::Format_ARGB32_Premultiplied);
    QPainter painter(&result);
    const QColor light(232, 232, 232);
    const QColor dark(188, 188, 188);
    for (int y = 0; y < result.height(); y += tileSize) {
        for (int x = 0; x < result.width(); x += tileSize) {
            const auto alternate = ((x / tileSize) + (y / tileSize)) % 2 != 0;
            painter.fillRect(x, y, tileSize, tileSize, alternate ? dark : light);
        }
    }

    auto cutout = sourceImage.convertToFormat(QImage::Format_ARGB32);
    cutout.setAlphaChannel(toGray8(alphaMatte));
    painter.drawImage(QPoint(0, 0), cutout);
    painter.end();
    return result;
}

} // namespace pppps::matting
