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

class OutputStream;

class XMLAttribute {
public:
    XMLAttribute(std::u8string name);
    virtual ~XMLAttribute();

public:
    virtual void writeOut(OutputStream* out) = 0;

    std::u8string getName();

private:
    std::u8string name;
};
