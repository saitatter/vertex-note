#include "BackgroundImage.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <string>   // for string
#include <utility>  // for move

#include <QByteArray>
#include <QBuffer>
#include <QImage>
#include <QImageReader>
#include <QIODevice>
#include <QString>

#include "util/PathUtil.h"
#include "util/Stacktrace.h"  // for Stacktrace
#include "util/StringUtils.h"

/*
 * The contents of a background image
 *
 * Internal impl object, dont move this to an external header/source file due this is the best way to reduce code
 * bloat and increase encapsulation. This object is only used in this source scope and is a RAII Container for the
 * QImage.
 * No legacy memory leak tests are necessary, because we use smart ptrs to ensure memory correctness
 */

namespace {

auto backgroundImageErrorQuark() -> GQuark {
    return g_quark_from_static_string("vertex-note-background-image");
}

void setLoadError(GError** error, const QString& message) {
    if (error) {
        g_set_error_literal(error, backgroundImageErrorQuark(), 1, message.toUtf8().constData());
    }
}

auto loadImageFromBytes(QByteArray bytes, GError** error) -> QImage {
    QImageReader reader;
    QBuffer buffer(&bytes);
    buffer.open(QIODevice::ReadOnly);
    reader.setDevice(&buffer);
    reader.setAutoTransform(true);
    QImage image = reader.read();
    if (image.isNull()) {
        setLoadError(error, reader.errorString());
    }
    return image;
}

}  // namespace

struct BackgroundImage::Content {
    Content(fs::path path, GError** error):
            path(std::move(path)) {
        QImageReader reader(QString::fromUtf8(Util::toGFilename(this->path).c_str()));
        reader.setAutoTransform(true);
        this->image = reader.read();
        if (this->image.isNull()) {
            setLoadError(error, reader.errorString());
        }
    }

    Content(GInputStream* stream, fs::path path, GError** error): path(std::move(path)) {
        QByteArray bytes;
        std::array<char, 8192> chunk{};
        while (true) {
            const auto read = g_input_stream_read(stream, chunk.data(), chunk.size(), nullptr, error);
            if (read < 0) {
                return;
            }
            if (read == 0) {
                break;
            }
            bytes.append(chunk.data(), static_cast<qsizetype>(read));
        }
        this->image = loadImageFromBytes(bytes, error);
    }

    ~Content() = default;

    Content(const Content&) = delete;
    Content(Content&&) = default;
    auto operator=(const Content&) -> Content& = delete;
    auto operator=(Content&&) -> Content& = default;

    fs::path path;
    QImage image;
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

auto saveBackgroundImagePng(const BackgroundImage& image, const fs::path& path) -> bool {
    if (!image.img || image.img->image.isNull()) {
        return false;
    }
    return image.img->image.save(QString::fromUtf8(Util::toGFilename(path).c_str()), "PNG");
}

auto BackgroundImage::renderPreviewRaster() const -> xoj::util::RasterImageData {
    if (!this->img || this->img->image.isNull()) {
        return {};
    }

    const QImage rgba = this->img->image.convertToFormat(QImage::Format_RGBA8888);
    const int width = rgba.width();
    const int height = rgba.height();
    const int rowstride = rgba.bytesPerLine();
    const auto* sourcePixels = rgba.constBits();
    if (!sourcePixels || width <= 0 || height <= 0 || rowstride <= 0) {
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
        std::copy_n(sourceRow, static_cast<std::size_t>(raster.stride), targetRow);
    }

    return raster;
}

auto BackgroundImage::hasLoadedImage() const -> bool { return this->img && !this->img->image.isNull(); }

auto BackgroundImage::isEmpty() const -> bool { return !this->img; }
