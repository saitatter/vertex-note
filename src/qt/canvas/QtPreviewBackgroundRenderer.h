/*
 * VertexNote
 *
 * Qt preview background renderer for the Qt shell.
 */

#pragma once

#include "view/render/Renderers.h"

namespace vn::view::render {

class QtPreviewBackgroundRenderer: public BackgroundRenderer {
public:
    void draw(const PageBackgroundRenderModel& page, const RenderRect& rect, RenderContext& context) const override;
    void setPageShadowEnabled(bool enabled);

private:
    bool pageShadowEnabled = true;
};

}  // namespace vn::view::render
