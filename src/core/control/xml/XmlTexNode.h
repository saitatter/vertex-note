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

#include <string>  // for string

#include "XmlNode.h"  // for XmlNode

class OutputStream;

class XmlTexNode: public XmlNode {
public:
    XmlTexNode(StringUtils::StaticStringView tag, std::string&& binaryData);
    virtual ~XmlTexNode();

public:
    void writeOut(OutputStream* out) override;

private:
    /**
     * Binary .PNG or .PDF
     */
    std::string binaryData;
};
