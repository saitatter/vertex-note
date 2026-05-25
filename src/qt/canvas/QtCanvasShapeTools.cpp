/*
 * VertexNote
 *
 * Qt canvas shape and instrument tools.
 */

#include "QtCanvas.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <utility>
#include <vector>

#include <QApplication>
#include <QPainter>
#include <QPainterPath>
#include <QPen>

namespace {

constexpr double ROTATION_SNAP_STEP_RADIANS = M_PI / 12.0;
constexpr double CM_TO_PT = 28.3464566929;
constexpr double INSTRUMENT_EDGE_HIT_BAND = 14.0;
constexpr double INSTRUMENT_INNER_BAND = 12.0;
constexpr double SHAPE_POINT_EPSILON = 1e-6;

auto qColorFromColor(Color color, int alphaOverride = -1) -> QColor {
    return QColor(color.red, color.green, color.blue, alphaOverride >= 0 ? alphaOverride : color.alpha);
}

auto snapColor(std::optional<vn::snap::SnapKind> kind) -> QColor {
    if (!kind) {
        return QColor(45, 125, 255);
    }

    switch (*kind) {
        case vn::snap::SnapKind::Grid:
            return QColor(90, 90, 90);
        case vn::snap::SnapKind::ExplicitVertex:
        case vn::snap::SnapKind::EdgeEndpoint:
            return QColor(0, 115, 255);
        case vn::snap::SnapKind::Midpoint:
            return QColor(0, 166, 89);
        case vn::snap::SnapKind::EdgeProjection:
            return QColor(255, 140, 20);
        case vn::snap::SnapKind::Intersection:
            return QColor(203, 30, 203);
        case vn::snap::SnapKind::ConstraintGuide:
            return QColor(0, 153, 191);
    }

    return QColor(45, 125, 255);
}

auto isVertexSnapKind(std::optional<vn::snap::SnapKind> kind) -> bool {
    return kind == vn::snap::SnapKind::ExplicitVertex || kind == vn::snap::SnapKind::EdgeEndpoint;
}

auto snapHint(std::optional<vn::snap::SnapKind> kind) -> QString {
    if (!kind) {
        return QStringLiteral("hit");
    }

    switch (*kind) {
        case vn::snap::SnapKind::Grid:
            return QStringLiteral("grid");
        case vn::snap::SnapKind::ExplicitVertex:
        case vn::snap::SnapKind::EdgeEndpoint:
            return QStringLiteral("vertex");
        case vn::snap::SnapKind::Midpoint:
            return QStringLiteral("midpoint");
        case vn::snap::SnapKind::EdgeProjection:
            return QStringLiteral("edge projection");
        case vn::snap::SnapKind::Intersection:
            return QStringLiteral("intersection");
        case vn::snap::SnapKind::ConstraintGuide:
            return QStringLiteral("constraint guide");
    }

    return QStringLiteral("hit");
}

auto sameShapePoint(const QPointF& lhs, const QPointF& rhs) -> bool {
    return std::hypot(lhs.x() - rhs.x(), lhs.y() - rhs.y()) <= SHAPE_POINT_EPSILON;
}

void drawSnapMarker(QPainter& painter, const QPointF& center, std::optional<vn::snap::SnapKind> kind,
                    int vertexMarkerSizePixels) {
    const double scale = std::max(0.1, painter.transform().m11());
    const QColor color = snapColor(kind);
    if (isVertexSnapKind(kind)) {
        const double halfSize = std::clamp(vertexMarkerSizePixels + 6, 12, 56) * 0.5 / scale;
        const QRectF rect(center.x() - halfSize, center.y() - halfSize, halfSize * 2.0, halfSize * 2.0);
        painter.setBrush(QColor(0, 115, 255, 44));
        painter.setPen(QPen(QColor(255, 255, 255, 245), 3.8 / scale));
        painter.drawRect(rect);
        painter.setPen(QPen(QColor(0, 92, 255), 2.4 / scale));
        painter.drawRect(rect.adjusted(0.7 / scale, 0.7 / scale, -0.7 / scale, -0.7 / scale));
        painter.setPen(Qt::NoPen);
        painter.setBrush(QColor(0, 92, 255, 235));
        const double dotSize = 3.4 / scale;
        painter.drawRect(QRectF(center.x() - dotSize / 2.0, center.y() - dotSize / 2.0, dotSize, dotSize));
        return;
    }

    const double radius = 4.8 / scale;
    painter.setPen(QPen(QColor(255, 255, 255, 235), 3.2 / scale));
    painter.setBrush(QColor(color.red(), color.green(), color.blue(), 40));
    painter.drawEllipse(center, radius, radius);
    painter.setPen(QPen(color, 1.4 / scale));
    painter.drawEllipse(center, radius, radius);
    painter.drawLine(QPointF(center.x() - radius, center.y()), QPointF(center.x() + radius, center.y()));
    painter.drawLine(QPointF(center.x(), center.y() - radius), QPointF(center.x(), center.y() + radius));
}

auto cubicSplinePath(const std::vector<QPointF>& points) -> QPainterPath {
    QPainterPath path;
    if (points.empty()) {
        return path;
    }

    path.moveTo(points.front());
    if (points.size() == 1U) {
        return path;
    }
    if (points.size() == 2U) {
        path.lineTo(points.back());
        return path;
    }

    for (std::size_t index = 0; index + 1 < points.size(); ++index) {
        const QPointF& p0 = index == 0 ? points[index] : points[index - 1];
        const QPointF& p1 = points[index];
        const QPointF& p2 = points[index + 1];
        const QPointF& p3 = (index + 2 < points.size()) ? points[index + 2] : points[index + 1];
        const QPointF c1(p1.x() + (p2.x() - p0.x()) / 6.0, p1.y() + (p2.y() - p0.y()) / 6.0);
        const QPointF c2(p2.x() - (p3.x() - p1.x()) / 6.0, p2.y() - (p3.y() - p1.y()) / 6.0);
        path.cubicTo(c1, c2, p2);
    }

    return path;
}

auto buildArrowPreviewPoints(const QPointF& start, const QPointF& end, double thickness, bool doubleEnded)
        -> std::vector<QPointF> {
    const double lineLength = std::hypot(end.x() - start.x(), end.y() - start.y());
    if (lineLength <= 0.0001) {
        return {start, end};
    }

    const double safeThickness = std::max(0.5, thickness);
    const double slimness = lineLength / safeThickness;
    double delta = M_PI / 6.0;
    constexpr double THICK1 = 7.0;
    constexpr double THICK3 = 1.6;
    constexpr double LENGTH2 = 0.4;
    constexpr double LENGTH4 = 0.8;
    constexpr double LENGTH4_DOUBLE = 0.5;
    double arrowDist = safeThickness * THICK1;
    if (slimness >= THICK1 / LENGTH2) {
        // keep default
    } else if (slimness >= THICK3 / LENGTH2) {
        arrowDist = lineLength * LENGTH2;
    } else if (slimness >= THICK3 / (doubleEnded ? LENGTH4_DOUBLE : LENGTH4)) {
        arrowDist = safeThickness * THICK3;
        delta = (1 + (slimness - THICK3 / LENGTH2) /
                            (THICK3 / (doubleEnded ? LENGTH4_DOUBLE : LENGTH4) - THICK3 / LENGTH2)) *
                M_PI / 6.0;
        arrowDist *= std::sin(M_PI / 6.0) / std::sin(delta);
    } else {
        arrowDist = lineLength * (doubleEnded ? LENGTH4_DOUBLE : LENGTH4);
        delta = M_PI / 3.0;
        arrowDist *= std::sin(M_PI / 6.0) / std::sin(M_PI / 3.0);
    }

    const double angle = std::atan2(end.y() - start.y(), end.x() - start.x());
    std::vector<QPointF> shape;
    shape.reserve(doubleEnded ? 10 : 6);
    shape.emplace_back(start);
    if (doubleEnded) {
        shape.emplace_back(start.x() + arrowDist * std::cos(angle + delta),
                           start.y() + arrowDist * std::sin(angle + delta));
        shape.emplace_back(start);
        shape.emplace_back(start.x() + arrowDist * std::cos(angle - delta),
                           start.y() + arrowDist * std::sin(angle - delta));
        shape.emplace_back(start);
    }
    shape.emplace_back(end);
    shape.emplace_back(end.x() - arrowDist * std::cos(angle + delta), end.y() - arrowDist * std::sin(angle + delta));
    shape.emplace_back(end);
    shape.emplace_back(end.x() - arrowDist * std::cos(angle - delta), end.y() - arrowDist * std::sin(angle - delta));
    shape.emplace_back(end);
    return shape;
}

auto buildCoordinateSystemPreviewPoints(const QPointF& start, const QPointF& current) -> std::vector<QPointF> {
    return {start, QPointF(start.x(), current.y()), QPointF(current.x(), current.y())};
}

auto instrumentDefaultSize(QtToolType tool) -> double {
    switch (tool) {
        case QtToolType::Setsquare:
            return 8.0 * CM_TO_PT;
        case QtToolType::Compass:
            return 3.0 * CM_TO_PT;
        default:
            return 4.0 * CM_TO_PT;
    }
}

auto rotatePoint(const QPointF& point, double angle) -> QPointF {
    const double c = std::cos(angle);
    const double s = std::sin(angle);
    return QPointF(point.x() * c - point.y() * s, point.x() * s + point.y() * c);
}

auto toLocalInstrument(const QPointF& point, const QPointF& origin, double rotation) -> QPointF {
    return rotatePoint(point - origin, -rotation);
}

auto fromLocalInstrument(const QPointF& point, const QPointF& origin, double rotation) -> QPointF {
    return origin + rotatePoint(point, rotation);
}

auto setsquareHalfSpan(double size) -> double { return size / std::sqrt(2.0); }

auto setsquareRadius(double size) -> double { return std::max(0.0, setsquareHalfSpan(size) - 1.15 * CM_TO_PT); }

auto insideSetsquare(const QPointF& local, double size) -> bool {
    const double halfSpan = setsquareHalfSpan(size);
    return local.y() <= 0.0 && local.y() >= local.x() - halfSpan && local.y() >= -local.x() - halfSpan;
}

auto insideCompass(const QPointF& local, double size) -> bool { return std::hypot(local.x(), local.y()) <= size; }

auto buildSetsquareOutline(const QPointF& origin, double rotation, double size) -> std::array<QPointF, 4> {
    const double halfSpan = setsquareHalfSpan(size);
    return {fromLocalInstrument(QPointF(-halfSpan, 0.0), origin, rotation),
            fromLocalInstrument(QPointF(halfSpan, 0.0), origin, rotation),
            fromLocalInstrument(QPointF(0.0, -halfSpan), origin, rotation),
            fromLocalInstrument(QPointF(-halfSpan, 0.0), origin, rotation)};
}

auto buildSetsquareStrokePoints(bool edgeStroke, const QPointF& origin, double rotation, double size, double a,
                                double b) -> std::vector<QPointF> {
    if (edgeStroke) {
        return {fromLocalInstrument(QPointF(a, 0.0), origin, rotation),
                fromLocalInstrument(QPointF(b, 0.0), origin, rotation)};
    }

    const double radius = std::min(std::max(0.0, b), setsquareRadius(size));
    return {origin, fromLocalInstrument(QPointF(radius * std::cos(a), -radius * std::sin(a)), origin, rotation)};
}

auto normalizeAngleDelta(double previousAngle, double angle) -> double {
    return previousAngle + std::remainder(angle - previousAngle, 2.0 * M_PI);
}

auto buildCompassArcPoints(const QPointF& origin, double rotation, double radius, double angleMin, double angleMax)
        -> std::vector<QPointF> {
    std::vector<QPointF> points;
    const double clampedMax = std::min(angleMax, angleMin + 2.0 * M_PI);
    const int samples = 100;
    points.reserve(samples + 1);
    for (int i = 0; i <= samples; ++i) {
        const double angle = angleMin + (static_cast<double>(i) / samples) * (clampedMax - angleMin);
        points.push_back(fromLocalInstrument(QPointF(radius * std::cos(angle), -radius * std::sin(angle)), origin, rotation));
    }
    return points;
}

auto buildCompassRadiusPoints(const QPointF& origin, double rotation, double radiusMin, double radiusMax)
        -> std::vector<QPointF> {
    return {fromLocalInstrument(QPointF(radiusMin, 0.0), origin, rotation),
            fromLocalInstrument(QPointF(radiusMax, 0.0), origin, rotation)};
}

}  // namespace

