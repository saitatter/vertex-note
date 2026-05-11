#include "XmlImageNode.h"

#include <QBuffer>
#include <QByteArray>
#include <QImage>
#include <QImageReader>
#include <QIODevice>

#include <glib.h>  // for g_base64_encode, g_free, gchar, g_e...

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
        g_error("XmlImageNode::writeOut(); image data is empty");
    } else {
        gchar* base64_str = g_base64_encode(reinterpret_cast<const guchar*>(this->pngData.data()), this->pngData.size());
        out->write(base64_str);
        g_free(base64_str);
    }

    out->write("</");
    out->write(tag);
    out->write(">\n");
}
