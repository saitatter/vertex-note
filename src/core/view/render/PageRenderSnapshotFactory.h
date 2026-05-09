/*
 * VertexNote
 *
 * Shared page snapshot builder for interactive render backends.
 */

#pragma once

#include <vector>

#include "Renderers.h"

class Document;

namespace vn::view::render {

[[nodiscard]] auto buildPageRenderSnapshots(Document& document) -> std::vector<PageRenderSnapshot>;

}  // namespace vn::view::render