// ---------------------------------------------------------------------------
// Shape drawing
// ---------------------------------------------------------------------------

void QtCanvas::beginShapeAtScreen(const QPointF& screenPoint) {
    const QPointF scenePoint = screenToScene(screenPoint);
    const auto pageIdx = pageIndexAtScenePoint(scenePoint);
    if (!pageIdx) {
        return;
    }

    const auto rects = pageRects();
    this->shapePageIndex = *pageIdx;
    this->shapeStartScene = snapShapePoint(
            *pageIdx, QPointF(scenePoint.x() - rects[*pageIdx].x(), scenePoint.y() - rects[*pageIdx].y()));
    this->shapeCurrentScene = this->shapeStartScene;
    this->shapeSnapPoint = this->shapeStartScene;
    this->shapeClickPoints.clear();
    this->shapeClickPoints.push_back(this->shapeStartScene);
    this->shapeDirectionModifiersFixed = false;
    this->shapeDirectionModifierShift = false;
    this->shapeDirectionModifierControl = false;
    this->shapeEffectiveShiftModifier = false;
    this->shapeEffectiveControlModifier = false;
    this->shapeDrawing = true;
    setCursor(Qt::CrossCursor);
    update();
}

void QtCanvas::updateShapeAtScreen(const QPointF& screenPoint) {
    const QPointF scenePoint = screenToScene(screenPoint);
    const auto rects = pageRects();
    if (this->shapePageIndex < rects.size()) {
        QPointF pagePoint(scenePoint.x() - rects[this->shapePageIndex].x(),
                          scenePoint.y() - rects[this->shapePageIndex].y());
        if (this->rotationSnapEnabled) {
            const QPointF origin =
                    this->shapeClickPoints.empty() ? this->shapeStartScene : this->shapeClickPoints.back();
            pagePoint = applyRotationSnap(origin, pagePoint);
        }
        pagePoint = applyShapeDirectionModifiers(pagePoint);
        pagePoint = snapShapePoint(this->shapePageIndex, pagePoint);
        this->shapeCurrentScene = pagePoint;
    }
    update();
}

