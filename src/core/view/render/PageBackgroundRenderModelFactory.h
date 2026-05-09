/*
 * VertexNote
 *
 * Shared helpers for building background render models from core page types.
 */

#pragma once

#include "model/PageRef.h"
#include "model/PageType.h"
#include "view/render/Renderers.h"

namespace vn::view::render {

class PageBackgroundRenderModelFactory {
public:
    [[nodiscard]] static auto fromPage(ConstPageRef page) -> PageBackgroundRenderModel;
    [[nodiscard]] static auto fromPageType(const PageType& pageType) -> PageBackgroundRenderModel;
};

}  // namespace vn::view::render
