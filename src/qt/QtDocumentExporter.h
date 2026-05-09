/*
 * VertexNote
 *
 * Qt document export (PDF, PNG).
 */

#pragma once

#include <filesystem>
#include <string>
#include <vector>

#include "view/render/Renderers.h"

namespace vn::view::render {
class PageContentRenderer;
}

class QtDocumentExporter {
public:
    explicit QtDocumentExporter(vn::view::render::PageContentRenderer* renderer);

    [[nodiscard]] auto exportPdf(const std::filesystem::path& path,
                                 const std::vector<vn::view::render::PageRenderSnapshot>& pages,
                                 std::string* errorMessage = nullptr) -> bool;

    [[nodiscard]] auto exportPng(const std::filesystem::path& path,
                                 const vn::view::render::PageRenderSnapshot& page, double scale = 2.0,
                                 std::string* errorMessage = nullptr) -> bool;

    [[nodiscard]] auto exportAllPagesPng(const std::filesystem::path& directory,
                                         const std::vector<vn::view::render::PageRenderSnapshot>& pages,
                                         double scale = 2.0, std::string* errorMessage = nullptr) -> bool;

private:
    vn::view::render::PageContentRenderer* contentRenderer = nullptr;
};
