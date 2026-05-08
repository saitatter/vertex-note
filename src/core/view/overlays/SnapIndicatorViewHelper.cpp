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
constexpr double SNAP_MARKER_INNER_RADIUS = 1.7;
constexpr double FULL_CIRCLE_RADIANS = 6.28318530717958647692;

auto labelFor(vn::snap::SnapKind kind) -> const char* {
    switch (kind) {
        case vn::snap::SnapKind::Grid:
            return "GRID";
        case vn::snap::SnapKind::ExplicitVertex:
        case vn::snap::SnapKind::EdgeEndpoint:
            return "VERTEX";
        case vn::snap::SnapKind::Midpoint:
            return "MID";
        case vn::snap::SnapKind::EdgeProjection:
            return "PROJ";
        case vn::snap::SnapKind::Intersection:
            return "INT";
        case vn::snap::SnapKind::ConstraintGuide:
            return "CONST";
    }
    return "";
}

void setSnapColor(cairo_t* cr, vn::snap::SnapKind kind, double alpha = 1.0) {
    switch (kind) {
        case vn::snap::SnapKind::Grid:
            cairo_set_source_rgba(cr, 0.35, 0.35, 0.35, alpha);
            break;
        case vn::snap::SnapKind::ExplicitVertex:
        case vn::snap::SnapKind::EdgeEndpoint:
            cairo_set_source_rgba(cr, 0.0, 0.45, 1.0, alpha);
            break;
        case vn::snap::SnapKind::Midpoint:
            cairo_set_source_rgba(cr, 0.0, 0.65, 0.35, alpha);
            break;
        case vn::snap::SnapKind::EdgeProjection:
            cairo_set_source_rgba(cr, 1.0, 0.55, 0.0, alpha);
            break;
        case vn::snap::SnapKind::Intersection:
            cairo_set_source_rgba(cr, 0.85, 0.0, 0.85, alpha);
            break;
        case vn::snap::SnapKind::ConstraintGuide:
            cairo_set_source_rgba(cr, 0.0, 0.6, 0.75, alpha);
            break;
    }
}

void drawClosedSnapShape(cairo_t* cr, vn::snap::SnapKind kind, const Point& point) {
    switch (kind) {
        case vn::snap::SnapKind::ExplicitVertex:
        case vn::snap::SnapKind::EdgeEndpoint:
            cairo_rectangle(cr, point.x - SNAP_MARKER_RADIUS, point.y - SNAP_MARKER_RADIUS,
                            SNAP_MARKER_RADIUS * 2.0, SNAP_MARKER_RADIUS * 2.0);
            break;
        case vn::snap::SnapKind::Midpoint:
            cairo_move_to(cr, point.x, point.y - SNAP_MARKER_RADIUS);
            cairo_line_to(cr, point.x + SNAP_MARKER_RADIUS, point.y + SNAP_MARKER_RADIUS);
            cairo_line_to(cr, point.x - SNAP_MARKER_RADIUS, point.y + SNAP_MARKER_RADIUS);
            cairo_close_path(cr);
            break;
        case vn::snap::SnapKind::ConstraintGuide:
            cairo_move_to(cr, point.x, point.y - SNAP_MARKER_RADIUS);
            cairo_line_to(cr, point.x + SNAP_MARKER_RADIUS, point.y);
            cairo_line_to(cr, point.x, point.y + SNAP_MARKER_RADIUS);
            cairo_line_to(cr, point.x - SNAP_MARKER_RADIUS, point.y);
            cairo_close_path(cr);
            break;
        case vn::snap::SnapKind::Grid:
        case vn::snap::SnapKind::EdgeProjection:
        case vn::snap::SnapKind::Intersection:
            cairo_arc(cr, point.x, point.y, SNAP_MARKER_RADIUS, 0.0, FULL_CIRCLE_RADIANS);
            break;
    }
}

