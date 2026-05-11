/*
 * VertexNote
 *
 * Qt canvas element selection helpers.
 */

#include "QtCanvas.h"

#include <algorithm>
#include <cmath>
#include <cstddef>

#include <QColor>
#include <QPainter>
#include <QPen>
#include <QPointF>
#include <QRectF>
#include <QString>

namespace {

auto selectionQColorFromColor(Color color, int alphaOverride = -1) -> QColor {
    return QColor(color.red, color.green, color.blue, alphaOverride >= 0 ? alphaOverride : color.alpha);
}

}  // namespace

void QtCanvas::selectElementAtScreen(const QPointF& screenPoint, bool additive) {
    if (!this->documentController) {
        return;
    }

    const QPointF scenePoint = screenToScene(screenPoint);
    const auto pageIdx = pageIndexAtScenePoint(scenePoint);
    if (!pageIdx) {
        if (!additive) {
            this->documentController->clearElementSelection();
        }
        update();
        return;
    }

    const auto rects = pageRects();
    const double pageX = scenePoint.x() - rects[*pageIdx].x();
    const double pageY = scenePoint.y() - rects[*pageIdx].y();
    const double hitRadius = 10.0 / this->zoomFactor;

    this->documentController->selectElementAt(*pageIdx, pageX, pageY, hitRadius, additive);
    const auto& sel = this->documentController->elementSelection();
    if (sel) {
        updateDebugOverlay(QStringLiteral("selected %1 element(s)").arg(static_cast<int>(sel->elements.size())));
    } else {
        updateDebugOverlay(QStringLiteral("selection cleared"));
    }
    update();
    Q_EMIT selectionStateChanged();
}

void QtCanvas::beginRubberBand(const QPointF& screenPoint) {
    this->rubberBanding = true;
    this->rubberBandOrigin = screenPoint;
    this->rubberBandCurrent = screenPoint;
}

void QtCanvas::updateRubberBand(const QPointF& screenPoint) {
    this->rubberBandCurrent = screenPoint;
    update();
}

void QtCanvas::finalizeRubberBand() {
    if (!this->rubberBanding) {
        return;
    }

    const QPointF delta = this->rubberBandCurrent - this->rubberBandOrigin;
    const bool isClick = std::abs(delta.x()) < 4.0 && std::abs(delta.y()) < 4.0;

    if (isClick) {
        selectElementAtScreen(this->rubberBandOrigin, false);
    } else if (this->documentController) {
        const QPointF sceneOrigin = screenToScene(this->rubberBandOrigin);
        const QPointF sceneCurrent = screenToScene(this->rubberBandCurrent);
        const double x = std::min(sceneOrigin.x(), sceneCurrent.x());
        const double y = std::min(sceneOrigin.y(), sceneCurrent.y());
        const double w = std::abs(sceneCurrent.x() - sceneOrigin.x());
        const double h = std::abs(sceneCurrent.y() - sceneOrigin.y());

        const auto rects = pageRects();
        const QRectF bandRect(x, y, w, h);
        for (std::size_t i = 0; i < rects.size(); ++i) {
            if (rects[i].intersects(bandRect)) {
                const double pageX = x - rects[i].x();
                const double pageY = y - rects[i].y();
                this->documentController->selectElementsInRect(i, pageX, pageY, w, h);
                break;
            }
        }

        const auto& sel = this->documentController->elementSelection();
        if (sel) {
            updateDebugOverlay(
                    QStringLiteral("rect-selected %1 element(s)").arg(static_cast<int>(sel->elements.size())));
        } else {
            updateDebugOverlay(QStringLiteral("selection cleared"));
        }
    }

    this->rubberBanding = false;
    update();
    Q_EMIT selectionStateChanged();
}

void QtCanvas::cancelRubberBand() {
    this->rubberBanding = false;
    update();
}

void QtCanvas::beginMoveSelectionAtScreen(const QPointF& screenPoint) {
    if (!this->documentController) {
        return;
    }

    const QPointF scenePoint = screenToScene(screenPoint);
    const auto& sel = this->documentController->elementSelection();
    if (!sel) {
        return;
    }

    const auto rects = pageRects();
    if (sel->pageIndex >= rects.size()) {
        return;
    }

    const double pageX = scenePoint.x() - rects[sel->pageIndex].x();
    const double pageY = scenePoint.y() - rects[sel->pageIndex].y();

    if (this->documentController->beginMoveSelection(pageX, pageY)) {
        this->movingSelection = true;
        setCursor(Qt::ClosedHandCursor);
    }
}

