#include "XmlTexNode.h"

#include <QByteArray>

#include "control/xml/XmlNode.h"  // for XmlNode
#include "util/OutputStream.h"    // for OutputStream
#include "util/StringUtils.h"     // for StaticStringView

XmlTexNode::XmlTexNode(StringUtils::StaticStringView tag, std::string&& binaryData):
        XmlNode(tag), binaryData(binaryData) {}

XmlTexNode::~XmlTexNode() = default;

void XmlTexNode::writeOut(OutputStream* out) {
    out->write("<");
    out->write(tag);
    writeAttributes(out);

    out->write(">");

    const QByteArray bytes =
            QByteArray::fromRawData(this->binaryData.data(), static_cast<qsizetype>(this->binaryData.size()));
    const QByteArray base64 = bytes.toBase64();
    out->write(std::string_view(base64.constData(), static_cast<std::size_t>(base64.size())));

    out->write("</");
    out->write(tag);
    out->write(">\n");
}
