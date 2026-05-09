/*
 * VertexNote
 *
 * [Header description]
 *
 * @author VertexNote Team
 * https://github.com/saitatter/vertex-note
 *
 * @license GNU GPLv2 or later
 */

#pragma once

#include "XmlNode.h"
#include "filesystem.h"

class XmlAudioNode: public XmlNode {
public:
    XmlAudioNode(StringUtils::StaticStringView tag);
    virtual ~XmlAudioNode();

private:
    XmlAudioNode(const XmlAudioNode& node);
    void operator=(const XmlAudioNode& node);

public:
    [[maybe_unused]] fs::path getAudioFilepath();
    [[maybe_unused]] void setAudioFilepath(fs::path filepath);

private:
    fs::path audioFilepath;
};
