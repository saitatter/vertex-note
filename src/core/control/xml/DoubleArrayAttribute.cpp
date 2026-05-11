#include "DoubleArrayAttribute.h"

#include <algorithm>  // for for_each
#include <array>      // for array
#include <charconv>   // for chars_format, to_chars
#include <iterator>   // for begin, end
#include <stdexcept>  // for runtime_error
#include <string>     // for allocator, string
#include <string_view>  // for string_view
#include <utility>    // for move

#include "control/xml/Attribute.h"  // for XMLAttribute
#include "util/OutputStream.h"      // for OutputStream

DoubleArrayAttribute::DoubleArrayAttribute(const char8_t* name, std::vector<double>&& values):
        XMLAttribute(name), values(std::move(values)) {}

DoubleArrayAttribute::~DoubleArrayAttribute() = default;

namespace {
void writeDouble(OutputStream* out, double value) {
    std::array<char, 64> str{};
    const auto [end, ec] = std::to_chars(str.data(), str.data() + str.size(), value, std::chars_format::general, 8);
    if (ec != std::errc{}) {
        throw std::runtime_error("Could not format XML double array attribute");
    }
    out->write(std::string_view(str.data(), static_cast<std::size_t>(end - str.data())));
}
}  // namespace

void DoubleArrayAttribute::writeOut(OutputStream* out) {
    if (!this->values.empty()) {
        writeDouble(out, this->values[0]);

        std::for_each(std::begin(this->values) + 1, std::end(this->values), [&](auto& x) {
            out->write(" ");
            writeDouble(out, x);
        });
    }
}
