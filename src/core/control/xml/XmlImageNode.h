/*
 * VertexNote
 *
 * XML Writer helper class
 *
 * @author VertexNote Team
 * https://github.com/saitatter/vertex-note
 *
 * @license GNU GPLv2 or later
 */

#pragma once

#include <string>
#include <string_view>

#include "XmlNode.h"  // for XmlNode

class OutputStream;

class XmlImageNode: public XmlNode {
public:
    XmlImageNode(StringUtils::StaticStringView tag);
    virtual ~XmlImageNode();

public:
    void setImage(std::string_view encodedImage);

    void writeOut(OutputStream* out) override;

private:
    std::string pngData;
};
