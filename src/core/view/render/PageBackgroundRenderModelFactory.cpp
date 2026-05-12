/*
 * VertexNote
 *
 * Shared helpers for building background render models from core page types.
 */

#include "PageBackgroundRenderModelFactory.h"

#include "model/NotePage.h"

namespace vn::view::render {

auto PageBackgroundRenderModelFactory::fromPage(ConstPageRef page) -> PageBackgroundRenderModel {
    if (!page) {
        return {};
    }

    const auto pageType = page->getBackgroundType();
    return {.backgroundFormat = pageType.format,
            .backgroundColor = page->getBackgroundColor(),
            .pageWidth = page->getWidth(),
            .pageHeight = page->getHeight(),
            .annotated = page->isAnnotated(),
            .hasBackgroundName = page->backgroundHasName(),
            .backgroundName = page->backgroundHasName() ? page->getBackgroundName() : std::string{},
            .layerCount = static_cast<std::size_t>(page->getLayerCount()),
            .pdfPageNumber = page->getPdfPageNr()};
}

auto PageBackgroundRenderModelFactory::fromPageType(const PageType& pageType) -> PageBackgroundRenderModel {
    return {.backgroundFormat = pageType.format,
            .backgroundColor = Colors::white,
            .pageWidth = 0.0,
            .pageHeight = 0.0,
            .annotated = false,
            .hasBackgroundName = false,
            .backgroundName = {},
            .layerCount = 0U,
            .pdfPageNumber = 0U};
}

}  // namespace vn::view::render