auto QtCanvas::snapShapePoint(std::size_t pageIndex, const QPointF& pagePoint) -> QPointF {
    const bool vertexWorkflow = this->currentToolState.isVertexDrawingTool();
    const bool strokeWorkflow = this->currentToolState.isStrokeDrawingTool();
    this->shapeSnapPoint =
            snapInputPagePoint(pageIndex, pagePoint, &this->shapeSnapKind, vertexWorkflow,
                               vertexWorkflow || strokeWorkflow);
    if (this->shapeSnapKind) {
        Q_EMIT statusHintChanged(QStringLiteral("Snap: %1").arg(snapHint(this->shapeSnapKind)));
    }
    return this->shapeSnapPoint;
}

auto QtCanvas::applyShapeDirectionModifiers(const QPointF& pagePoint) -> QPointF {
    const auto tool = this->currentToolState.activeTool;
    const bool supportedTool = tool == QtToolType::DrawRectangle || tool == QtToolType::DrawEllipse ||
                               tool == QtToolType::DrawCoordinateSystem;
    if (!supportedTool) {
        return pagePoint;
    }

    const double dx = pagePoint.x() - this->shapeStartScene.x();
    const double dy = pagePoint.y() - this->shapeStartScene.y();
    const auto keyboardModifiers = QApplication::keyboardModifiers();
    bool shift = keyboardModifiers.testFlag(Qt::ShiftModifier);
    bool control = keyboardModifiers.testFlag(Qt::ControlModifier);
    if (this->drawDirectionModifiersEnabled) {
        if (!this->shapeDirectionModifiersFixed) {
            this->shapeDirectionModifierShift = dx < 0.0;
            this->shapeDirectionModifierControl = dy < 0.0;
            const double lockDistance = this->drawDirectionModifiersRadiusPixels / std::max(0.001, this->zoomFactor);
            this->shapeDirectionModifiersFixed = std::abs(dx) > lockDistance || std::abs(dy) > lockDistance;
        }
        shift = shift != this->shapeDirectionModifierShift;
        control = control != this->shapeDirectionModifierControl;
    }
    this->shapeEffectiveShiftModifier = shift;
    this->shapeEffectiveControlModifier = control;

    double width = dx;
    double height = dy;
    if (shift) {
        const double size = std::max(std::abs(width), std::abs(height));
        width = std::copysign(size, width == 0.0 ? 1.0 : width);
        height = std::copysign(size, height == 0.0 ? 1.0 : height);
    }

    return QPointF(this->shapeStartScene.x() + width, this->shapeStartScene.y() + height);
}

