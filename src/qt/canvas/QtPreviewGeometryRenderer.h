/*
 * VertexNote
 *
 * Qt preview geometry renderer for the Qt shell.
 */

#pragma once

#include "view/render/Renderers.h"

namespace vn::view::render {

class QtPreviewGeometryRenderer: public GeometryRenderer {
public:
    void draw(const GeometryRenderModel& geometry, RenderContext& context) const override;
    void setWireframeViewEnabled(bool enabled);
    void setFaceFillVisible(bool visible);

private:
    bool wireframeViewEnabled = false;
    bool faceFillVisible = true;
};

}  // namespace vn::view::render
