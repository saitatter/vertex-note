#include "Image.h"

#include <algorithm>
#include <cmath>      // for sqrt
#include <memory>
#include <utility>  // for move, pair

#include <QBuffer>
#include <QByteArray>
#include <QImage>
#include <QImageReader>
#include <QIODevice>
#include <QSize>
#include <QString>

#include "model/Element.h"   // for Element, ELEMENT_IMAGE
#include "util/Assert.h"     // for xoj_assert
#include "util/Rectangle.h"  // for Rectangle
#include "util/i18n.h"
#include "util/safe_casts.h"
#include "util/serializing/ObjectInputStream.h"   // for ObjectInputStream
#include "util/serializing/ObjectOutputStream.h"  // for ObjectOutputStream

using vn::util::Rectangle;

Image::Image(): Element(ELEMENT_IMAGE) {}

auto Image::clone() const -> ElementPtr {
    auto img = std::make_unique<Image>();

    img->x = this->x;
    img->y = this->y;
    img->setColor(this->getColor());
    img->width = this->width;
    img->height = this->height;
    img->data = this->data;

    img->imageSize = this->imageSize;
    img->imageFormatName = this->imageFormatName;
    img->imageMetadataLoaded = this->imageMetadataLoaded;
    img->snappedBounds = this->snappedBounds;
    img->sizeCalculated = this->sizeCalculated;

    return img;
}

void Image::setWidth(double width) {
    this->width = width;
    this->calcSize();
}

void Image::setHeight(double height) {
    this->height = height;
    this->calcSize();
}

void Image::setImage(std::string_view data) { setImage(std::string(data)); }

void Image::setImage(std::string&& data) {
    this->data = std::move(data);
    this->imageSize = NOSIZE;
    this->imageFormatName.clear();
    this->imageMetadataLoaded = false;
}

auto Image::renderBuffer() const -> std::optional<std::string> {
    xoj_assert_message(data.length() > 0, "image has no data, cannot render it!");
    if (this->imageMetadataLoaded) {
        return std::nullopt;
    }

    QByteArray bytes = QByteArray::fromRawData(this->data.data(), static_cast<qsizetype>(this->data.size()));
    QBuffer buffer(&bytes);
    buffer.open(QIODevice::ReadOnly);

    QImageReader reader(&buffer);
    reader.setAutoTransform(true);

    if (!reader.canRead()) {
        return std::string(_("Failed to load image")) + "\n" + _("Error: ") + reader.errorString().toStdString();
    }

    if (const auto format = reader.format(); !format.isEmpty()) {
        this->imageFormatName = QString::fromLatin1(format).toStdString();
    }

    static constexpr uint64_t MAX_SIZE = 1ULL << 25;  // 32M pixels, enough for A4 at 72pp.
    const QSize originalSize = reader.size();
    if (originalSize.width() > 0 && originalSize.height() > 0 &&
        static_cast<uint64_t>(originalSize.width()) * static_cast<uint64_t>(originalSize.height()) > MAX_SIZE) {
        const double ratio = static_cast<double>(originalSize.width()) / static_cast<double>(originalSize.height());
        const int maxHeight = floor_cast<int>(std::sqrt(MAX_SIZE / ratio));
        const int maxWidth = floor_cast<int>(maxHeight * ratio);
        reader.setScaledSize(QSize(std::max(1, maxWidth), std::max(1, maxHeight)));
    }

    const QImage decoded = reader.read();
    if (decoded.isNull()) {
        return std::string(_("Failed to load image")) + "\n" + _("Error: ") + reader.errorString().toStdString();
    }

    this->imageSize = {decoded.width(), decoded.height()};
    this->imageMetadataLoaded = true;
    return std::nullopt;
}

void Image::scale(double x0, double y0, double fx, double fy, double rotation,
                  bool) {  // line width scaling option is not used
    this->x -= x0;
    this->x *= fx;
    this->x += x0;
    this->y -= y0;
    this->y *= fy;
    this->y += y0;

    this->width *= fx;
    this->height *= fy;
    this->calcSize();
}

void Image::rotate(double x0, double y0, double th) {}

void Image::serialize(ObjectOutputStream& out) const {
    out.writeObject("Image");

    this->Element::serialize(out);

    out.writeDouble(this->width);
    out.writeDouble(this->height);

    out.writeImage(this->data);

    out.endObject();
}

void Image::readSerialized(ObjectInputStream& in) {
    in.readObject("Image");

    this->Element::readSerialized(in);

    this->width = in.readDouble();
    this->height = in.readDouble();

    this->data = in.readImage();
    this->imageSize = NOSIZE;
    this->imageFormatName.clear();
    this->imageMetadataLoaded = false;

    in.endObject();
    this->calcSize();
}

void Image::calcSize() const {
    this->snappedBounds = Rectangle<double>(this->x, this->y, this->width, this->height);
    this->sizeCalculated = true;
}

bool Image::hasData() const { return !this->data.empty(); }

const unsigned char* Image::getRawData() const { return reinterpret_cast<const unsigned char*>(this->data.data()); }

size_t Image::getRawDataLength() const { return this->data.size(); }

std::pair<int, int> Image::getImageSize() const { return this->imageSize; }

const std::string& Image::getImageFormatName() const {
    if (!this->imageMetadataLoaded && hasData()) {
        (void) renderBuffer();
    }
    return this->imageFormatName;
}
