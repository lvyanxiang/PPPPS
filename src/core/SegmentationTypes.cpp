#include "pppps/segmentation/SegmentationTypes.h"

#include <limits>

namespace pppps::segmentation {

std::optional<int> findObjectAt(
    const QVector<DetectedObject> &objects,
    const QPoint &imagePoint
)
{
    std::optional<int> bestIndex;
    auto bestArea = std::numeric_limits<qsizetype>::max();
    float bestConfidence = -1.0F;

    for (qsizetype index = 0; index < objects.size(); ++index) {
        const auto &object = objects[index];
        if (!object.mask.rect().contains(imagePoint)) {
            continue;
        }

        const auto *row = object.mask.constScanLine(imagePoint.y());
        if (row == nullptr || row[imagePoint.x()] == 0) {
            continue;
        }

        if (object.maskArea < bestArea
            || (object.maskArea == bestArea && object.confidence > bestConfidence)) {
            bestIndex = static_cast<int>(index);
            bestArea = object.maskArea;
            bestConfidence = object.confidence;
        }
    }

    return bestIndex;
}

} // namespace pppps::segmentation

