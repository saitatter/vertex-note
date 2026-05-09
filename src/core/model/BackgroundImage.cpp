#include "BackgroundImage.h"

#include <cstddef>
#include <string>   // for string
#include <utility>  // for move

#include <glib-object.h>  // for g_object_unref

#include "util/Stacktrace.h"  // for Stacktrace
#include "util/StringUtils.h"

/*
 * The contents of a background image
 *
 * Internal impl object, dont move this to an external header/source file due this is the best way to reduce code
 * bloat and increase encapsulation. This object is only used in this source scope and is a RAII Container for the
 * GdkPixbuf*
 * No legacy memory leak tests are necessary, because we use smart ptrs to ensure memory correctness
 */

struct BackgroundImage::Content {
    Content(fs::path path, GError** error):
            path(std::move(path)), pixbuf(gdk_pixbuf_new_from_file(char_cast(this->path.u8string().c_str()), error)) {}

    Content(GInputStream* stream, fs::path path, GError** error):
            path(std::move(path)), pixbuf(gdk_pixbuf_new_from_stream(stream, nullptr, error)) {}

    ~Content() {
        if (this->pixbuf) {
            g_object_unref(this->pixbuf);
            this->pixbuf = nullptr;
        }
    };

    Content(const Content&) = delete;
    Content(Content&&) = default;
    auto operator=(const Content&) -> Content& = delete;
    auto operator=(Content&&) -> Content& = default;

    fs::path path;
    GdkPixbuf* pixbuf = nullptr;
    int pageId = -1;
    bool attach = false;
};

void BackgroundImage::free() { this->img.reset(); }

void BackgroundImage::loadFile(fs::path const& path, GError** error) {
    this->img = std::make_shared<Content>(path, error);
}

void BackgroundImage::loadFile(GInputStream* stream, fs::path const& path, GError** error) {
    this->img = std::make_shared<Content>(stream, path, error);
}

auto BackgroundImage::getCloneId() const -> int { return this->img ? this->img->pageId : -1; }

void BackgroundImage::setCloneId(int id) {
    if (this->img) {
        this->img->pageId = id;
    }
}

void BackgroundImage::clearSaveState() { this->setCloneId(-1); }

auto BackgroundImage::getFilepath() const -> fs::path { return this->img ? this->img->path : fs::path{}; }

void BackgroundImage::setFilepath(fs::path path) {
    if (this->img) {
        this->img->path = std::move(path);
    }
}

auto BackgroundImage::isAttached() const -> bool { return this->img ? this->img->attach : false; }

void BackgroundImage::setAttach(bool attach) {
    if (!this->img) {
        g_warning("BackgroundImage::setAttach: please load first an image before call setAttach!");
        Stacktrace::printStacktrace();
        return;
    }
    this->img->attach = attach;
}

auto BackgroundImage::getPixbuf() -> GdkPixbuf* { return this->img ? this->img->pixbuf : nullptr; }
auto BackgroundImage::getPixbuf() const -> const GdkPixbuf* { return this->img ? this->img->pixbuf : nullptr; }

auto BackgroundImage::renderPreviewRaster() const -> xoj::util::RasterImageData {
    const auto* pixbuf = getPixbuf();
    if (!pixbuf) {
        return {};
    }

    const int width = gdk_pixbuf_get_width(pixbuf);
    const int height = gdk_pixbuf_get_height(pixbuf);
    const int rowstride = gdk_pixbuf_get_rowstride(pixbuf);
    const int channels = gdk_pixbuf_get_n_channels(pixbuf);
    const bool hasAlpha = gdk_pixbuf_get_has_alpha(pixbuf);
    const auto* sourcePixels = gdk_pixbuf_read_pixels(pixbuf);
    if (!sourcePixels || width <= 0 || height <= 0 || rowstride <= 0 || channels < 3) {
        return {};
    }

    xoj::util::RasterImageData raster;
    raster.width = width;
    raster.height = height;
    raster.stride = width * 4;
    raster.format = xoj::util::RasterPixelFormat::Rgba8888;
    raster.pixels.resize(static_cast<std::size_t>(raster.stride * raster.height));

    for (int y = 0; y < height; ++y) {
        const auto* sourceRow = sourcePixels + static_cast<std::ptrdiff_t>(y * rowstride);
        auto* targetRow = raster.pixels.data() + static_cast<std::size_t>(y * raster.stride);
        for (int x = 0; x < width; ++x) {
            const auto sourceOffset = static_cast<std::ptrdiff_t>(x * channels);
            const auto targetOffset = static_cast<std::size_t>(x * 4);
            targetRow[targetOffset + 0] = sourceRow[sourceOffset + 0];
            targetRow[targetOffset + 1] = sourceRow[sourceOffset + 1];
            targetRow[targetOffset + 2] = sourceRow[sourceOffset + 2];
            targetRow[targetOffset + 3] = hasAlpha ? sourceRow[sourceOffset + 3] : 255;
        }
    }

    return raster;
}

auto BackgroundImage::isEmpty() const -> bool { return !this->img; }
