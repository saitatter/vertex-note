#include "util/serializing/ObjectEncoding.h"

ObjectEncoding::ObjectEncoding() = default;

ObjectEncoding::~ObjectEncoding() = default;

void ObjectEncoding::addStr(const char* str) { this->data.append(str); }

auto ObjectEncoding::stealData() -> std::string { return std::move(this->data); }
