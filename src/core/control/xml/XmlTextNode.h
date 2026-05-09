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

#include "XmlAudioNode.h"  // for XmlAudioNode

class OutputStream;

class XmlTextNode: public XmlAudioNode {
public:
    XmlTextNode(StringUtils::StaticStringView tag, std::string text);
    explicit XmlTextNode(StringUtils::StaticStringView tag);
    ~XmlTextNode() override = default;

public:
    void setText(std::string text);

    void writeOut(OutputStream* out) override;

private:
    std::string text;
};
