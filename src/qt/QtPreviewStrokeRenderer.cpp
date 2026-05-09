/*
 * VertexNote
 *
 * Qt preview stroke renderer for the experimental shell.
 */

#include "QtPreviewStrokeRenderer.h"

#include <algorithm>

#include <QBrush>
#include <QColor>
#include <QPainter>
#include <QPainterPath>
#include <QPainterPathStroker>
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

    path.moveTo(points.front().x, points.front().y);
    for (std::size_t i = 1; i < points.size(); ++i) {
        path.lineTo(points[i].x, points[i].y);
    }
    return path;
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
        for (std::size_t i = 0; i + 1 < stroke.points.size(); ++i) {
            const auto& lhs = stroke.points[i];
            const auto& rhs = stroke.points[i + 1];
            const double width = lhs.z > 0.0 ? lhs.z : stroke.width;
            QPen pen(toQColor(stroke.color), width, Qt::SolidLine, toPenCapStyle(stroke.capStyle), Qt::RoundJoin);
            painter->setPen(pen);
            painter->drawLine(QPointF(lhs.x, lhs.y), QPointF(rhs.x, rhs.y));
        }
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
