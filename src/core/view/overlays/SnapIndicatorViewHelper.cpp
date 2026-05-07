/*
 * VertexNote
 *
 * Shared drawing helper for snap feedback in click-based geometry tools.
 */

#include "SnapIndicatorViewHelper.h"

#include "util/raii/CairoWrappers.h"

namespace xoj::view {

namespace {

constexpr double SNAP_MARKER_RADIUS = 4.5;
constexpr double FULL_CIRCLE_RADIANS = 6.28318530717958647692;

void setSnapColor(cairo_t* cr, vn::snap::SnapKind kind) {
    switch (kind) {
        case vn::snap::SnapKind::Grid:
            cairo_set_source_rgb(cr, 0.35, 0.35, 0.35);
            break;
        case vn::snap::SnapKind::ExplicitVertex:
        case vn::snap::SnapKind::EdgeEndpoint:
            cairo_set_source_rgb(cr, 0.0, 0.45, 1.0);
            break;
        case vn::snap::SnapKind::Midpoint:
            cairo_set_source_rgb(cr, 0.0, 0.65, 0.35);
            break;
        case vn::snap::SnapKind::EdgeProjection:
            cairo_set_source_rgb(cr, 1.0, 0.55, 0.0);
            break;
        case vn::snap::SnapKind::Intersection:
            cairo_set_source_rgb(cr, 0.85, 0.0, 0.85);
            break;
        case vn::snap::SnapKind::ConstraintGuide:
            cairo_set_source_rgb(cr, 0.0, 0.6, 0.75);
            break;
    }
}

}  // namespace

void drawSnapIndicator(cairo_t* cr, const Point& point, std::optional<vn::snap::SnapKind> kind) {
    if (!kind) {
        return;
    }

    xoj::util::CairoSaveGuard saveGuard(cr);
    cairo_set_line_width(cr, 1.3);
    cairo_set_dash(cr, nullptr, 0, 0);
    setSnapColor(cr, *kind);

    switch (*kind) {
        case vn::snap::SnapKind::ExplicitVertex:
        case vn::snap::SnapKind::EdgeEndpoint:
            cairo_rectangle(cr, point.x - SNAP_MARKER_RADIUS, point.y - SNAP_MARKER_RADIUS,
                            SNAP_MARKER_RADIUS * 2.0, SNAP_MARKER_RADIUS * 2.0);
            cairo_stroke(cr);
            break;
        case vn::snap::SnapKind::Midpoint:
            cairo_move_to(cr, point.x, point.y - SNAP_MARKER_RADIUS);
            cairo_line_to(cr, point.x + SNAP_MARKER_RADIUS, point.y + SNAP_MARKER_RADIUS);
            cairo_line_to(cr, point.x - SNAP_MARKER_RADIUS, point.y + SNAP_MARKER_RADIUS);
            cairo_close_path(cr);
            cairo_stroke(cr);
            break;
        case vn::snap::SnapKind::EdgeProjection:
            cairo_move_to(cr, point.x - SNAP_MARKER_RADIUS, point.y);
            cairo_line_to(cr, point.x + SNAP_MARKER_RADIUS, point.y);
            cairo_move_to(cr, point.x, point.y - SNAP_MARKER_RADIUS);
            cairo_line_to(cr, point.x, point.y + SNAP_MARKER_RADIUS);
            cairo_stroke(cr);
            break;
        case vn::snap::SnapKind::Intersection:
            cairo_move_to(cr, point.x - SNAP_MARKER_RADIUS, point.y - SNAP_MARKER_RADIUS);
            cairo_line_to(cr, point.x + SNAP_MARKER_RADIUS, point.y + SNAP_MARKER_RADIUS);
            cairo_move_to(cr, point.x + SNAP_MARKER_RADIUS, point.y - SNAP_MARKER_RADIUS);
            cairo_line_to(cr, point.x - SNAP_MARKER_RADIUS, point.y + SNAP_MARKER_RADIUS);
            cairo_stroke(cr);
            break;
        case vn::snap::SnapKind::Grid:
        case vn::snap::SnapKind::ConstraintGuide:
            cairo_arc(cr, point.x, point.y, SNAP_MARKER_RADIUS, 0.0, FULL_CIRCLE_RADIANS);
            cairo_stroke(cr);
            break;
    }
}

}  // namespace xoj::view
