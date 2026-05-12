/*
 * VertexNote
 *
 * Qt canvas vertical-space and touch interaction tools.
 */

#include "QtCanvas.h"

#include <algorithm>
#include <cmath>

#include <QColor>
#include <QPainter>
#include <QPen>
#include <QPointF>
#include <QRectF>
#include <QString>
#include <QTouchEvent>

void QtCanvas::beginVerticalSpaceAtScreen(const QPointF& screenPoint, bool moveAbove) {
    if (!this->documentController) {
        return;
    }

    const QPointF scenePoint = screenToScene(screenPoint);
    const auto pageIdx = pageIndexAtScenePoint(scenePoint);
    const auto rects = pageRects();
    if (!pageIdx || *pageIdx >= rects.size()) {
        return;
    }

    const auto& pageRect = rects[*pageIdx];
    const double pageY = scenePoint.y() - pageRect.y();
    if (!this->documentController->beginVerticalSpace(*pageIdx, pageY, moveAbove)) {
        Q_EMIT statusHintChanged(QStringLiteral("No elements to move"));
        return;
    }

    this->verticalSpacePreview = VerticalSpacePreview{
            .pageIndex = *pageIdx, .startY = pageY, .currentY = pageY, .moveAbove = moveAbove};
    updateDebugOverlay(moveAbove ? QStringLiteral("vertical space above") : QStringLiteral("vertical space below"));
    update();
}

void QtCanvas::updateVerticalSpaceAtScreen(const QPointF& screenPoint) {
    if (!this->documentController || !this->documentController->isVerticalSpacing() || !this->verticalSpacePreview) {
        return;
    }

    const QPointF scenePoint = screenToScene(screenPoint);
    const auto rects = pageRects();
    if (this->verticalSpacePreview->pageIndex >= rects.size()) {
        return;
    }

    const auto& pageRect = rects[this->verticalSpacePreview->pageIndex];
    double pageY = scenePoint.y() - pageRect.y();
    pageY = std::clamp(pageY, 0.0, pageRect.height());
    if (this->gridSnapEnabled && this->snapGridSize > 0.0) {
        pageY = std::round(pageY / this->snapGridSize) * this->snapGridSize;
    }

    this->verticalSpacePreview->currentY = pageY;
    if (this->documentController->updateVerticalSpace(pageY)) {
        Q_EMIT statusHintChanged(
                QStringLiteral("Vertical space %1 pt").arg(pageY - this->verticalSpacePreview->startY, 0, 'f', 1));
    }
    update();
}

void QtCanvas::finalizeVerticalSpace() {
    if (!this->documentController) {
        this->verticalSpacePreview.reset();
        return;
    }

    const bool changed = this->documentController->endVerticalSpace();
    this->verticalSpacePreview.reset();
    update();
    if (changed) {
        Q_EMIT documentEdited();
        Q_EMIT statusHintChanged(QStringLiteral("Vertical space inserted"));
    }
}

void QtCanvas::cancelVerticalSpace() {
    if (this->documentController) {
        this->documentController->cancelVerticalSpace();
    }
    this->verticalSpacePreview.reset();
    update();
}

void QtCanvas::drawVerticalSpacePreview(QPainter& painter) const {
    if (!this->verticalSpacePreview) {
        return;
    }

    const auto rects = pageRects();
    if (this->verticalSpacePreview->pageIndex >= rects.size()) {
        return;
    }

    const auto& pageRect = rects[this->verticalSpacePreview->pageIndex];
    const double startY = pageRect.y() + this->verticalSpacePreview->startY;
    const double currentY = pageRect.y() + this->verticalSpacePreview->currentY;
    const QRectF band(pageRect.x(), std::min(startY, currentY), pageRect.width(), std::abs(currentY - startY));

    painter.save();
    painter.setPen(QPen(QColor(30, 100, 220), 1.5 / this->zoomFactor, Qt::DashLine));
    painter.setBrush(QColor(30, 100, 220, 40));
    if (band.height() > 0.0) {
        painter.drawRect(band);
    }
    painter.drawLine(QPointF(pageRect.x(), startY), QPointF(pageRect.right(), startY));
    painter.setPen(QPen(QColor(30, 100, 220), 2.0 / this->zoomFactor));
    painter.drawLine(QPointF(pageRect.x(), currentY), QPointF(pageRect.right(), currentY));
    painter.restore();
}