void drawOpenSnapGlyph(cairo_t* cr, vn::snap::SnapKind kind, const Point& point) {
    switch (kind) {
        case vn::snap::SnapKind::Grid:
            cairo_move_to(cr, point.x - SNAP_MARKER_RADIUS, point.y);
            cairo_line_to(cr, point.x + SNAP_MARKER_RADIUS, point.y);
            cairo_move_to(cr, point.x, point.y - SNAP_MARKER_RADIUS);
            cairo_line_to(cr, point.x, point.y + SNAP_MARKER_RADIUS);
            break;
        case vn::snap::SnapKind::EdgeProjection:
            cairo_move_to(cr, point.x - SNAP_MARKER_RADIUS, point.y);
            cairo_line_to(cr, point.x + SNAP_MARKER_RADIUS, point.y);
            cairo_move_to(cr, point.x, point.y - SNAP_MARKER_RADIUS);
            cairo_line_to(cr, point.x, point.y + SNAP_MARKER_RADIUS);
            cairo_move_to(cr, point.x + SNAP_MARKER_RADIUS * 0.45, point.y - SNAP_MARKER_RADIUS);
            cairo_line_to(cr, point.x + SNAP_MARKER_RADIUS * 0.45, point.y - SNAP_MARKER_RADIUS * 0.45);
            cairo_line_to(cr, point.x + SNAP_MARKER_RADIUS, point.y - SNAP_MARKER_RADIUS * 0.45);
            break;
        case vn::snap::SnapKind::Intersection:
            cairo_move_to(cr, point.x - SNAP_MARKER_RADIUS, point.y - SNAP_MARKER_RADIUS);
            cairo_line_to(cr, point.x + SNAP_MARKER_RADIUS, point.y + SNAP_MARKER_RADIUS);
            cairo_move_to(cr, point.x + SNAP_MARKER_RADIUS, point.y - SNAP_MARKER_RADIUS);
            cairo_line_to(cr, point.x - SNAP_MARKER_RADIUS, point.y + SNAP_MARKER_RADIUS);
            break;
        case vn::snap::SnapKind::ExplicitVertex:
        case vn::snap::SnapKind::EdgeEndpoint:
        case vn::snap::SnapKind::Midpoint:
        case vn::snap::SnapKind::ConstraintGuide:
            cairo_new_sub_path(cr);
            cairo_arc(cr, point.x, point.y, SNAP_MARKER_INNER_RADIUS, 0.0, FULL_CIRCLE_RADIANS);
            break;
    }
}

}  // namespace

void drawSnapIndicator(cairo_t* cr, const Point& point, std::optional<vn::snap::SnapKind> kind) {
    if (!kind) {
        return;
    }

    xoj::util::CairoSaveGuard saveGuard(cr);
    cairo_set_dash(cr, nullptr, 0, 0);

    cairo_set_line_width(cr, 3.3);
    drawClosedSnapShape(cr, *kind, point);
    cairo_set_source_rgba(cr, 1.0, 1.0, 1.0, 0.92);
    cairo_stroke_preserve(cr);

    setSnapColor(cr, *kind, 0.16);
    cairo_fill_preserve(cr);

    cairo_set_line_width(cr, 1.35);
    setSnapColor(cr, *kind);
    cairo_stroke(cr);

    cairo_set_line_width(cr, 1.5);
    drawOpenSnapGlyph(cr, *kind, point);
    setSnapColor(cr, *kind);
    cairo_stroke(cr);

    cairo_select_font_face(cr, "Sans", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_BOLD);
    cairo_set_font_size(cr, 9.0);
    cairo_text_extents_t extents{};
    const char* label = labelFor(*kind);
    cairo_text_extents(cr, label, &extents);
    const double paddingX = 4.0;
    const double paddingY = 2.0;
    const double labelX = point.x + SNAP_MARKER_RADIUS + 5.0;
    const double labelY = point.y - SNAP_MARKER_RADIUS - 3.0;
    cairo_rectangle(cr, labelX - paddingX, labelY - extents.height - paddingY, extents.width + paddingX * 2.0,
                    extents.height + paddingY * 2.0);
    cairo_set_source_rgba(cr, 1.0, 1.0, 1.0, 0.86);
    cairo_fill(cr);
    setSnapColor(cr, *kind);
    cairo_move_to(cr, labelX, labelY);
    cairo_show_text(cr, label);
}

}  // namespace xoj::view
