/*
 * VertexNote
 *
 * Shared helpers for building geometry render models from core document data.
 */

#pragma once

#include "view/render/Renderers.h"

namespace vn::geom {
class GeometryElement;
}

namespace vn::view::render {

class GeometryRenderModelFactory {
public:
    [[nodiscard]] static auto fromGeometryElement(const vn::geom::GeometryElement& geometry) -> GeometryRenderModel;
};

}  // namespace vn::view::render
