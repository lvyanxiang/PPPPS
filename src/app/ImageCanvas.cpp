#include "ImageCanvas.h"

#include "pppps/matting/AlphaMaskEditor.h"

#include <QMouseEvent>
#include <QPainter>
#include <QPaintEvent>

#include <algorithm>
#include <utility>

ImageCanvas::ImageCanvas(QWidget *parent)
    : QWidget(parent)
{
    setMouseTracking(true);
    setMinimumSize(520, 360);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
}

void ImageCanvas::clearResult()
{
    result_ = {};
    alphaMatte_ = {};
    alphaBrushMode_ = AlphaBrushMode::Disabled;
    showAlphaMatte_ = false;
    hoveredIndex_.reset();
    selectedIndex_.reset();
    update();
}

void ImageCanvas::setResult(pppps::segmentation::SegmentationResult result)
{
    result_ = std::move(result);
    alphaMatte_ = {};
    alphaBrushMode_ = AlphaBrushMode::Disabled;
    showAlphaMatte_ = false;
    hoveredIndex_.reset();
    selectedIndex_.reset();
    update();
}

void ImageCanvas::setAlphaMatte(QImage alphaMatte)
{
    if (alphaMatte.size() != result_.sourceImage.size()) {
        return;
    }
    alphaMatte_ = alphaMatte.convertToFormat(QImage::Format_Grayscale8);
    showAlphaMatte_ = true;
    hoveredIndex_.reset();
    update();
}

void ImageCanvas::setShowAlphaMatte(const bool show)
{
    showAlphaMatte_ = show && !alphaMatte_.isNull();
    if (!showAlphaMatte_) {
        alphaBrushMode_ = AlphaBrushMode::Disabled;
    }
    update();
}

void ImageCanvas::setAlphaBrushMode(const AlphaBrushMode mode)
{
    alphaBrushMode_ = showAlphaMatte_ ? mode : AlphaBrushMode::Disabled;
    setCursor(alphaBrushMode_ == AlphaBrushMode::Disabled ? Qt::ArrowCursor : Qt::CrossCursor);
}

std::optional<int> ImageCanvas::hoveredObjectIndex() const noexcept
{
    return hoveredIndex_;
}

std::optional<int> ImageCanvas::selectedObjectIndex() const noexcept
{
    return selectedIndex_;
}

const pppps::segmentation::SegmentationResult &ImageCanvas::result() const noexcept
{
    return result_;
}

const QImage &ImageCanvas::alphaMatte() const noexcept
{
    return alphaMatte_;
}

bool ImageCanvas::isShowingAlphaMatte() const noexcept
{
    return showAlphaMatte_;
}

QRectF ImageCanvas::imageTargetRect() const
{
    if (result_.sourceImage.isNull()) {
        return {};
    }

    const auto available = QRectF(rect()).adjusted(12.0, 12.0, -12.0, -12.0);
    const auto scale = std::min(
        available.width() / result_.sourceImage.width(),
        available.height() / result_.sourceImage.height()
    );
    const QSizeF displaySize(
        result_.sourceImage.width() * scale,
        result_.sourceImage.height() * scale
    );
    return QRectF(
        available.center() - QPointF(displaySize.width() / 2.0, displaySize.height() / 2.0),
        displaySize
    );
}

std::optional<QPoint> ImageCanvas::mapToImage(const QPointF &widgetPoint) const
{
    const auto target = imageTargetRect();
    if (!target.contains(widgetPoint) || result_.sourceImage.isNull()) {
        return std::nullopt;
    }

    const auto x = static_cast<int>(
        (widgetPoint.x() - target.left()) * result_.sourceImage.width() / target.width()
    );
    const auto y = static_cast<int>(
        (widgetPoint.y() - target.top()) * result_.sourceImage.height() / target.height()
    );
    return QPoint(
        std::clamp(x, 0, result_.sourceImage.width() - 1),
        std::clamp(y, 0, result_.sourceImage.height() - 1)
    );
}

