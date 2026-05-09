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


class DoubleAttribute: public XMLAttribute {
public:
    DoubleAttribute(const char8_t* name, double value);
    ~DoubleAttribute() override;

public:
    void writeOut(OutputStream* out) override;

private:
    double value;
};
