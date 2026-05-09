/*
 * VertexNote
 *
 * Shared helpers for building stroke render models from core document data.
 */

#pragma once

#include <vector>

#include "model/PageRef.h"
#include "view/render/Renderers.h"

class Stroke;

namespace vn::view::render {

class StrokeRenderModelFactory {
public:
    [[nodiscard]] static auto fromStroke(const Stroke& stroke) -> StrokeRenderModel;
    [[nodiscard]] static auto fromPage(ConstPageRef page) -> std::vector<StrokeRenderModel>;
};

}  // namespace vn::view::render