void ImageCanvas::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event)
    QPainter painter(this);
    painter.fillRect(rect(), palette().color(QPalette::Base));
    if (result_.sourceImage.isNull()) {
        painter.setPen(palette().color(QPalette::PlaceholderText));
        painter.drawText(rect(), Qt::AlignCenter, QStringLiteral("选择图片后在这里显示识别结果"));
        return;
    }

    const auto target = imageTargetRect();
    painter.setRenderHint(QPainter::SmoothPixmapTransform);
    if (showAlphaMatte_ && !alphaMatte_.isNull()) {
        paintCheckerboard(painter, target);
        auto cutout = result_.sourceImage.convertToFormat(QImage::Format_ARGB32_Premultiplied);
        cutout.setAlphaChannel(alphaMatte_);
        painter.drawImage(target, cutout);
    } else {
        painter.drawImage(target, result_.sourceImage);
    }

    if (showAlphaMatte_) {
        return;
    }
    painter.save();
    painter.translate(target.topLeft());
    const auto scaleX = target.width() / result_.sourceImage.width();
    const auto scaleY = target.height() / result_.sourceImage.height();
    painter.scale(scaleX, scaleY);
    if (selectedIndex_) {
        paintObjectOverlay(painter, *selectedIndex_, QColor(0, 180, 255));
    }
    if (hoveredIndex_ && hoveredIndex_ != selectedIndex_) {
        paintObjectOverlay(painter, *hoveredIndex_, QColor(255, 190, 0));
    }
    painter.restore();
}

void ImageCanvas::paintCheckerboard(QPainter &painter, const QRectF &target) const
{
    constexpr int tileSize = 16;
    const QColor light(232, 232, 232);
    const QColor dark(188, 188, 188);
    const auto left = static_cast<int>(std::floor(target.left()));
    const auto top = static_cast<int>(std::floor(target.top()));
    const auto right = static_cast<int>(std::ceil(target.right()));
    const auto bottom = static_cast<int>(std::ceil(target.bottom()));
    for (int y = top; y < bottom; y += tileSize) {
        for (int x = left; x < right; x += tileSize) {
            const auto darkTile = (((x - left) / tileSize) + ((y - top) / tileSize)) % 2 != 0;
            painter.fillRect(
                QRect(x, y, std::min(tileSize, right - x), std::min(tileSize, bottom - y)),
                darkTile ? dark : light
            );
        }
    }
}

void ImageCanvas::paintObjectOverlay(
    QPainter &painter,
    const int index,
    const QColor &color
) const
{
    if (index < 0 || index >= result_.objects.size()) {
        return;
    }
    const auto &object = result_.objects[index];
    QImage overlay(object.mask.size(), QImage::Format_ARGB32_Premultiplied);
    overlay.fill(color);
    overlay.setAlphaChannel(object.mask);
    painter.save();
    painter.setOpacity(0.28);
    painter.drawImage(QPoint(0, 0), overlay);
    painter.restore();

    QPen pen(color);
    pen.setWidthF(2.5);
    pen.setCosmetic(true);
    painter.setPen(pen);
    painter.setBrush(Qt::NoBrush);
    for (const auto &contour : object.contours) {
        painter.drawPolygon(contour);
    }
}

void ImageCanvas::updateHover(const QPointF &widgetPoint)
{
    if (showAlphaMatte_) {
        return;
    }
    std::optional<int> nextIndex;
    if (const auto imagePoint = mapToImage(widgetPoint)) {
        nextIndex = pppps::segmentation::findObjectAt(result_.objects, *imagePoint);
    }
    if (nextIndex == hoveredIndex_) {
        return;
    }
    hoveredIndex_ = nextIndex;
    emit hoveredObjectChanged(hoveredIndex_.value_or(-1));
    update();
}

void ImageCanvas::applyBrushAt(const QPointF &widgetPoint)
{
    if (!showAlphaMatte_ || alphaBrushMode_ == AlphaBrushMode::Disabled) {
        return;
    }
    const auto imagePoint = mapToImage(widgetPoint);
    if (!imagePoint) {
        return;
    }
    constexpr int brushRadius = 24;
    pppps::matting::applyAlphaBrush(
        alphaMatte_,
        *imagePoint,
        brushRadius,
        alphaBrushMode_ == AlphaBrushMode::Restore ? 255 : 0
    );
    emit alphaMatteEdited();
    update();
}

void ImageCanvas::mouseMoveEvent(QMouseEvent *event)
{
    if ((event->buttons() & Qt::LeftButton) != 0) {
        applyBrushAt(event->position());
    } else {
        updateHover(event->position());
    }
    QWidget::mouseMoveEvent(event);
}

void ImageCanvas::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        if (showAlphaMatte_ && alphaBrushMode_ != AlphaBrushMode::Disabled) {
            applyBrushAt(event->position());
            QWidget::mousePressEvent(event);
            return;
        }
        updateHover(event->position());
        if (selectedIndex_ != hoveredIndex_) {
            selectedIndex_ = hoveredIndex_;
            emit selectedObjectChanged(selectedIndex_.value_or(-1));
            update();
        }
    }
    QWidget::mousePressEvent(event);
}

void ImageCanvas::leaveEvent(QEvent *event)
{
    if (hoveredIndex_) {
        hoveredIndex_.reset();
        emit hoveredObjectChanged(-1);
        update();
    }
    QWidget::leaveEvent(event);
}
