#include "util/serializing/HexObjectEncoding.h"

#include <cstdint>    // for uint8_t
#include <string>     // for string

HexObjectEncoding::HexObjectEncoding() = default;

HexObjectEncoding::~HexObjectEncoding() = default;

void HexObjectEncoding::addData(const void* data, size_t len) {
    constexpr char hex[] = "0123456789abcdef";
    std::string buffer;
    buffer.reserve(len * 2);

    for (size_t i = 0; i < len; i++) {
        uint8_t x = static_cast<uint8_t const*>(data)[i];
        buffer.push_back(hex[x >> 4U]);
        buffer.push_back(hex[x & 0x0FU]);
    }

    this->data.append(buffer);
}