void QtCanvas::updateMoveSelectionAtScreen(const QPointF& screenPoint) {
    if (!this->documentController || !this->movingSelection) {
        return;
    }

    const auto& sel = this->documentController->elementSelection();
    if (!sel) {
        return;
    }

    const auto rects = pageRects();
    if (sel->pageIndex >= rects.size()) {
        return;
    }

    const QPointF scenePoint = screenToScene(screenPoint);
    const double pageX = scenePoint.x() - rects[sel->pageIndex].x();
    const double pageY = scenePoint.y() - rects[sel->pageIndex].y();

    if (this->documentController->updateMoveSelection(pageX, pageY)) {
        update();
    }
}

void QtCanvas::finalizeMoveSelection() {
    if (!this->documentController) {
        this->movingSelection = false;
        return;
    }

    const bool changed = this->documentController->endMoveSelection();
    this->movingSelection = false;
    setCursor(Qt::ArrowCursor);
    update();
    if (changed) {
        Q_EMIT documentEdited();
    }
}

void QtCanvas::cancelMoveSelection() {
    if (this->documentController) {
        this->documentController->cancelMoveSelection();
    }
    this->movingSelection = false;
    setCursor(Qt::ArrowCursor);
    update();
}

auto QtCanvas::selectionScaleHandleAtScreen(const QPointF& screenPoint) const -> int {
    if (!this->documentController) {
        return -1;
    }

    const auto& sel = this->documentController->elementSelection();
    const auto bounds = this->documentController->selectionBounds();
    if (!sel || !bounds || (bounds->width <= 0.0 && bounds->height <= 0.0)) {
        return -1;
    }

    const auto rects = pageRects();
    if (sel->pageIndex >= rects.size()) {
        return -1;
    }

    const QPointF scenePoint = screenToScene(screenPoint);
    const double margin = 4.0 / this->zoomFactor;
    const double visualWidth = std::max(bounds->width, 8.0 / this->zoomFactor);
    const double visualHeight = std::max(bounds->height, 8.0 / this->zoomFactor);
    const QRectF rect(rects[sel->pageIndex].x() + bounds->x - margin,
                      rects[sel->pageIndex].y() + bounds->y - margin,
                      visualWidth + (2.0 * margin),
                      visualHeight + (2.0 * margin));
    const QPointF handles[] = {rect.topLeft(),
                               QPointF(rect.center().x(), rect.top()),
                               rect.topRight(),
                               QPointF(rect.right(), rect.center().y()),
                               rect.bottomRight(),
                               QPointF(rect.center().x(), rect.bottom()),
                               rect.bottomLeft(),
                               QPointF(rect.left(), rect.center().y())};

    const double hitSize = 12.0 / this->zoomFactor;
    for (int i = 0; i < 8; ++i) {
        const QRectF hitRect(handles[i].x() - hitSize / 2.0, handles[i].y() - hitSize / 2.0, hitSize, hitSize);
        if (hitRect.contains(scenePoint)) {
            return i;
        }
    }
    return -1;
}

void QtCanvas::beginScaleSelectionAtScreen(const QPointF& screenPoint, int handleIndex) {
    if (!this->documentController || handleIndex < 0 || handleIndex > 7) {
        return;
    }

    const auto& sel = this->documentController->elementSelection();
    const auto bounds = this->documentController->selectionBounds();
    if (!sel || !bounds || (bounds->width <= 0.0 && bounds->height <= 0.0)) {
        return;
    }

    struct ScaleHandle {
        double originX = 0.0;
        double originY = 0.0;
        double startX = 0.0;
        double startY = 0.0;
        bool scaleX = true;
        bool scaleY = true;
    };

    const double left = bounds->x;
    const double top = bounds->y;
    const double right = bounds->x + bounds->width;
    const double bottom = bounds->y + bounds->height;
    const double centerX = bounds->x + bounds->width / 2.0;
    const double centerY = bounds->y + bounds->height / 2.0;
    const ScaleHandle handles[] = {
            {.originX = right, .originY = bottom, .startX = left, .startY = top, .scaleX = true, .scaleY = true},
            {.originX = centerX, .originY = bottom, .startX = centerX, .startY = top, .scaleX = false, .scaleY = true},
            {.originX = left, .originY = bottom, .startX = right, .startY = top, .scaleX = true, .scaleY = true},
            {.originX = left, .originY = centerY, .startX = right, .startY = centerY, .scaleX = true, .scaleY = false},
            {.originX = left, .originY = top, .startX = right, .startY = bottom, .scaleX = true, .scaleY = true},
            {.originX = centerX, .originY = top, .startX = centerX, .startY = bottom, .scaleX = false, .scaleY = true},
            {.originX = right, .originY = top, .startX = left, .startY = bottom, .scaleX = true, .scaleY = true},
            {.originX = right, .originY = centerY, .startX = left, .startY = centerY, .scaleX = true, .scaleY = false},
    };
    const auto& handle = handles[handleIndex];

    if (this->documentController->beginScaleSelection(handle.originX, handle.originY, handle.startX, handle.startY,
                                                      handle.scaleX, handle.scaleY, this->restoreLineWidthOnScale)) {
        this->scalingSelection = true;
        this->activeSelectionScaleHandle = handleIndex;
        if (!handle.scaleX) {
            setCursor(Qt::SizeVerCursor);
        } else if (!handle.scaleY) {
            setCursor(Qt::SizeHorCursor);
        } else {
            setCursor((handleIndex == 0 || handleIndex == 4) ? Qt::SizeFDiagCursor : Qt::SizeBDiagCursor);
        }
        updateScaleSelectionAtScreen(screenPoint);
    }
}

