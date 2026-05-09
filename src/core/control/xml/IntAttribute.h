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

#include "Attribute.h"  // for XMLAttribute

class OutputStream;

class IntAttribute: public XMLAttribute {
public:
    IntAttribute(const char8_t* name, int value);
    ~IntAttribute() override;

public:
    void writeOut(OutputStream* out) override;

private:
    int value;
};
