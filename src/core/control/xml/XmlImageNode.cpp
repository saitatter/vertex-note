#include "XmlImageNode.h"

#include <QBuffer>
#include <QByteArray>
#include <QImage>
#include <QImageReader>
#include <QIODevice>

#include <stdexcept>  // for runtime_error

#include "control/xml/XmlNode.h"  // for XmlNode
#include "util/OutputStream.h"    // for OutputStream
#include "util/StringUtils.h"     // for StaticStringView

XmlImageNode::XmlImageNode(StringUtils::StaticStringView tag): XmlNode(tag) {}

XmlImageNode::~XmlImageNode() = default;

void XmlImageNode::setImage(std::string_view encodedImage) {
    this->pngData.clear();

    QByteArray source = QByteArray::fromRawData(encodedImage.data(), static_cast<qsizetype>(encodedImage.size()));
    QBuffer sourceBuffer(&source);
    sourceBuffer.open(QIODevice::ReadOnly);
    QImageReader reader(&sourceBuffer);
    reader.setAutoTransform(true);
    const QImage image = reader.read();
    if (image.isNull()) {
        return;
    }

    QByteArray pngBytes;
    QBuffer pngBuffer(&pngBytes);
    pngBuffer.open(QIODevice::WriteOnly);
    if (image.save(&pngBuffer, "PNG")) {
        this->pngData.assign(pngBytes.constData(), static_cast<std::size_t>(pngBytes.size()));
    }
}

void XmlImageNode::writeOut(OutputStream* out) {
    out->write("<");
    out->write(tag);
    writeAttributes(out);

    out->write(">");

    if (this->pngData.empty()) {
        throw std::runtime_error("XmlImageNode::writeOut(); image data is empty");
    } else {
        const QByteArray bytes = QByteArray::fromRawData(this->pngData.data(), static_cast<qsizetype>(this->pngData.size()));
        const QByteArray base64 = bytes.toBase64();
        out->write(std::string_view(base64.constData(), static_cast<std::size_t>(base64.size())));
    }

    out->write("</");
    out->write(tag);
    out->write(">\n");
}
