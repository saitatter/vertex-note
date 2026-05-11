/*
 * VertexNote
 *
 * Qt preview text renderer for the Qt shell.
 */

#pragma once

#include "view/render/Renderers.h"

namespace vn::view::render {

class QtPreviewTextRenderer: public TextRenderer {
public:
    void draw(const TextRenderModel& text, RenderContext& context) const override;
};

}  // namespace vn::view::render
