/*
 * VertexNote
 *
 * Draw object-based geometry elements.
 */

#include "GeometryElementView.h"

#include <cmath>

#include <cairo.h>

#include "util/Color.h"
#include "util/raii/CairoWrappers.h"
#include "vertexnote/geometry/GeometryElement.h"

using namespace vn::view;

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

        if (edge.kind == vn::geom::EdgeKind::Arc && !edge.controls.empty()) {
            const auto* center = object.vertex(edge.controls.front());
            if (center) {
                const double radius = std::hypot(start->position.x - center->position.x, start->position.y - center->position.y);
                if (edge.start == edge.end) {
                    cairo_arc(ctx.cr, center->position.x, center->position.y, radius, 0.0, 2.0 * M_PI);
                    cairo_stroke(ctx.cr);
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
                continue;
            }
        }

        cairo_move_to(ctx.cr, start->position.x, start->position.y);
        cairo_line_to(ctx.cr, end->position.x, end->position.y);
        cairo_stroke(ctx.cr);
    }
}
