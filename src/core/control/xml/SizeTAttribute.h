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

#include <cstddef>  // for size_t

#include "Attribute.h"  // for XMLAttribute

class OutputStream;

class SizeTAttribute: public XMLAttribute {
public:
    SizeTAttribute(const char8_t* name, size_t value);
    ~SizeTAttribute() override;

public:
    void writeOut(OutputStream* out) override;

private:
    size_t value;
};