void QtCanvas::updateScaleSelectionAtScreen(const QPointF& screenPoint) {
    if (!this->documentController || !this->scalingSelection) {
        return;
    }

    const auto& sel = this->documentController->elementSelection();
    if (!sel) {
        return;
    }

    const auto rects = pageRects();
    if (sel->pageIndex >= rects.size()) {
        return;
    }

    const QPointF scenePoint = screenToScene(screenPoint);
    const double pageX = scenePoint.x() - rects[sel->pageIndex].x();
    const double pageY = scenePoint.y() - rects[sel->pageIndex].y();

    if (this->documentController->updateScaleSelection(pageX, pageY)) {
        update();
    }
}

void QtCanvas::finalizeScaleSelection() {
    if (!this->documentController) {
        this->scalingSelection = false;
        this->activeSelectionScaleHandle = -1;
        return;
    }

    const bool changed = this->documentController->endScaleSelection();
    this->scalingSelection = false;
    this->activeSelectionScaleHandle = -1;
    setCursor(Qt::ArrowCursor);
    update();
    if (changed) {
        Q_EMIT documentEdited();
    }
}

void QtCanvas::cancelScaleSelection() {
    if (this->documentController) {
        this->documentController->cancelScaleSelection();
    }
    this->scalingSelection = false;
    this->activeSelectionScaleHandle = -1;
    setCursor(Qt::ArrowCursor);
    update();
}

void QtCanvas::drawSelectionOverlay(QPainter& painter) const {
    if (!this->documentController) {
        return;
    }

    const auto& sel = this->documentController->elementSelection();
    const auto bounds = this->documentController->selectionBounds();
    if (!sel || !bounds || sel->elements.empty()) {
        return;
    }

    const auto rects = pageRects();
    if (sel->pageIndex >= rects.size()) {
        return;
    }

    const QRectF& pageRect = rects[sel->pageIndex];
    painter.save();

    const QRectF selRect(pageRect.x() + bounds->x, pageRect.y() + bounds->y,
                         std::max(bounds->width, 8.0 / this->zoomFactor),
                         std::max(bounds->height, 8.0 / this->zoomFactor));
    const double handleSize = 6.0 / this->zoomFactor;
    const double margin = 4.0 / this->zoomFactor;
    const QRectF outerRect = selRect.adjusted(-margin, -margin, margin, margin);

    const QColor selection = selectionQColorFromColor(this->selectionColor);
    QPen dashPen(selectionQColorFromColor(this->selectionColor, 200), 1.2 / this->zoomFactor);
    dashPen.setStyle(Qt::DashLine);
    dashPen.setCosmetic(false);
    painter.setPen(dashPen);
    painter.setBrush(selectionQColorFromColor(this->selectionColor, 20));
    painter.drawRect(outerRect);

    painter.setPen(QPen(selection, 1.0 / this->zoomFactor));
    painter.setBrush(QColor(255, 255, 255, 230));
    const QPointF handles[] = {outerRect.topLeft(),
                               QPointF(outerRect.center().x(), outerRect.top()),
                               outerRect.topRight(),
                               QPointF(outerRect.right(), outerRect.center().y()),
                               outerRect.bottomRight(),
                               QPointF(outerRect.center().x(), outerRect.bottom()),
                               outerRect.bottomLeft(),
                               QPointF(outerRect.left(), outerRect.center().y())};
    for (const auto& handle: handles) {
        painter.drawRect(QRectF(handle.x() - handleSize / 2.0, handle.y() - handleSize / 2.0, handleSize, handleSize));
    }

    painter.restore();
}

void QtCanvas::drawRubberBand(QPainter& painter) const {
    if (!this->rubberBanding) {
        return;
    }

    painter.save();
    painter.resetTransform();

    const QRectF bandRect = QRectF(this->rubberBandOrigin, this->rubberBandCurrent).normalized();
    QPen pen(selectionQColorFromColor(this->selectionColor, 180), 1.0);
    pen.setStyle(Qt::DashLine);
    painter.setPen(pen);
    painter.setBrush(selectionQColorFromColor(this->selectionColor, 30));
    painter.drawRect(bandRect);

    painter.restore();
}