void QtCanvas::addShapeClickAtScreen(const QPointF& screenPoint) {
    const QPointF scenePoint = screenToScene(screenPoint);
    const auto rects = pageRects();
    if (this->shapePageIndex < rects.size()) {
        QPointF pagePoint(scenePoint.x() - rects[this->shapePageIndex].x(),
                          scenePoint.y() - rects[this->shapePageIndex].y());
        if (this->rotationSnapEnabled && !this->shapeClickPoints.empty()) {
            pagePoint = applyRotationSnap(this->shapeClickPoints.back(), pagePoint);
        }
        pagePoint = snapShapePoint(this->shapePageIndex, pagePoint);
        if (this->shapeClickPoints.empty() || !sameShapePoint(this->shapeClickPoints.back(), pagePoint)) {
            this->shapeClickPoints.push_back(pagePoint);
        }
        this->shapeCurrentScene = pagePoint;
    }

    // For arc: finalize after 3 clicks (center, start, end)
    if (this->currentToolState.activeTool == QtToolType::DrawArc && this->shapeClickPoints.size() >= 3U) {
        finalizeShape();
        return;
    }
    update();
}

void QtCanvas::finalizeShape() {
    if (!this->documentController || !this->shapeDrawing) {
        cancelShape();
        return;
    }

    const Color color = this->currentToolState.penColor;
    const double width = this->currentToolState.penWidth;
    const std::string& lineStyle = this->currentToolState.penLineStyle;
    const int fill = this->currentToolState.fillEnabled ? this->currentToolState.fillOpacity : -1;
    const Element* created = nullptr;
    const auto centeredStart = [&]() {
        return QPointF(2.0 * this->shapeStartScene.x() - this->shapeCurrentScene.x(),
                       2.0 * this->shapeStartScene.y() - this->shapeCurrentScene.y());
    };

    switch (this->currentToolState.activeTool) {
        case QtToolType::DrawLine:
            created = this->documentController->createLine(this->shapePageIndex, this->shapeStartScene.x(),
                                                           this->shapeStartScene.y(), this->shapeCurrentScene.x(),
                                                           this->shapeCurrentScene.y(), color, width, lineStyle);
            break;
        case QtToolType::DrawEdge:
            created = this->documentController->createEdge(this->shapePageIndex, this->shapeStartScene.x(),
                                                           this->shapeStartScene.y(), this->shapeCurrentScene.x(),
                                                           this->shapeCurrentScene.y(), color, width);
            break;
        case QtToolType::DrawRectangle: {
            const QPointF start = this->shapeEffectiveControlModifier ? centeredStart() : this->shapeStartScene;
            created = this->documentController->createRectangle(this->shapePageIndex, start.x(), start.y(),
                                                                this->shapeCurrentScene.x(), this->shapeCurrentScene.y(),
                                                                color, width, fill);
            break;
        }
        case QtToolType::DrawCircle:
            created = this->documentController->createCircle(this->shapePageIndex, this->shapeStartScene.x(),
                                                             this->shapeStartScene.y(), this->shapeCurrentScene.x(),
                                                             this->shapeCurrentScene.y(), color, width);
            break;
        case QtToolType::DrawEllipse: {
            const QPointF start = this->shapeEffectiveControlModifier ? centeredStart() : this->shapeStartScene;
            created = this->documentController->createEllipse(
                    this->shapePageIndex, start.x(), start.y(),
                    this->shapeCurrentScene.x(), this->shapeCurrentScene.y(), color, width, lineStyle, fill);
            break;
        }
        case QtToolType::DrawArrow:
            created = this->documentController->createArrow(
                    this->shapePageIndex, this->shapeStartScene.x(), this->shapeStartScene.y(),
                    this->shapeCurrentScene.x(), this->shapeCurrentScene.y(), color, width, lineStyle, false);
            break;
        case QtToolType::DrawDoubleArrow:
            created = this->documentController->createArrow(
                    this->shapePageIndex, this->shapeStartScene.x(), this->shapeStartScene.y(),
                    this->shapeCurrentScene.x(), this->shapeCurrentScene.y(), color, width, lineStyle, true);
            break;
        case QtToolType::DrawCoordinateSystem:
            created = this->documentController->createCoordinateSystem(
                    this->shapePageIndex, this->shapeStartScene.x(), this->shapeStartScene.y(),
                    this->shapeCurrentScene.x(), this->shapeCurrentScene.y(), color, width, lineStyle);
            break;
        case QtToolType::DrawArc:
            if (this->shapeClickPoints.size() >= 3U) {
                created = this->documentController->createArc(
                        this->shapePageIndex, this->shapeClickPoints[0].x(), this->shapeClickPoints[0].y(),
                        this->shapeClickPoints[1].x(), this->shapeClickPoints[1].y(),
                        this->shapeClickPoints[2].x(), this->shapeClickPoints[2].y(), color, width);
            }
            break;
        case QtToolType::DrawPolyline: {
            std::vector<std::pair<double, double>> points;
            points.reserve(this->shapeClickPoints.size());
            for (const auto& pt: this->shapeClickPoints) {
                points.emplace_back(pt.x(), pt.y());
            }
            created = this->documentController->createPolyline(this->shapePageIndex, points, color, width, fill);
            break;
        }
        case QtToolType::DrawSpline: {
            std::vector<std::pair<double, double>> points;
            points.reserve(this->shapeClickPoints.size());
            for (const auto& pt: this->shapeClickPoints) {
                points.emplace_back(pt.x(), pt.y());
            }
            created = this->documentController->createSpline(this->shapePageIndex, points, color, width, lineStyle);
            break;
        }
        case QtToolType::DrawConstructionLine:
            created = this->documentController->createConstructionLine(
                    this->shapePageIndex, this->shapeStartScene.x(), this->shapeStartScene.y(),
                    this->shapeCurrentScene.x(), this->shapeCurrentScene.y(), color, width);
            break;
        case QtToolType::DrawConstructionCircle:
            created = this->documentController->createConstructionCircle(
                    this->shapePageIndex, this->shapeStartScene.x(), this->shapeStartScene.y(),
                    this->shapeCurrentScene.x(), this->shapeCurrentScene.y(), color, width);
            break;
        default:
            break;
    }

    this->shapeDrawing = false;
    this->shapeClickPoints.clear();
    this->shapeSnapKind.reset();
    setCursor(Qt::ArrowCursor);

    if (created) {
        Q_EMIT documentEdited();
    }
    update();
}

