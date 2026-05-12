/*
 * VertexNote
 *
 * Shared raster preview helpers for page backgrounds that are not yet rendered natively.
 */

#pragma once

#include <cstddef>

#include "util/NamespaceAliases.h"
#include "util/RasterImageData.h"

class BackgroundImage;
class Document;

namespace vn::view::render {

[[nodiscard]] auto createPdfPagePreviewRaster(const Document& document, std::size_t pdfPageNumber, double pageWidth,
                                              double pageHeight) -> vn::util::RasterImageData;

[[nodiscard]] auto createBackgroundImagePreviewRaster(const BackgroundImage& image) -> vn::util::RasterImageData;

}  // namespace vn::view::render
