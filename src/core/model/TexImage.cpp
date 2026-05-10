#include "TexImage.h"

#include <memory>
#include <algorithm>
#include <cmath>
#include <utility>  // for move

#include <poppler-document.h>  // for poppler_document_ge...
#include <poppler-page.h>      // for poppler_page_get_size

#include "model/Element.h"                        // for Element, ELEMENT_TE...
#include "util/Rectangle.h"                       // for Rectangle
#include "util/raii/CairoWrappers.h"
#include "util/raii/GObjectSPtr.h"                // for GObjectSPtr
#include "util/serializing/ObjectInputStream.h"   // for ObjectInputStream
#include "util/serializing/ObjectOutputStream.h"  // for ObjectOutputStream

using vn::util::Rectangle;

TexImage::TexImage(): Element(ELEMENT_TEXIMAGE) { this->sizeCalculated = true; }

TexImage::~TexImage() { freeImageAndPdf(); }

void TexImage::freeImageAndPdf() {
    if (this->image) {
        cairo_surface_destroy(this->image);
        this->image = nullptr;
    }

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

    // Clone has a copy of our PDF.
    img->pdf = this->pdf;

    // Load a copy of our data (must be called after
    // giving the clone a copy of our PDF -- it may change
    // the PDF we've given it).
    img->loadData(std::string(this->binaryData), nullptr);

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

auto TexImage::cairoReadFunction(TexImage* image, unsigned char* data, unsigned int length) -> cairo_status_t {
    for (unsigned int i = 0; i < length; i++, image->read++) {
        if (image->read >= image->binaryData.length()) {
            return CAIRO_STATUS_READ_ERROR;
        }
        data[i] = static_cast<unsigned char>(image->binaryData[image->read]);
    }

    return CAIRO_STATUS_SUCCESS;
}

/**
 * Gets the binary data, a .PNG image or a .PDF
 */
auto TexImage::getBinaryData() const -> std::string const& { return this->binaryData; }

void TexImage::setText(std::string text) { this->text = std::move(text); }

auto TexImage::getText() const -> std::string { return this->text; }

auto TexImage::loadData(std::string&& bytes, GError** err) -> bool {
    this->freeImageAndPdf();
    this->binaryData = bytes;
    if (this->binaryData.length() < 4) {
        return false;
    }

    const std::string type = binaryData.substr(1, 3);
    if (type == "PDF") {
        // Note: binaryData must not be modified while pdf is live.
        auto* bytes = g_bytes_new_with_free_func(this->binaryData.data(), this->binaryData.size(), nullptr, nullptr);
        this->pdf.reset(poppler_document_new_from_bytes(bytes, nullptr, err), vn::util::adopt);
        g_bytes_unref(bytes);

        if (!pdf.get() || poppler_document_get_n_pages(this->pdf.get()) < 1) {
            return false;
        }
        if (std::abs(this->width * this->height) <= std::numeric_limits<double>::epsilon()) {
            vn::util::GObjectSPtr<PopplerPage> page(poppler_document_get_page(this->pdf.get(), 0), vn::util::adopt);
            poppler_page_get_size(page.get(), &this->width, &this->height);
        }
    } else if (type == "PNG") {
        this->image = cairo_image_surface_create_from_png_stream(
                reinterpret_cast<cairo_read_func_t>(&cairoReadFunction), this);
    } else {
        g_warning("Unknown Latex image type: \"%s\"", type.c_str());
    }

    return true;
}

auto TexImage::getImage() const -> cairo_surface_t* { return this->image; }

auto TexImage::getPdf() const -> PopplerDocument* { return this->pdf.get(); }

auto TexImage::renderPreviewRaster() const -> xoj::util::RasterImageData {
    if (!this->pdf) {
        return {};
    }

    vn::util::GObjectSPtr<PopplerPage> page(poppler_document_get_page(this->pdf.get(), 0), vn::util::adopt);
    if (!page) {
        return {};
    }

    const int pixelWidth = std::max(1, static_cast<int>(std::lround(std::max(1.0, this->width))));
    const int pixelHeight = std::max(1, static_cast<int>(std::lround(std::max(1.0, this->height))));

    vn::util::CairoSurfaceSPtr surface(cairo_image_surface_create(CAIRO_FORMAT_ARGB32, pixelWidth, pixelHeight),
                                       vn::util::adopt);
    vn::util::CairoSPtr cr(cairo_create(surface.get()), vn::util::adopt);

    cairo_set_source_rgb(cr.get(), 1.0, 1.0, 1.0);
    cairo_paint(cr.get());
    cairo_scale(cr.get(), pixelWidth / std::max(this->width, 1.0), pixelHeight / std::max(this->height, 1.0));
    poppler_page_render(page.get(), cr.get());
    cairo_surface_flush(surface.get());

    auto* data = cairo_image_surface_get_data(surface.get());
    const int stride = cairo_image_surface_get_stride(surface.get());
    if (!data || stride <= 0) {
        return {};
    }

    xoj::util::RasterImageData raster;
    raster.width = pixelWidth;
    raster.height = pixelHeight;
    raster.stride = stride;
    raster.format = xoj::util::RasterPixelFormat::Argb32Premultiplied;
    raster.pixels.assign(data, data + static_cast<std::size_t>(stride * pixelHeight));
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
    this->loadData(std::move(data), nullptr);

    in.endObject();
    this->calcSize();
}

void TexImage::calcSize() const {
    this->snappedBounds = Rectangle<double>(this->x, this->y, this->width, this->height);
    this->sizeCalculated = true;
}
