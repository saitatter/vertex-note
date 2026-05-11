/*
 * VertexNote
 *
 * Pure page layout helpers for the Qt canvas.
 */

#pragma once

#include <vector>

#include <QRectF>

#include "view/render/Renderers.h"

struct QtCanvasLayoutOptions {
    int span = 1;
    int pairOffset = 0;
    bool vertical = true;
    bool rightToLeft = false;
    bool bottomToTop = false;
};

[[nodiscard]] auto layoutQtCanvasPages(const std::vector<vn::view::render::PageRenderSnapshot>& pages,
                                       QtCanvasLayoutOptions options) -> std::vector<QRectF>;
