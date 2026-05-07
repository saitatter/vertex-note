/*
 * VertexNote
 *
 * Draw object-based geometry elements.
 */

#include "GeometryElementView.h"

#include <cairo.h>

#include "util/Color.h"
#include "util/raii/CairoWrappers.h"
#include "vertexnote/geometry/GeometryElement.h"

using namespace xoj::view;

GeometryElementView::GeometryElementView(const vn::geom::GeometryElement* geometry): geometry(geometry) {}

void GeometryElementView::draw(const Context& ctx) const {
    if (!this->geometry) {
        return;
    }

    auto const& object = this->geometry->geometry();
    xoj::util::CairoSaveGuard saveGuard(ctx.cr);
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

        cairo_move_to(ctx.cr, start->position.x, start->position.y);
        cairo_line_to(ctx.cr, end->position.x, end->position.y);
        cairo_stroke(ctx.cr);
    }
}
