/*
 * VertexNote
 *
 * Shared drawing helper for snap feedback in click-based geometry tools.
 */

#pragma once

#include <optional>

#include <cairo.h>

#include "model/Point.h"
#include "vertexnote/snapping/SnapTypes.h"

namespace xoj::view {

void drawSnapIndicator(cairo_t* cr, const Point& point, std::optional<vn::snap::SnapKind> kind);

}  // namespace xoj::view
