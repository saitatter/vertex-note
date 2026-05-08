#pragma once

#include <compare>
#include <string_view>

namespace vn::update {

auto compareVersions(std::string_view current, std::string_view candidate) -> std::strong_ordering;
auto isUpdateAvailable(std::string_view current, std::string_view candidate) -> bool;

}  // namespace vn::update
