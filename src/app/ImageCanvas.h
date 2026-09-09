#pragma once

#include "pppps/segmentation/SegmentationTypes.h"

#include <QWidget>

#include <optional>

class QColor;
class QPainter;

class ImageCanvas final : public QWidget {
    Q_OBJECT

public:
    enum class AlphaBrushMode {
        Disabled,
        Restore,
        Erase,
    };

    explicit ImageCanvas(QWidget *parent = nullptr);

    void clearResult();
    void setResult(pppps::segmentation::SegmentationResult result);
    void setAlphaMatte(QImage alphaMatte);
    void setShowAlphaMatte(bool show);
    void setAlphaBrushMode(AlphaBrushMode mode);

    [[nodiscard]] std::optional<int> hoveredObjectIndex() const noexcept;
    [[nodiscard]] std::optional<int> selectedObjectIndex() const noexcept;
    [[nodiscard]] const pppps::segmentation::SegmentationResult &result() const noexcept;
    [[nodiscard]] const QImage &alphaMatte() const noexcept;
    [[nodiscard]] bool isShowingAlphaMatte() const noexcept;

signals:
    void hoveredObjectChanged(int objectIndex);
    void selectedObjectChanged(int objectIndex);
    void alphaMatteEdited();

protected:
    void paintEvent(QPaintEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void leaveEvent(QEvent *event) override;

private:
    [[nodiscard]] QRectF imageTargetRect() const;
    [[nodiscard]] std::optional<QPoint> mapToImage(const QPointF &widgetPoint) const;
    void updateHover(const QPointF &widgetPoint);
    void applyBrushAt(const QPointF &widgetPoint);
    void paintCheckerboard(QPainter &painter, const QRectF &target) const;
    void paintObjectOverlay(QPainter &painter, int index, const QColor &color) const;

    pppps::segmentation::SegmentationResult result_;
    QImage alphaMatte_;
    AlphaBrushMode alphaBrushMode_{AlphaBrushMode::Disabled};
    bool showAlphaMatte_{false};
    std::optional<int> hoveredIndex_;
    std::optional<int> selectedIndex_;
};
