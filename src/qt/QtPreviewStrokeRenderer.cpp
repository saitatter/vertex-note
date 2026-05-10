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

    // Resolve per-point widths. The trailing/leading input samples can occasionally carry
    // a zero pressure value even though the segment itself should keep the neighbouring width.
    // Matching the legacy renderer more closely avoids odd pinched or blunt stroke caps in Qt.
    std::vector<double> resolvedWidths(n, fallbackWidth);
    for (std::size_t i = 0; i < n; ++i) {
        if (points[i].z > 0.0) {
            resolvedWidths[i] = points[i].z;
            continue;
        }

        if (i > 0 && points[i - 1].z > 0.0) {
            resolvedWidths[i] = points[i - 1].z;
            continue;
        }

        if (i + 1 < n && points[i + 1].z > 0.0) {
            resolvedWidths[i] = points[i + 1].z;
            continue;
        }
    }

    // Compute per-point half-widths
    std::vector<double> halfW(n);
    for (std::size_t i = 0; i < n; ++i) {
        halfW[i] = resolvedWidths[i] * 0.5;
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

    const auto appendRoundCap = [](QPainterPath& path, const Point& center, double radius, Vec2 startDirection) {
        if (radius <= 0.0) {
            path.lineTo(center.x, center.y);
            return;
        }

        constexpr int CAP_SEGMENTS = 12;
        const double startAngle = std::atan2(startDirection.y, startDirection.x);
        for (int segment = 1; segment <= CAP_SEGMENTS; ++segment) {
            const double t = static_cast<double>(segment) / static_cast<double>(CAP_SEGMENTS);
            const double angle = startAngle - M_PI * t;
            path.lineTo(center.x + std::cos(angle) * radius, center.y + std::sin(angle) * radius);
        }
    };

    // Build closed polygon: left side forward, round cap at end, right side backward, round cap at start.
    // Avoid QPainterPath::arcTo here: if Qt inserts a line to the arc start point, the filled outline can
    // self-intersect and look like the cap was erased instead of filled.
    outline.moveTo(leftSide[0]);

    // Left side
    for (std::size_t i = 1; i < n; ++i) {
        outline.lineTo(leftSide[i]);
    }

    appendRoundCap(outline, points[n - 1], halfW[n - 1], normals[n - 1]);

    // Right side (backward)
    for (std::size_t i = n - 1; i > 0; --i) {
        outline.lineTo(rightSide[i - 1]);
    }

    appendRoundCap(outline, points[0], halfW[0], {-normals[0].x, -normals[0].y});

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