void QtCanvas::cancelShape() {
    this->shapeDrawing = false;
    this->shapeClickPoints.clear();
    this->shapeSnapKind.reset();
    refreshToolCursor();
    update();
}

void QtCanvas::drawShapePreview(QPainter& painter) const {
    if (!this->shapeDrawing) {
        return;
    }

    const auto rects = pageRects();
    if (this->shapePageIndex >= rects.size()) {
        return;
    }

    painter.save();
    const auto& pageRect = rects[this->shapePageIndex];
    painter.translate(pageRect.x(), pageRect.y());

    QPen pen(QColor(this->currentToolState.penColor.red, this->currentToolState.penColor.green,
                    this->currentToolState.penColor.blue, 180),
             this->currentToolState.penWidth);
    pen.setStyle(Qt::DashLine);
    painter.setPen(pen);
    painter.setBrush(Qt::NoBrush);

    const QPointF start = this->shapeStartScene;
    const QPointF current = this->shapeCurrentScene;
    const auto centeredStart = [&]() {
        return QPointF(2.0 * start.x() - current.x(), 2.0 * start.y() - current.y());
    };

    switch (this->currentToolState.activeTool) {
        case QtToolType::DrawLine:
        case QtToolType::DrawEdge:
        case QtToolType::DrawConstructionLine:
            painter.drawLine(start, current);
            break;
        case QtToolType::DrawRectangle:
            painter.drawRect(QRectF(this->shapeEffectiveControlModifier ? centeredStart() : start, current).normalized());
            break;
        case QtToolType::DrawCircle:
        case QtToolType::DrawConstructionCircle: {
            const double radius = std::hypot(current.x() - start.x(), current.y() - start.y());
            painter.drawEllipse(start, radius, radius);
            break;
        }
        case QtToolType::DrawEllipse:
            painter.drawEllipse(QRectF(this->shapeEffectiveControlModifier ? centeredStart() : start, current).normalized());
            break;
        case QtToolType::DrawArrow:
        case QtToolType::DrawDoubleArrow: {
            const auto points =
                    buildArrowPreviewPoints(start, current, this->currentToolState.penWidth,
                                            this->currentToolState.activeTool == QtToolType::DrawDoubleArrow);
            for (std::size_t i = 1; i < points.size(); ++i) {
                painter.drawLine(points[i - 1], points[i]);
            }
            break;
        }
        case QtToolType::DrawCoordinateSystem: {
            const auto points = buildCoordinateSystemPreviewPoints(start, current);
            for (std::size_t i = 1; i < points.size(); ++i) {
                painter.drawLine(points[i - 1], points[i]);
            }
            break;
        }
        case QtToolType::DrawArc:
            if (this->shapeClickPoints.size() == 1U) {
                painter.drawLine(this->shapeClickPoints[0], current);
            } else if (this->shapeClickPoints.size() >= 2U) {
                const QPointF& center = this->shapeClickPoints[0];
                const QPointF& arcStart = this->shapeClickPoints[1];
                const double radius = std::hypot(arcStart.x() - center.x(), arcStart.y() - center.y());
                painter.drawEllipse(center, radius, radius);
                painter.drawLine(center, current);
            }
            break;
        case QtToolType::DrawPolyline:
            for (std::size_t i = 1; i < this->shapeClickPoints.size(); ++i) {
                painter.drawLine(this->shapeClickPoints[i - 1], this->shapeClickPoints[i]);
            }
            if (!this->shapeClickPoints.empty()) {
                painter.drawLine(this->shapeClickPoints.back(), current);
            }
            break;
        case QtToolType::DrawSpline: {
            std::vector<QPointF> points = this->shapeClickPoints;
            if (points.empty() || points.back() != current) {
                points.push_back(current);
            }
            painter.drawPath(cubicSplinePath(points));
            break;
        }
        default:
            break;
    }

    // Draw vertex handles
    painter.setPen(Qt::NoPen);
    painter.setBrush(qColorFromColor(this->selectionColor, 200));
    const double handleRadius = 3.0 / painter.transform().m11();
    for (const auto& pt: this->shapeClickPoints) {
        painter.drawEllipse(pt, handleRadius, handleRadius);
    }
    if (this->shapeSnapKind) {
        drawSnapMarker(painter, this->shapeSnapPoint, this->shapeSnapKind, this->vertexSnapMarkerSize);
    }

    painter.restore();
}

