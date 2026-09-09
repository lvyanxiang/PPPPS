#include "pppps/matting/AlphaMaskEditor.h"

#include "pppps/poc/PocPipeline.h"

#include <algorithm>
#include <cmath>

namespace pppps::matting {

void applyAlphaBrush(
    QImage &alphaMatte,
    const QPoint &center,
    const int radius,
    const quint8 targetAlpha,
    const float hardness
)
{
    if (alphaMatte.isNull() || alphaMatte.format() != QImage::Format_Grayscale8) {
        throw pppps::poc::PocError("Alpha brush requires a non-empty Grayscale8 matte");
    }
    if (radius <= 0 || hardness < 0.0F || hardness > 1.0F) {
        throw pppps::poc::PocError("Alpha brush radius/hardness is invalid");
    }

    const auto left = std::max(0, center.x() - radius);
    const auto right = std::min(alphaMatte.width() - 1, center.x() + radius);
    const auto top = std::max(0, center.y() - radius);
    const auto bottom = std::min(alphaMatte.height() - 1, center.y() + radius);
    const auto hardRadius = static_cast<float>(radius) * hardness;
    const auto featherWidth = std::max(0.0001F, static_cast<float>(radius) - hardRadius);

    for (int y = top; y <= bottom; ++y) {
        auto *row = alphaMatte.scanLine(y);
        for (int x = left; x <= right; ++x) {
            const auto dx = static_cast<float>(x - center.x());
            const auto dy = static_cast<float>(y - center.y());
            const auto distance = std::sqrt((dx * dx) + (dy * dy));
            if (distance > static_cast<float>(radius)) {
                continue;
            }
            const auto strength = distance <= hardRadius
                ? 1.0F
                : std::clamp(1.0F - ((distance - hardRadius) / featherWidth), 0.0F, 1.0F);
            const auto current = static_cast<float>(row[x]);
            row[x] = static_cast<uchar>(std::clamp(
                static_cast<int>(std::lround(current + ((targetAlpha - current) * strength))),
                0,
                255
            ));
        }
    }
}

} // namespace pppps::matting