void QtCanvas::processTouchDrawing(const vn::ui::input::TouchEvent& event) {
    const auto touchAction = this->activeTouchAction.value_or(this->buttonMatrix.touchAction);
    if (!this->touchDrawingEnabled && touchAction == QtPointerButtonAction::None) {
        return;
    }

    if (event.points.empty()) {
        if (this->panning && touchAction == QtPointerButtonAction::Pan) {
            endPan();
            this->activeTouchPointId = -1;
        }
        if (this->erasing && touchAction == QtPointerButtonAction::Eraser) {
            if (!releasePointerAction(touchAction)) {
                finalizeErase();
                clearEraserPreview();
            }
            this->activeTouchPointId = -1;
        }
        if (this->drawing && this->activeTouchPointId >= 0) {
            finalizeActiveStroke();
            this->activeTouchPointId = -1;
        }
        return;
    }

    const auto* touchPoint = [&]() -> const vn::ui::input::TouchPoint* {
        if (this->activeTouchPointId >= 0) {
            for (const auto& point: event.points) {
                if (point.id == this->activeTouchPointId) {
                    return &point;
                }
            }
        }
        return &event.points.front();
    }();

    if (!touchPoint) {
        return;
    }

    const QPointF screenPoint(touchPoint->x, touchPoint->y);
    const double pressure = touchPoint->pressure > 0.0 ? touchPoint->pressure : 0.5;

    if (touchAction == QtPointerButtonAction::Pan) {
        if (!this->panning) {
            this->activeTouchPointId = static_cast<int>(touchPoint->id);
            beginPan(screenPoint);
            return;
        }
        if (this->activeTouchPointId == touchPoint->id) {
            const QPointF delta = screenPoint - this->lastPanScreenPosition;
            this->lastPanScreenPosition = screenPoint;
            this->scrollX -= delta.x() / this->zoomFactor;
            this->scrollY -= delta.y() / this->zoomFactor;
            emitViewportUpdate();
        }
        return;
    }

    if (touchAction == QtPointerButtonAction::Eraser) {
        if (!this->erasing) {
            this->activeTouchPointId = static_cast<int>(touchPoint->id);
            (void) beginPointerAction(touchAction, screenPoint, pressure);
            return;
        }
        if (this->activeTouchPointId == touchPoint->id) {
            eraseAtScreen(screenPoint);
        }
        return;
    }

    const bool drawTool = this->currentToolState.activeTool == QtToolType::Pen ||
                          this->currentToolState.activeTool == QtToolType::Highlighter ||
                          this->currentToolState.activeTool == QtToolType::LaserPointerPen ||
                          this->currentToolState.activeTool == QtToolType::LaserPointerHighlighter ||
                          this->currentToolState.activeTool == QtToolType::ShapeRecognizer;
    if (!drawTool) {
        return;
    }

    if (!this->drawing) {
        this->activeTouchPointId = static_cast<int>(touchPoint->id);
        beginStrokeAtScreen(screenPoint, pressure);
        return;
    }

    if (this->activeTouchPointId == touchPoint->id) {
        updateStrokeAtScreen(screenPoint, pressure);
    }
}

auto QtCanvas::handleTouchGesture(const QTouchEvent& event) -> bool {
    if (!this->zoomGesturesEnabled) {
        return false;
    }

    const auto points = event.points();
    if (points.size() < 2) {
        const bool wasGesture = this->touchZoomGestureActive || this->touchZoomInitialDistance > 0.0;
        this->touchZoomGestureActive = false;
        this->touchZoomInitialDistance = 0.0;
        this->touchZoomLastDistance = 0.0;
        return wasGesture;
    }

    const QPointF first = points[0].position();
    const QPointF second = points[1].position();
    const QPointF center = (first + second) / 2.0;
    const double distance = std::hypot(first.x() - second.x(), first.y() - second.y());
    if (distance <= 0.0) {
        return true;
    }

    if (this->touchZoomInitialDistance <= 0.0) {
        this->touchZoomInitialDistance = distance;
        this->touchZoomLastDistance = distance;
        this->touchZoomLastCenter = center;
        return true;
    }

    if (!this->touchZoomGestureActive &&
        std::abs(distance - this->touchZoomInitialDistance) < this->touchZoomStartThreshold) {
        this->touchZoomLastDistance = distance;
        this->touchZoomLastCenter = center;
        return true;
    }

    this->touchZoomGestureActive = true;
    const double factor = distance / std::max(this->touchZoomLastDistance, 1.0);
    const QPointF centerDelta = center - this->touchZoomLastCenter;
    const bool moved = std::abs(centerDelta.x()) > 0.1 || std::abs(centerDelta.y()) > 0.1;
    if (moved) {
        this->scrollX -= centerDelta.x() / std::max(0.001, this->zoomFactor);
        this->scrollY -= centerDelta.y() / std::max(0.001, this->zoomFactor);
    }
    if (std::abs(factor - 1.0) > 0.001) {
        zoomAroundScreenPoint(factor, center);
    } else if (moved) {
        emitViewportUpdate();
    }
    this->touchZoomLastDistance = distance;
    this->touchZoomLastCenter = center;
    return true;
}
