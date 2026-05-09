/*
 * VertexNote
 *
 * Qt preview background renderer for the experimental shell.
 */

#pragma once

#include "view/render/Renderers.h"

namespace vn::view::render {

class QtPreviewBackgroundRenderer: public BackgroundRenderer {
public:
    void draw(const PageBackgroundRenderModel& page, const RenderRect& rect, RenderContext& context) const override;
};

}  // namespace vn::view::render
