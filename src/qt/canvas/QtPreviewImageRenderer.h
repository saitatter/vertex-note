/*
 * VertexNote
 *
 * Qt preview image renderer for the Qt shell.
 */

#pragma once

#include "view/render/Renderers.h"

namespace vn::view::render {

class QtPreviewImageRenderer: public ImageRenderer {
public:
    void draw(const ImageRenderModel& image, RenderContext& context) const override;
};

}  // namespace vn::view::render
