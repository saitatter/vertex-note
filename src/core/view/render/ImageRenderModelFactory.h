/*
 * VertexNote
 *
 * Shared helpers for building image render models from core document data.
 */

#pragma once

#include "view/render/Renderers.h"

class Image;
class TexImage;

namespace vn::view::render {

class ImageRenderModelFactory {
public:
    [[nodiscard]] static auto fromImage(const Image& image) -> ImageRenderModel;
    [[nodiscard]] static auto fromTexImage(const TexImage& image) -> ImageRenderModel;
};

}  // namespace vn::view::render
