/*
 * VertexNote
 *
 * Shared raster preview helpers for page backgrounds that are not yet rendered natively.
 */

#include "PageRasterPreviewFactory.h"

#include <algorithm>

#include <gdk-pixbuf/gdk-pixbuf.h>

#include "model/BackgroundImage.h"
#include "model/Document.h"
#include "util/raii/CairoWrappers.h"

namespace {

auto writeSurfaceToPng(cairo_surface_t* surface) -> std::string {
    if (!surface) {
        return {};
    }

    std::string encoded;
    const auto writer = [](void* closure, const unsigned char* data, unsigned int length) -> cairo_status_t {
        auto* target = static_cast<std::string*>(closure);
        target->append(reinterpret_cast<const char*>(data), length);
        return CAIRO_STATUS_SUCCESS;
    };

    if (cairo_surface_write_to_png_stream(surface, writer, &encoded) != CAIRO_STATUS_SUCCESS) {
        return {};
    }

    return encoded;
}

}  // namespace

auto vn::view::render::encodePdfPagePreviewPng(const Document& document, std::size_t pdfPageNumber, double pageWidth,
                                               double pageHeight) -> std::string {
    auto pdfPage = document.getPdfPage(pdfPageNumber);
    if (!pdfPage) {
        return {};
    }

    constexpr int previewWidth = 768;
    const double aspectRatio = pageHeight > 0.0 ? pageWidth / pageHeight : 1.0;
    const int previewHeight = std::max(1, static_cast<int>(previewWidth / std::max(aspectRatio, 0.001)));

    vn::util::CairoSurfaceSPtr surface(cairo_image_surface_create(CAIRO_FORMAT_ARGB32, previewWidth, previewHeight),
                                       vn::util::adopt);
    vn::util::CairoSPtr cr(cairo_create(surface.get()), vn::util::adopt);

    cairo_set_source_rgb(cr.get(), 1.0, 1.0, 1.0);
    cairo_paint(cr.get());
    cairo_scale(cr.get(), previewWidth / std::max(pageWidth, 1.0), previewHeight / std::max(pageHeight, 1.0));
    pdfPage->render(cr.get());

    return writeSurfaceToPng(surface.get());
}

auto vn::view::render::encodeBackgroundImagePreviewPng(const BackgroundImage& image) -> std::string {
    const auto* pixbuf = image.getPixbuf();
    if (!pixbuf) {
        return {};
    }

    gchar* buffer = nullptr;
    gsize size = 0;
    GError* error = nullptr;
    if (!gdk_pixbuf_save_to_buffer(const_cast<GdkPixbuf*>(pixbuf), &buffer, &size, "png", &error, nullptr)) {
        if (error) {
            g_error_free(error);
        }
        return {};
    }

    std::string encoded(buffer, size);
    g_free(buffer);
    return encoded;
}
