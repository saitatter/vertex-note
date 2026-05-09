/*
 * VertexNote
 *
 * Shared raster preview helpers for page backgrounds that are not yet rendered natively.
 */

#include "PageRasterPreviewFactory.h"

#include <algorithm>

#include "model/BackgroundImage.h"
#include "model/Document.h"
auto vn::view::render::createPdfPagePreviewRaster(const Document& document, std::size_t pdfPageNumber, double pageWidth,
                                                  double pageHeight) -> xoj::util::RasterImageData {
    auto pdfPage = document.getPdfPage(pdfPageNumber);
    if (!pdfPage) {
        return {};
    }

    constexpr int previewWidth = 768;
    const double aspectRatio = pageHeight > 0.0 ? pageWidth / pageHeight : 1.0;
    const int previewHeight = std::max(1, static_cast<int>(previewWidth / std::max(aspectRatio, 0.001)));
    return pdfPage->renderPreviewRaster(previewWidth, previewHeight, pageWidth, pageHeight);
}

auto vn::view::render::createBackgroundImagePreviewRaster(const BackgroundImage& image) -> xoj::util::RasterImageData {
    return image.renderPreviewRaster();
}
