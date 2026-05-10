/*
 * VertexNote
 *
 * Qt preview stroke renderer for the Qt shell.
 */

#include "QtPreviewStrokeRenderer.h"

#include <algorithm>
#include <cmath>

#include <QBrush>
#include <QColor>
#include <QPainter>
#include <QPainterPath>
#include <QPen>
#include <QPointF>
#include <Qt>

#include "view/render/QtPainterRenderContext.h"

namespace {

auto toQColor(Color color, double alpha = 1.0) -> QColor {
    return QColor(color.red, color.green, color.blue, static_cast<int>(std::clamp(alpha, 0.0, 1.0) * 255.0));
}

auto toPenCapStyle(int capStyle) -> Qt::PenCapStyle {
    switch (capStyle) {
        case 1:
            return Qt::FlatCap;
        case 2:
            return Qt::SquareCap;
        case 0:
        default:
            return Qt::RoundCap;
    }
}

auto toPainterPath(const std::vector<Point>& points) -> QPainterPath {
    QPainterPath path;
    if (points.empty()) {
        return path;
    }

    path.setFillRule(Qt::WindingFill);

    path.moveTo(points.front().x, points.front().y);
    for (std::size_t i = 1; i < points.size(); ++i) {
        path.lineTo(points[i].x, points[i].y);
    }
    return path;
}

/// Build a filled outline path for a variable-width stroke.
/// Each point's .z stores the width at that point.
auto buildVariableWidthOutline(const std::vector<Point>& points, double fallbackWidth) -> QPainterPath {
    QPainterPath outline;
    outline.setFillRule(Qt::WindingFill);
    if (points.size() < 2) {
        if (points.size() == 1) {
            const double r = (points[0].z > 0.0 ? points[0].z : fallbackWidth) * 0.5;
            outline.addEllipse(QPointF(points[0].x, points[0].y), r, r);
        }
        return outline;
    }

    const auto n = points.size();

    // Compute per-point half-widths
    std::vector<double> halfW(n);
    for (std::size_t i = 0; i < n; ++i) {
        halfW[i] = (points[i].z > 0.0 ? points[i].z : fallbackWidth) * 0.5;
    }

    // Compute per-segment normals
    struct Vec2 {
        double x, y;
    };
    std::vector<Vec2> segNormals(n - 1);
    for (std::size_t i = 0; i + 1 < n; ++i) {
        const double dx = points[i + 1].x - points[i].x;
        const double dy = points[i + 1].y - points[i].y;
        const double len = std::hypot(dx, dy);
        if (len > 1e-9) {
            segNormals[i] = {-dy / len, dx / len};
        } else {
            segNormals[i] = {0.0, 1.0};
        }
    }

    // Compute per-point normals (average of adjacent segment normals)
    std::vector<Vec2> normals(n);
    normals[0] = segNormals[0];
    normals[n - 1] = segNormals[n - 2];
    for (std::size_t i = 1; i + 1 < n; ++i) {
        double nx = segNormals[i - 1].x + segNormals[i].x;
        double ny = segNormals[i - 1].y + segNormals[i].y;
        double len = std::hypot(nx, ny);
        if (len > 1e-9) {
            normals[i] = {nx / len, ny / len};
        } else {
            normals[i] = segNormals[i];
        }
    }

    // Build left side (forward pass)
    std::vector<QPointF> leftSide(n);
    for (std::size_t i = 0; i < n; ++i) {
        leftSide[i] = QPointF(points[i].x + normals[i].x * halfW[i], points[i].y + normals[i].y * halfW[i]);
    }

    // Build right side (backward pass)
    std::vector<QPointF> rightSide(n);
    for (std::size_t i = 0; i < n; ++i) {
        rightSide[i] = QPointF(points[i].x - normals[i].x * halfW[i], points[i].y - normals[i].y * halfW[i]);
    }

    // Build closed polygon: left side forward, semicircle at end, right side backward, semicircle at start
    outline.moveTo(leftSide[0]);

    // Left side
    for (std::size_t i = 1; i < n; ++i) {
        outline.lineTo(leftSide[i]);
    }

    // End cap (semicircle)
    {
        const auto& last = points[n - 1];
        const double r = halfW[n - 1];
        const double angle = std::atan2(normals[n - 1].y, normals[n - 1].x);
        const double startAngle = angle * 180.0 / M_PI;
        // arcTo uses a bounding rect
        outline.arcTo(QRectF(last.x - r, last.y - r, 2.0 * r, 2.0 * r), startAngle, -180.0);
    }

    // Right side (backward)
    for (std::size_t i = n - 1; i > 0; --i) {
        outline.lineTo(rightSide[i - 1]);
    }

    // Start cap (semicircle)
    {
        const auto& first = points[0];
        const double r = halfW[0];
        const double angle = std::atan2(-normals[0].y, -normals[0].x);
        const double startAngle = angle * 180.0 / M_PI;
        outline.arcTo(QRectF(first.x - r, first.y - r, 2.0 * r, 2.0 * r), startAngle, -180.0);
    }

    outline.closeSubpath();
    return outline;
}

}  // namespace

namespace vn::view::render {

void QtPreviewStrokeRenderer::draw(const StrokeRenderModel& stroke, RenderContext& context) const {
    if (context.backend() != RenderBackend::QtPainter || stroke.points.size() < 2) {
        return;
    }

    auto* painter = static_cast<QtPainterRenderContext&>(context).native();
    if (!painter) {
        return;
    }

    const auto restoreCompositionMode = painter->compositionMode();
    if (stroke.highlighter) {
        painter->setCompositionMode(QPainter::CompositionMode_Multiply);
    }

    if (stroke.fill != -1 && stroke.points.size() >= 3) {
        auto fillPath = toPainterPath(stroke.points);
        fillPath.closeSubpath();
        painter->fillPath(fillPath, QBrush(toQColor(stroke.color, static_cast<double>(stroke.fill) / 255.0)));
    }

    if (stroke.pressureSensitive) {
        auto outlinePath = buildVariableWidthOutline(stroke.points, stroke.width);
        painter->setPen(Qt::NoPen);
        painter->setBrush(QBrush(toQColor(stroke.color)));
        painter->drawPath(outlinePath);
    } else {
        QPen pen(toQColor(stroke.color, stroke.highlighter ? 0.47 : 1.0), stroke.width, Qt::SolidLine,
                 toPenCapStyle(stroke.capStyle), Qt::RoundJoin);
        if (!stroke.dashPattern.empty()) {
            QList<qreal> pattern;
            pattern.reserve(static_cast<qsizetype>(stroke.dashPattern.size()));
            for (double value: stroke.dashPattern) {
                pattern.push_back(value);
            }
            pen.setDashPattern(pattern);
        }
        painter->setPen(pen);
        painter->setBrush(Qt::NoBrush);
        painter->drawPath(toPainterPath(stroke.points));
    }

    if (stroke.highlighter) {
        painter->setCompositionMode(restoreCompositionMode);
    }
}

}  // namespace vn::view::render