void QtCanvas::drawInstrumentOverlay(QPainter& painter) const {
    const auto instrument = activeInstrumentTool();
    if (instrument == InstrumentToolKind::None || !this->instrumentOverlay.visible) {
        return;
    }

    const auto rects = pageRects();
    if (this->instrumentOverlay.pageIndex >= rects.size()) {
        return;
    }

    painter.save();
    const auto& pageRect = rects[this->instrumentOverlay.pageIndex];
    painter.translate(pageRect.x(), pageRect.y());

    QPen framePen(QColor(75, 104, 224, 190), 1.2 / std::max(0.1, this->zoomFactor));
    framePen.setCosmetic(true);
    painter.setPen(framePen);
    painter.setBrush(QColor(120, 155, 255, 24));

    if (instrument == InstrumentToolKind::Setsquare) {
        const auto outline = buildSetsquareOutline(this->instrumentOverlay.origin, this->instrumentOverlay.rotation,
                                                   this->instrumentOverlay.size);
        QPainterPath outlinePath;
        outlinePath.moveTo(outline.front());
        for (std::size_t i = 1; i < outline.size(); ++i) {
            outlinePath.lineTo(outline[i]);
        }
        painter.drawPath(outlinePath);

        QPen guidePen(QColor(70, 70, 190, 150), 1.0 / std::max(0.1, this->zoomFactor), Qt::DashLine);
        guidePen.setCosmetic(true);
        painter.setPen(guidePen);
        const double radius = setsquareRadius(this->instrumentOverlay.size);
        painter.drawArc(QRectF(this->instrumentOverlay.origin.x() - radius, this->instrumentOverlay.origin.y() - radius,
                               2.0 * radius, 2.0 * radius),
                        0, 180 * 16);
    } else {
        const double radius = this->instrumentOverlay.size;
        painter.drawEllipse(this->instrumentOverlay.origin, radius, radius);

        QPen guidePen(QColor(70, 70, 190, 150), 1.0 / std::max(0.1, this->zoomFactor), Qt::DashLine);
        guidePen.setCosmetic(true);
        painter.setPen(guidePen);
        const QPointF left = fromLocalInstrument(QPointF(0.0, 0.0), this->instrumentOverlay.origin,
                                                 this->instrumentOverlay.rotation);
        const QPointF right = fromLocalInstrument(QPointF(radius, 0.0), this->instrumentOverlay.origin,
                                                  this->instrumentOverlay.rotation);
        painter.drawLine(left, right);
    }

    painter.setPen(Qt::NoPen);
    painter.setBrush(QColor(45, 125, 255, 220));
    const double handleRadius = 4.0 / std::max(0.1, this->zoomFactor);
    painter.drawEllipse(this->instrumentOverlay.origin, handleRadius, handleRadius);

    if (this->activeInstrumentStroke && this->activeInstrumentStroke->pageIndex == this->instrumentOverlay.pageIndex &&
        this->activeInstrumentStroke->previewPoints.size() >= 2U) {
        QPen previewPen(QColor(this->currentToolState.penColor.red, this->currentToolState.penColor.green,
                               this->currentToolState.penColor.blue, 220),
                        std::max(1.0, this->currentToolState.penWidth));
        previewPen.setCosmetic(false);
        painter.setPen(previewPen);
        painter.setBrush(Qt::NoBrush);
        for (std::size_t i = 1; i < this->activeInstrumentStroke->previewPoints.size(); ++i) {
            painter.drawLine(this->activeInstrumentStroke->previewPoints[i - 1],
                             this->activeInstrumentStroke->previewPoints[i]);
        }
    }

    painter.restore();
}

