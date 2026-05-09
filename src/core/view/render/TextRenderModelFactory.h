/*
 * VertexNote
 *
 * Shared helpers for building text render models from core document data.
 */

#pragma once

#include <vector>

#include "model/PageRef.h"
#include "view/render/Renderers.h"

class Text;

namespace vn::view::render {

class TextRenderModelFactory {
public:
    [[nodiscard]] static auto fromText(const Text& text) -> TextRenderModel;
    [[nodiscard]] static auto fromPage(ConstPageRef page) -> std::vector<TextRenderModel>;
};

}  // namespace vn::view::render
