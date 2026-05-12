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

struct PageRenderSnapshotOptions {
    bool renderPdfBackgrounds = true;
};

[[nodiscard]] auto buildPageRenderSnapshots(Document& document, PageRenderSnapshotOptions options = {})
        -> std::vector<PageRenderSnapshot>;

}  // namespace vn::view::render
