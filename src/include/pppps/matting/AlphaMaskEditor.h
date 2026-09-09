#pragma once

#include <QImage>
#include <QPoint>

namespace pppps::matting {

void applyAlphaBrush(
    QImage &alphaMatte,
    const QPoint &center,
    int radius,
    quint8 targetAlpha,
    float hardness = 0.65F
);

} // namespace pppps::matting
