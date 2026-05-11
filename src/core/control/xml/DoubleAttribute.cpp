#include "DoubleAttribute.h"

#include <array>    // for array
#include <charconv> // for chars_format, to_chars
#include <stdexcept>  // for runtime_error
#include <string>  // for allocator, string
#include <string_view>  // for string_view

#include "control/xml/Attribute.h"  // for XMLAttribute
#include "util/OutputStream.h"      // for OutputStream

DoubleAttribute::DoubleAttribute(const char8_t* name, double value): XMLAttribute(name) { this->value = value; }

DoubleAttribute::~DoubleAttribute() = default;

void DoubleAttribute::writeOut(OutputStream* out) {
    std::array<char, 64> str{};
    const auto [end, ec] = std::to_chars(str.data(), str.data() + str.size(), value, std::chars_format::general, 8);
    if (ec != std::errc{}) {
        throw std::runtime_error("Could not format XML double attribute");
    }
    out->write(std::string_view(str.data(), static_cast<std::size_t>(end - str.data())));
}