auto QtCanvas::activeInstrumentTool() const -> InstrumentToolKind {
    switch (this->currentToolState.activeTool) {
        case QtToolType::Setsquare:
            return InstrumentToolKind::Setsquare;
        case QtToolType::Compass:
            return InstrumentToolKind::Compass;
        default:
            return InstrumentToolKind::None;
    }
}

void QtCanvas::ensureInstrumentOverlay(std::size_t pageIndex, const QPointF& pagePoint) {
    this->instrumentOverlay.visible = true;
    this->instrumentOverlay.pageIndex = pageIndex;
    this->instrumentOverlay.origin = pagePoint;
    if (this->instrumentOverlay.size <= 0.0) {
        this->instrumentOverlay.size = instrumentDefaultSize(this->currentToolState.activeTool);
    }
}

void QtCanvas::beginInstrumentToolAtScreen(const QPointF& screenPoint, Qt::MouseButton button) {
    const QPointF scenePoint = screenToScene(screenPoint);
    const auto pageIdx = pageIndexAtScenePoint(scenePoint);
    if (!pageIdx) {
        return;
    }

    const auto rects = pageRects();
    if (*pageIdx >= rects.size()) {
        return;
    }

    const QPointF pagePoint(scenePoint.x() - rects[*pageIdx].x(), scenePoint.y() - rects[*pageIdx].y());
    ensureInstrumentOverlay(*pageIdx, this->instrumentOverlay.visible && this->instrumentOverlay.pageIndex == *pageIdx
                                              ? this->instrumentOverlay.origin
                                              : pagePoint);

    if (button == Qt::RightButton) {
        this->movingInstrumentOverlay = true;
        this->instrumentOverlay.pageIndex = *pageIdx;
        this->instrumentOverlay.origin = pagePoint;
        update();
        return;
    }

    const QPointF local =
            toLocalInstrument(pagePoint, this->instrumentOverlay.origin, this->instrumentOverlay.rotation);
    if (activeInstrumentTool() == InstrumentToolKind::Setsquare) {
        if (!insideSetsquare(local, this->instrumentOverlay.size)) {
            this->instrumentOverlay.pageIndex = *pageIdx;
            this->instrumentOverlay.origin = pagePoint;
            update();
            return;
        }

        InstrumentStrokeState state;
        state.pageIndex = *pageIdx;
        state.origin = this->instrumentOverlay.origin;
        state.rotation = this->instrumentOverlay.rotation;
        state.size = this->instrumentOverlay.size;
        if (local.y() >= -this->instrumentOverlay.size * 0.2) {
            state.kind = InstrumentStrokeKind::SetsquareEdge;
            state.extentMin = local.x();
            state.extentMax = local.x();
            state.previewPoints =
                    buildSetsquareStrokePoints(true, state.origin, state.rotation,
                                               state.size, state.extentMin, state.extentMax);
        } else {
            state.kind = InstrumentStrokeKind::SetsquareRadial;
            state.anchor = std::atan2(-local.y(), local.x());
            state.extentMax = std::hypot(local.x(), local.y());
            state.previewPoints =
                    buildSetsquareStrokePoints(false, state.origin, state.rotation,
                                               state.size, state.anchor, state.extentMax);
        }
        this->activeInstrumentStroke = std::move(state);
    } else {
        if (!insideCompass(local, this->instrumentOverlay.size)) {
            this->instrumentOverlay.pageIndex = *pageIdx;
            this->instrumentOverlay.origin = pagePoint;
            update();
            return;
        }

        InstrumentStrokeState state;
        state.pageIndex = *pageIdx;
        state.origin = this->instrumentOverlay.origin;
        state.rotation = this->instrumentOverlay.rotation;
        state.size = this->instrumentOverlay.size;
        const double radius = std::hypot(local.x(), local.y());
        if (std::abs(radius - this->instrumentOverlay.size) <= INSTRUMENT_EDGE_HIT_BAND / std::max(0.1, this->zoomFactor)) {
            state.kind = InstrumentStrokeKind::CompassOutline;
            state.anchor = std::atan2(-local.y(), local.x());
            state.extentMin = state.anchor;
            state.extentMax = state.anchor;
            state.lastAngle = state.anchor;
            state.previewPoints = buildCompassArcPoints(state.origin, state.rotation, state.size, state.extentMin,
                                                        state.extentMax);
        } else if (std::abs(local.y()) <= INSTRUMENT_INNER_BAND / std::max(0.1, this->zoomFactor) &&
                   local.x() >= 0.0 && local.x() <= this->instrumentOverlay.size) {
            state.kind = InstrumentStrokeKind::CompassRadius;
            state.extentMin = local.x();
            state.extentMax = local.x();
            state.previewPoints = buildCompassRadiusPoints(state.origin, state.rotation, state.extentMin,
                                                           state.extentMax);
        } else {
            this->instrumentOverlay.pageIndex = *pageIdx;
            this->instrumentOverlay.origin = pagePoint;
            update();
            return;
        }
        this->activeInstrumentStroke = std::move(state);
    }
    update();
}

