#include "TexImage.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <memory>
#include <utility>  // for move

#include <poppler/cpp/poppler-document.h>
#include <poppler/cpp/poppler-image.h>
#include <poppler/cpp/poppler-page-renderer.h>
#include <poppler/cpp/poppler-page.h>

#include <glib.h>  // for g_warning

#include "model/Element.h"                        // for Element, ELEMENT_TE...
#include "util/Rectangle.h"                       // for Rectangle
#include "util/serializing/ObjectInputStream.h"   // for ObjectInputStream
#include "util/serializing/ObjectOutputStream.h"  // for ObjectOutputStream

using vn::util::Rectangle;

namespace {

auto adoptPopplerDocument(poppler::document* document) -> std::shared_ptr<poppler::document> {
    return std::shared_ptr<poppler::document>(document, [](poppler::document* ptr) { delete ptr; });
}

auto makePopplerDocumentFromBytes(const std::string& bytes) -> std::shared_ptr<poppler::document> {
    if (bytes.size() > static_cast<std::size_t>(std::numeric_limits<int>::max())) {
        return {};
    }

    return adoptPopplerDocument(
            poppler::document::load_from_raw_data(bytes.data(), static_cast<int>(bytes.size())));
}

void setErrorMessage(std::string* errorMessage, std::string message) {
    if (errorMessage) {
        *errorMessage = std::move(message);
    }
}

}  // namespace

TexImage::TexImage(): Element(ELEMENT_TEXIMAGE) { this->sizeCalculated = true; }

TexImage::~TexImage() { freeImageAndPdf(); }

void TexImage::freeImageAndPdf() {
    this->pdf.reset();
}

auto TexImage::cloneTexImage() const -> std::unique_ptr<TexImage> {

    auto img = std::make_unique<TexImage>();
    img->x = this->x;
    img->y = this->y;
    img->setColor(this->getColor());
    img->width = this->width;
    img->height = this->height;
    img->text = this->text;
    img->snappedBounds = this->snappedBounds;
    img->sizeCalculated = this->sizeCalculated;

    img->loadData(std::string(this->binaryData));

    return img;
}

auto TexImage::clone() const -> ElementPtr { return cloneTexImage(); }

void TexImage::setWidth(double width) {
    this->width = width;
    this->calcSize();
}

void TexImage::setHeight(double height) {
    this->height = height;
    this->calcSize();
}

/**
 * Gets the binary data, a .PNG image or a .PDF
 */
auto TexImage::getBinaryData() const -> std::string const& { return this->binaryData; }

void TexImage::setText(std::string text) { this->text = std::move(text); }

auto TexImage::getText() const -> std::string { return this->text; }

auto TexImage::loadData(std::string&& bytes, std::string* errorMessage) -> bool {
    this->freeImageAndPdf();
    this->binaryData = bytes;
    if (this->binaryData.length() < 4) {
        setErrorMessage(errorMessage, "LaTeX image data is too short.");
        return false;
    }

    const std::string type = binaryData.substr(1, 3);
    if (type == "PDF") {
        this->pdf = makePopplerDocumentFromBytes(this->binaryData);
        if (!this->pdf || this->pdf->pages() < 1) {
            setErrorMessage(errorMessage, "Could not load LaTeX PDF image.");
            return false;
        }
        if (std::abs(this->width * this->height) <= std::numeric_limits<double>::epsilon()) {
            std::unique_ptr<poppler::page> page(this->pdf->create_page(0));
            if (!page) {
                setErrorMessage(errorMessage, "Could not read LaTeX PDF page.");
                return false;
            }
            const auto rect = page->page_rect();
            this->width = rect.width();
            this->height = rect.height();
        }
    } else if (type != "PNG") {
        setErrorMessage(errorMessage, "Unknown LaTeX image type: " + type);
        g_warning("Unknown Latex image type: \"%s\"", type.c_str());
    }

    return true;
}

auto TexImage::getPdf() const -> const poppler::document* { return this->pdf.get(); }

auto TexImage::renderPreviewRaster() const -> xoj::util::RasterImageData {
    if (!this->pdf) {
        return {};
    }

    const int pixelWidth = std::max(1, static_cast<int>(std::lround(std::max(1.0, this->width))));
    const int pixelHeight = std::max(1, static_cast<int>(std::lround(std::max(1.0, this->height))));

    std::unique_ptr<poppler::page> page(this->pdf->create_page(0));
    if (!page) {
        return {};
    }

    poppler::page_renderer renderer;
    renderer.set_render_hints(poppler::page_renderer::antialiasing | poppler::page_renderer::text_antialiasing);
    renderer.set_image_format(poppler::image::format_argb32);
    renderer.set_paper_color(0xffffffff);

    const double xres = static_cast<double>(pixelWidth) / std::max(this->width, 1.0) * 72.0;
    const double yres = static_cast<double>(pixelHeight) / std::max(this->height, 1.0) * 72.0;
    const poppler::image image = renderer.render_page(page.get(), xres, yres);
    if (!image.is_valid() || image.format() != poppler::image::format_argb32 || !image.const_data() ||
        image.bytes_per_row() <= 0 || image.width() <= 0 || image.height() <= 0) {
        return {};
    }

    xoj::util::RasterImageData raster;
    raster.width = image.width();
    raster.height = image.height();
    raster.stride = image.bytes_per_row();
    raster.format = xoj::util::RasterPixelFormat::Argb32Premultiplied;
    const auto* data = reinterpret_cast<const unsigned char*>(image.const_data());
    raster.pixels.assign(data, data + static_cast<std::size_t>(raster.stride * raster.height));
    return raster;
}

void TexImage::scale(double x0, double y0, double fx, double fy, double rotation,
                     bool) {  // line width scaling option is not used

    this->x = (this->x - x0) * fx + x0;
    this->y = (this->y - y0) * fy + y0;

    this->width *= fx;
    this->height *= fy;
    this->calcSize();
}

void TexImage::rotate(double x0, double y0, double th) {
    // Rotation for TexImages not yet implemented
}

void TexImage::serialize(ObjectOutputStream& out) const {
    out.writeObject("TexImage");

    this->Element::serialize(out);

    out.writeDouble(this->width);
    out.writeDouble(this->height);
    out.writeString(this->text);

    out.writeString(this->binaryData);

    out.endObject();
}

void TexImage::readSerialized(ObjectInputStream& in) {
    in.readObject("TexImage");

    this->Element::readSerialized(in);

    this->width = in.readDouble();
    this->height = in.readDouble();
    this->text = in.readString();

    freeImageAndPdf();

    std::string data = in.readString();
    this->loadData(std::move(data));

    in.endObject();
    this->calcSize();
}

void TexImage::calcSize() const {
    this->snappedBounds = Rectangle<double>(this->x, this->y, this->width, this->height);
    this->sizeCalculated = true;
}
