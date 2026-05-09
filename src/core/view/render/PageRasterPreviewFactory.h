/*
 * VertexNote
 *
 * Shared raster preview helpers for page backgrounds that are not yet rendered natively.
 */

#pragma once

#include <cstddef>

#include "util/RasterImageData.h"

class BackgroundImage;
class Document;

namespace vn::view::render {

[[nodiscard]] auto createPdfPagePreviewRaster(const Document& document, std::size_t pdfPageNumber, double pageWidth,
                                              double pageHeight) -> xoj::util::RasterImageData;

[[nodiscard]] auto createBackgroundImagePreviewRaster(const BackgroundImage& image) -> xoj::util::RasterImageData;

}  // namespace vn::view::render
