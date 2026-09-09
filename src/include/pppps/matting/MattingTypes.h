#pragma once

#include <QImage>
#include <QMetaType>
#include <QString>

namespace pppps::matting {

struct AlphaStatistics {
    qsizetype transparentPixels{0};
    qsizetype softPixels{0};
    qsizetype opaquePixels{0};
    double meanAlpha{0.0};
};

struct MaskAlphaComparison {
    qsizetype comparedPixels{0};
    qsizetype disagreementPixels{0};
    double thresholdIoU{0.0};
    double meanAbsoluteDifference{0.0};
};

struct MattingResult {
    QImage sourceImage;
    QImage alphaMatte;
    AlphaStatistics statistics;
    QString modelName;
    QString executionProvider;
    qint64 modelLoadMs{0};
    qint64 preprocessingMs{0};
    qint64 inferenceMs{0};
    qint64 postprocessingMs{0};
    qint64 totalMs{0};
};

[[nodiscard]] AlphaStatistics analyzeAlpha(const QImage &alphaMatte);
[[nodiscard]] MaskAlphaComparison compareBinaryMaskWithAlpha(
    const QImage &binaryMask,
    const QImage &alphaMatte,
    quint8 threshold = 128
);
[[nodiscard]] QImage composeOnCheckerboard(
    const QImage &sourceImage,
    const QImage &alphaMatte,
    int tileSize = 16
);

} // namespace pppps::matting

Q_DECLARE_METATYPE(pppps::matting::MattingResult)
