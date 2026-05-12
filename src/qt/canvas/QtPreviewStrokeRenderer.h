/*
 * VertexNote
 *
 * Qt preview stroke renderer for the Qt shell.
 */

#pragma once

#include "view/render/Renderers.h"

namespace vn::view::render {

class QtPreviewStrokeRenderer: public StrokeRenderer {
public:
    void draw(const StrokeRenderModel& stroke, RenderContext& context) const override;
};

}  // namespace vn::view::render
