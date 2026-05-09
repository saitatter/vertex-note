/*
 * VertexNote
 *
 * Shared helpers for building image render models from core document data.
 */

#pragma once

#include "view/render/Renderers.h"

class Image;

namespace vn::view::render {

class ImageRenderModelFactory {
public:
    [[nodiscard]] static auto fromImage(const Image& image) -> ImageRenderModel;
};

}  // namespace vn::view::render
