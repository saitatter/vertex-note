/*
 * VertexNote
 *
 * Shared raster preview helpers for page backgrounds that are not yet rendered natively.
 */

#pragma once

#include <cstddef>
#include <string>

class BackgroundImage;
class Document;

namespace vn::view::render {

[[nodiscard]] auto encodePdfPagePreviewPng(const Document& document, std::size_t pdfPageNumber, double pageWidth,
                                           double pageHeight) -> std::string;

[[nodiscard]] auto encodeBackgroundImagePreviewPng(const BackgroundImage& image) -> std::string;

}  // namespace vn::view::render
