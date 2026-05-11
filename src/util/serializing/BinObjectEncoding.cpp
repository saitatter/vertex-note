#include "util/serializing/BinObjectEncoding.h"

BinObjectEncoding::BinObjectEncoding() = default;

BinObjectEncoding::~BinObjectEncoding() = default;

void BinObjectEncoding::addData(const void* data, size_t len) {
    this->data.append(static_cast<const char*>(data), len);
}
