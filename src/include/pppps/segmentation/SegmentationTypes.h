#pragma once

#include <QImage>
#include <QMetaType>
#include <QPoint>
#include <QPolygonF>
#include <QRectF>
#include <QSize>
#include <QString>
#include <QVector>

#include <optional>

namespace pppps::segmentation {

struct DetectedObject {
    int id{0};
    int classId{0};
    QString className;
    float confidence{0.0F};
    QRectF boundingBox;
    QImage mask;
    QVector<QPolygonF> contours;
    qsizetype maskArea{0};
};

struct SegmentationResult {
    QImage sourceImage;
    QVector<DetectedObject> objects;
    QString modelName;
    QString executionProvider;
    qint64 modelLoadMs{0};
    qint64 preprocessingMs{0};
    qint64 inferenceMs{0};
    qint64 postprocessingMs{0};
    qint64 totalMs{0};
};

[[nodiscard]] std::optional<int> findObjectAt(
    const QVector<DetectedObject> &objects,
    const QPoint &imagePoint
);

} // namespace pppps::segmentation

Q_DECLARE_METATYPE(pppps::segmentation::SegmentationResult)

