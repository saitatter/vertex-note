#include "IntAttribute.h"

#include <string>  // for allocator, string

#include "control/xml/Attribute.h"  // for XMLAttribute
#include "util/OutputStream.h"      // for OutputStream

IntAttribute::IntAttribute(const char8_t* name, int value): XMLAttribute(name) { this->value = value; }

IntAttribute::~IntAttribute() = default;

void IntAttribute::writeOut(OutputStream* out) {
    const auto str = std::to_string(value);
    out->write(str);
}
