/*
 * VertexNote
 *
 * Draw object-based geometry elements.
 */

#include "GeometryElementView.h"

#include <array>
#include <cmath>

#include <cairo.h>

#include "util/Color.h"
#include "util/raii/CairoWrappers.h"
#include "vertexnote/geometry/GeometryElement.h"

using namespace vn::view;

namespace {
constexpr std::array<double, 2> ConstructionDash{8.0, 6.0};
constexpr double ConstructionCenterlineInsetRatio = 0.6;

auto drawExtendedLine(cairo_t* cr, vn::geom::Vec2 start, vn::geom::Vec2 end) -> void {
    double clipMinX = 0.0;
    double clipMinY = 0.0;
    double clipMaxX = 0.0;
    double clipMaxY = 0.0;
    cairo_clip_extents(cr, &clipMinX, &clipMinY, &clipMaxX, &clipMaxY);

    const double dx = end.x - start.x;
    const double dy = end.y - start.y;
    const double length = std::hypot(dx, dy);
    if (length == 0.0) {
        cairo_move_to(cr, start.x, start.y);
        cairo_line_to(cr, end.x, end.y);
        cairo_stroke(cr);
        return;
    }

    const double extent = std::hypot(clipMaxX - clipMinX, clipMaxY - clipMinY) + length;
    const double unitX = dx / length;
    const double unitY = dy / length;
    cairo_move_to(cr, start.x - unitX * extent, start.y - unitY * extent);
    cairo_line_to(cr, start.x + unitX * extent, start.y + unitY * extent);
    cairo_stroke(cr);
}

auto drawConstructionCircleHelper(cairo_t* cr, vn::geom::Vec2 center, double radius) -> void {
    const double helperExtent = radius * ConstructionCenterlineInsetRatio;
    cairo_move_to(cr, center.x - helperExtent, center.y);
    cairo_line_to(cr, center.x + helperExtent, center.y);
    cairo_move_to(cr, center.x, center.y - helperExtent);
    cairo_line_to(cr, center.x, center.y + helperExtent);
    cairo_stroke(cr);
}
}

GeometryElementView::GeometryElementView(const vn::geom::GeometryElement* geometry): geometry(geometry) {}

void GeometryElementView::draw(const Context& ctx) const {
    if (!this->geometry) {
        return;
    }

    auto const& object = this->geometry->geometry();
    vn::util::CairoSaveGuard saveGuard(ctx.cr);
    cairo_set_line_join(ctx.cr, CAIRO_LINE_JOIN_ROUND);
    cairo_set_line_cap(ctx.cr, CAIRO_LINE_CAP_ROUND);
    cairo_set_line_width(ctx.cr, this->geometry->getStrokeWidth());
    Util::cairo_set_source_rgbi(ctx.cr, this->geometry->getColor());

    for (const auto& edge: object.edges()) {
        const auto* start = object.vertex(edge.start);
        const auto* end = object.vertex(edge.end);
        if (!start || !end) {
            continue;
        }

        if (edge.kind == vn::geom::EdgeKind::ConstructionLine ||
            edge.kind == vn::geom::EdgeKind::ConstructionCircle) {
            cairo_set_dash(ctx.cr, ConstructionDash.data(), static_cast<int>(ConstructionDash.size()), 0.0);
        } else {
            cairo_set_dash(ctx.cr, nullptr, 0, 0);
        }

        if ((edge.kind == vn::geom::EdgeKind::Arc || edge.kind == vn::geom::EdgeKind::ConstructionCircle) &&
            !edge.controls.empty()) {
            const auto* center = object.vertex(edge.controls.front());
            if (center) {
                const double radius = std::hypot(start->position.x - center->position.x, start->position.y - center->position.y);
                if (edge.start == edge.end) {
                    cairo_arc(ctx.cr, center->position.x, center->position.y, radius, 0.0, 2.0 * M_PI);
                    cairo_stroke(ctx.cr);
                    if (edge.kind == vn::geom::EdgeKind::ConstructionCircle) {
                        drawConstructionCircleHelper(ctx.cr, center->position, radius);
                    }
                    continue;
                }

                const double startAngle =
                        std::atan2(start->position.y - center->position.y, start->position.x - center->position.x);
                double endAngle =
                        std::atan2(end->position.y - center->position.y, end->position.x - center->position.x);
                if (endAngle <= startAngle) {
                    endAngle += 2.0 * M_PI;
                }
                cairo_arc(ctx.cr, center->position.x, center->position.y, radius, startAngle, endAngle);
                cairo_stroke(ctx.cr);
                if (edge.kind == vn::geom::EdgeKind::ConstructionCircle) {
                    drawConstructionCircleHelper(ctx.cr, center->position, radius);
                }
                continue;
            }
        }

        if (edge.kind == vn::geom::EdgeKind::ConstructionLine) {
            drawExtendedLine(ctx.cr, start->position, end->position);
        } else {
            cairo_move_to(ctx.cr, start->position.x, start->position.y);
            cairo_line_to(ctx.cr, end->position.x, end->position.y);
            cairo_stroke(ctx.cr);
        }
    }
}
