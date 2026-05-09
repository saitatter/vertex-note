/*
 * VertexNote
 *
 * Qt preview geometry renderer for the experimental shell.
 */

#pragma once

#include "view/render/Renderers.h"

namespace vn::view::render {

class QtPreviewGeometryRenderer: public GeometryRenderer {
public:
    void draw(const GeometryRenderModel& geometry, RenderContext& context) const override;
};

}  // namespace vn::view::render
