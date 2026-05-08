#include "VersionComparator.h"

#include <algorithm>
#include <charconv>
#include <cctype>
#include <string>
#include <vector>

namespace vn::update {
namespace {

auto normalizedVersion(std::string_view value) -> std::vector<int> {
    const auto firstDigit = std::find_if(value.begin(), value.end(), [](unsigned char c) { return std::isdigit(c); });
    if (firstDigit == value.end()) {
        return {};
    }

    std::vector<int> parts;
    auto cursor = firstDigit;
    while (cursor != value.end()) {
        if (!std::isdigit(static_cast<unsigned char>(*cursor))) {
            break;
        }

        int part = 0;
        const auto* begin = &*cursor;
        const auto* end = begin;
        while (end != value.data() + value.size() && std::isdigit(static_cast<unsigned char>(*end))) {
            ++end;
        }

        const auto [ptr, ec] = std::from_chars(begin, end, part);
        if (ec != std::errc{} || ptr != end) {
            return {};
        }
        parts.push_back(part);

        cursor = value.begin() + (end - value.data());
        if (cursor == value.end() || *cursor != '.') {
            break;
        }
        ++cursor;
    }

    while (parts.size() > 1 && parts.back() == 0) {
        parts.pop_back();
    }
    return parts;
}

}  // namespace

auto compareVersions(std::string_view current, std::string_view candidate) -> std::strong_ordering {
    auto lhs = normalizedVersion(current);
    auto rhs = normalizedVersion(candidate);
    const auto count = std::max(lhs.size(), rhs.size());
    lhs.resize(count, 0);
    rhs.resize(count, 0);

    for (size_t i = 0; i < count; ++i) {
        if (lhs[i] < rhs[i]) {
            return std::strong_ordering::less;
        }
        if (lhs[i] > rhs[i]) {
            return std::strong_ordering::greater;
        }
    }
    return std::strong_ordering::equal;
}

auto isUpdateAvailable(std::string_view current, std::string_view candidate) -> bool {
    return compareVersions(current, candidate) == std::strong_ordering::less;
}

}  // namespace vn::update
