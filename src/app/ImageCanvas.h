#pragma once

#include "pppps/segmentation/SegmentationTypes.h"

#include <QWidget>

#include <optional>

class QColor;
class QPainter;

class ImageCanvas final : public QWidget {
    Q_OBJECT

public:
    explicit ImageCanvas(QWidget *parent = nullptr);

    void clearResult();
    void setResult(pppps::segmentation::SegmentationResult result);

    [[nodiscard]] std::optional<int> hoveredObjectIndex() const noexcept;
    [[nodiscard]] std::optional<int> selectedObjectIndex() const noexcept;
    [[nodiscard]] const pppps::segmentation::SegmentationResult &result() const noexcept;

signals:
    void hoveredObjectChanged(int objectIndex);
    void selectedObjectChanged(int objectIndex);

protected:
    void paintEvent(QPaintEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void leaveEvent(QEvent *event) override;

private:
    [[nodiscard]] QRectF imageTargetRect() const;
    [[nodiscard]] std::optional<QPoint> mapToImage(const QPointF &widgetPoint) const;
    void updateHover(const QPointF &widgetPoint);
    void paintObjectOverlay(QPainter &painter, int index, const QColor &color) const;

    pppps::segmentation::SegmentationResult result_;
    std::optional<int> hoveredIndex_;
    std::optional<int> selectedIndex_;
};
