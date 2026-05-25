/*
 * VertexNote
 *
 * Qt preview geometry renderer for the Qt shell.
 */

#include "QtPreviewGeometryRenderer.h"

#include <algorithm>
#include <array>
#include <cmath>

#include <QColor>
#include <QPainter>
#include <QPen>
#include <QPointF>
#include <QPolygonF>
#include <QRectF>

#include "view/render/QtPainterRenderContext.h"

namespace {

constexpr std::array<double, 2> ConstructionDash{8.0, 6.0};
constexpr double ConstructionCenterlineInsetRatio = 0.6;
constexpr double Pi = 3.14159265358979323846;

auto toQColor(Color color) -> QColor { return QColor(color.red, color.green, color.blue); }

auto drawConstructionCircleHelper(QPainter* painter, const QPointF& center, double radius) -> void {
    const double helperExtent = radius * ConstructionCenterlineInsetRatio;
    painter->drawLine(QPointF(center.x() - helperExtent, center.y()), QPointF(center.x() + helperExtent, center.y()));
    painter->drawLine(QPointF(center.x(), center.y() - helperExtent), QPointF(center.x(), center.y() + helperExtent));
}

auto drawExtendedLine(QPainter* painter, const QPointF& start, const QPointF& end) -> void {
    if (!painter) {
        return;
    }

    const double dx = end.x() - start.x();
    const double dy = end.y() - start.y();
    const double length = std::hypot(dx, dy);
    if (length == 0.0) {
        painter->drawLine(start, end);
        return;
    }

    const QRectF clip = painter->clipBoundingRect();
    const double extent = std::hypot(clip.width(), clip.height()) + length;
    const double unitX = dx / length;
    const double unitY = dy / length;
    painter->drawLine(QPointF(start.x() - unitX * extent, start.y() - unitY * extent),
                      QPointF(start.x() + unitX * extent, start.y() + unitY * extent));
}

}  // namespace

namespace vn::view::render {

void QtPreviewGeometryRenderer::draw(const GeometryRenderModel& geometry, RenderContext& context) const {
    if (context.backend() != RenderBackend::QtPainter || (geometry.edges.empty() && geometry.faces.empty())) {
        return;
    }

    auto* painter = static_cast<QtPainterRenderContext&>(context).native();
    if (!painter) {
        return;
    }

    painter->save();
    QPen pen(toQColor(geometry.color), geometry.strokeWidth, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
    painter->setBrush(Qt::NoBrush);

    if (!geometry.faces.empty() && this->faceFillVisible && !this->wireframeViewEnabled) {
        QColor fillColor = toQColor(geometry.color);
        for (const auto& face: geometry.faces) {
            fillColor.setAlpha(std::clamp(face.fill, 0, 255));
            painter->setPen(Qt::NoPen);
            painter->setBrush(fillColor);
            for (const auto& triangle: face.triangles) {
                QPolygonF polygon;
                polygon.reserve(3);
                polygon << QPointF(triangle.a.x, triangle.a.y) << QPointF(triangle.b.x, triangle.b.y)
                        << QPointF(triangle.c.x, triangle.c.y);
                painter->drawPolygon(polygon);
            }
        }
        painter->setBrush(Qt::NoBrush);
    }

    if (!geometry.faces.empty() && this->wireframeViewEnabled) {
        QColor wireColor = toQColor(geometry.color);
        wireColor.setAlpha(165);
        QPen wirePen(wireColor, std::max(0.75, geometry.strokeWidth * 0.65), Qt::DashLine, Qt::RoundCap,
                     Qt::RoundJoin);
        painter->setPen(wirePen);
        painter->setBrush(Qt::NoBrush);
        for (const auto& face: geometry.faces) {
            for (const auto& triangle: face.triangles) {
                QPolygonF polygon;
                polygon.reserve(3);
                polygon << QPointF(triangle.a.x, triangle.a.y) << QPointF(triangle.b.x, triangle.b.y)
                        << QPointF(triangle.c.x, triangle.c.y);
                painter->drawPolygon(polygon);
            }
        }
    }

    for (const auto& edge: geometry.edges) {
        pen.setStyle(Qt::SolidLine);
        if (edge.kind == vn::geom::EdgeKind::ConstructionLine || edge.kind == vn::geom::EdgeKind::ConstructionCircle) {
            QList<qreal> pattern;
            pattern.reserve(static_cast<qsizetype>(ConstructionDash.size()));
            for (double value: ConstructionDash) {
                pattern.push_back(value);
            }
            pen.setDashPattern(pattern);
        } else {
            pen.setDashPattern({});
        }
        painter->setPen(pen);

        const QPointF start(edge.start.x, edge.start.y);
        const QPointF end(edge.end.x, edge.end.y);

        if ((edge.kind == vn::geom::EdgeKind::Arc || edge.kind == vn::geom::EdgeKind::ConstructionCircle) &&
            !edge.controls.empty()) {
            const QPointF center(edge.controls.front().x, edge.controls.front().y);
            const double radius = std::hypot(start.x() - center.x(), start.y() - center.y());
            if (radius > 0.0) {
                if (edge.closedLoop) {
                    painter->drawEllipse(center, radius, radius);
                    if (edge.kind == vn::geom::EdgeKind::ConstructionCircle) {
                        drawConstructionCircleHelper(painter, center, radius);
                    }
                    continue;
                }

                const double startAngle = std::atan2(start.y() - center.y(), start.x() - center.x());
                double endAngle = std::atan2(end.y() - center.y(), end.x() - center.x());
                if (endAngle <= startAngle) {
                    endAngle += 2.0 * Pi;
                }

                const QRectF arcRect(center.x() - radius, center.y() - radius, radius * 2.0, radius * 2.0);
                const int startAngle16 = static_cast<int>(std::round((-startAngle) * 180.0 / Pi * 16.0));
                const int spanAngle16 = static_cast<int>(std::round((-(endAngle - startAngle)) * 180.0 / Pi * 16.0));
                painter->drawArc(arcRect, startAngle16, spanAngle16);
                if (edge.kind == vn::geom::EdgeKind::ConstructionCircle) {
                    drawConstructionCircleHelper(painter, center, radius);
                }
                continue;
            }
        }

        if (edge.kind == vn::geom::EdgeKind::ConstructionLine) {
            drawExtendedLine(painter, start, end);
        } else {
            painter->drawLine(start, end);
        }
    }

    painter->restore();
}

void QtPreviewGeometryRenderer::setWireframeViewEnabled(bool enabled) { this->wireframeViewEnabled = enabled; }

void QtPreviewGeometryRenderer::setFaceFillVisible(bool visible) { this->faceFillVisible = visible; }

}  // namespace vn::view::render
