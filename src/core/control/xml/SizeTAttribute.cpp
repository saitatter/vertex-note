#include "SizeTAttribute.h"

#include <string>  // for allocator, string

#include "control/xml/Attribute.h"  // for XMLAttribute
#include "util/OutputStream.h"      // for OutputStream

SizeTAttribute::SizeTAttribute(const char8_t* name, size_t value): XMLAttribute(name) { this->value = value; }

SizeTAttribute::~SizeTAttribute() = default;

void SizeTAttribute::writeOut(OutputStream* out) {
    const auto str = std::to_string(value);
    out->write(str);
}