void QtCanvas::updateInstrumentToolAtScreen(const QPointF& screenPoint) {
    const QPointF scenePoint = screenToScene(screenPoint);
    const auto rects = pageRects();

    if (this->movingInstrumentOverlay) {
        if (this->instrumentOverlay.pageIndex < rects.size()) {
            this->instrumentOverlay.origin = QPointF(scenePoint.x() - rects[this->instrumentOverlay.pageIndex].x(),
                                                     scenePoint.y() - rects[this->instrumentOverlay.pageIndex].y());
            update();
        }
        return;
    }

    if (!this->activeInstrumentStroke || this->activeInstrumentStroke->pageIndex >= rects.size()) {
        return;
    }

    const QPointF pagePoint(scenePoint.x() - rects[this->activeInstrumentStroke->pageIndex].x(),
                            scenePoint.y() - rects[this->activeInstrumentStroke->pageIndex].y());
    const QPointF local =
            toLocalInstrument(pagePoint, this->activeInstrumentStroke->origin, this->activeInstrumentStroke->rotation);
    auto& state = *this->activeInstrumentStroke;
    switch (state.kind) {
        case InstrumentStrokeKind::SetsquareEdge:
            state.extentMin = std::min(state.extentMin, local.x());
            state.extentMax = std::max(state.extentMax, local.x());
            state.previewPoints = buildSetsquareStrokePoints(true, state.origin,
                                                             state.rotation, state.size, state.extentMin,
                                                             state.extentMax);
            break;
        case InstrumentStrokeKind::SetsquareRadial:
            state.extentMax = std::hypot(local.x(), local.y());
            state.previewPoints = buildSetsquareStrokePoints(false, state.origin,
                                                             state.rotation, state.size, state.anchor,
                                                             state.extentMax);
            break;
        case InstrumentStrokeKind::CompassOutline: {
            const double angle = normalizeAngleDelta(state.lastAngle, std::atan2(-local.y(), local.x()));
            state.extentMin = std::min(state.extentMin, angle);
            state.extentMax = std::max(state.extentMax, angle);
            state.lastAngle = angle;
            state.previewPoints = buildCompassArcPoints(state.origin, state.rotation, state.size, state.extentMin,
                                                        state.extentMax);
            break;
        }
        case InstrumentStrokeKind::CompassRadius:
            state.extentMin = std::min(state.extentMin, std::max(0.0, local.x()));
            state.extentMax = std::max(state.extentMax, std::max(0.0, local.x()));
            state.previewPoints = buildCompassRadiusPoints(state.origin, state.rotation, state.extentMin,
                                                           state.extentMax);
            break;
        default:
            break;
    }
    update();
}

void QtCanvas::finalizeInstrumentTool() {
    if (!this->documentController || !this->activeInstrumentStroke) {
        return;
    }

    std::vector<std::pair<double, double>> points;
    points.reserve(this->activeInstrumentStroke->previewPoints.size());
    for (const auto& point: this->activeInstrumentStroke->previewPoints) {
        points.emplace_back(point.x(), point.y());
    }

    const Element* created = nullptr;
    if (this->currentToolState.activeTool == QtToolType::Setsquare) {
        created = this->documentController->createSetsquareStroke(this->activeInstrumentStroke->pageIndex, points,
                                                                  this->currentToolState.penColor,
                                                                  this->currentToolState.penWidth,
                                                                  this->currentToolState.penLineStyle);
    } else if (this->currentToolState.activeTool == QtToolType::Compass) {
        created = this->documentController->createCompassStroke(this->activeInstrumentStroke->pageIndex, points,
                                                                this->currentToolState.penColor,
                                                                this->currentToolState.penWidth,
                                                                this->currentToolState.penLineStyle);
    }

    this->activeInstrumentStroke.reset();
    if (created) {
        Q_EMIT documentEdited();
    }
    update();
}

void QtCanvas::cancelInstrumentTool() {
    this->activeInstrumentStroke.reset();
    this->movingInstrumentOverlay = false;
    update();
}

auto QtCanvas::isMultiClickShapeTool() const -> bool {
    return this->currentToolState.activeTool == QtToolType::DrawPolyline ||
           this->currentToolState.activeTool == QtToolType::DrawArc ||
           this->currentToolState.activeTool == QtToolType::DrawSpline;
}

auto QtCanvas::applyRotationSnap(const QPointF& origin, const QPointF& point) const -> QPointF {
    const QPointF delta = point - origin;
    const double length = std::hypot(delta.x(), delta.y());
    if (length <= 0.0001) {
        return point;
    }

    const double angle = std::atan2(delta.y(), delta.x());
    const double snappedAngle = std::round(angle / ROTATION_SNAP_STEP_RADIANS) * ROTATION_SNAP_STEP_RADIANS;
    if (std::abs(angle - snappedAngle) > this->rotationSnapTolerance) {
        return point;
    }
    return QPointF(origin.x() + std::cos(snappedAngle) * length, origin.y() + std::sin(snappedAngle) * length);
}

