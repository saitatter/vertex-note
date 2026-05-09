/*
 * VertexNote
 *
 * Cairo preview background renderer for GTK-side preview surfaces.
 */

#pragma once

#include "view/render/Renderers.h"

namespace vn::view::render {

class CairoPreviewBackgroundRenderer: public BackgroundRenderer {
public:
    void draw(const PageBackgroundRenderModel& page, const RenderRect& rect, RenderContext& context) const override;
};

}  // namespace vn::view::render
