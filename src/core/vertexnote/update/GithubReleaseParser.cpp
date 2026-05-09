#include "GithubReleaseParser.h"

#include <cctype>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

namespace vn::update {
namespace {

auto skipWhitespace(std::string_view payload, size_t pos) -> size_t {
    while (pos < payload.size() && std::isspace(static_cast<unsigned char>(payload[pos]))) {
        ++pos;
    }
    return pos;
}

auto appendEscaped(std::string& out, char escaped) {
    switch (escaped) {
        case '"':
        case '\\':
        case '/':
            out.push_back(escaped);
            break;
        case 'b':
            out.push_back('\b');
            break;
        case 'f':
            out.push_back('\f');
            break;
        case 'n':
            out.push_back('\n');
            break;
        case 'r':
            out.push_back('\r');
            break;
        case 't':
            out.push_back('\t');
            break;
        default:
            out.push_back(escaped);
            break;
    }
}

struct ParsedString {
    std::string value;
    size_t nextPos = 0;
};

auto parseHexDigit(char digit) -> std::optional<std::uint16_t> {
    if (digit >= '0' && digit <= '9') {
        return static_cast<std::uint16_t>(digit - '0');
    }
    if (digit >= 'a' && digit <= 'f') {
        return static_cast<std::uint16_t>(10 + digit - 'a');
    }
    if (digit >= 'A' && digit <= 'F') {
        return static_cast<std::uint16_t>(10 + digit - 'A');
    }
    return std::nullopt;
}

auto appendUtf8CodePoint(std::string& out, std::uint32_t codePoint) -> bool {
    if (codePoint > 0x10FFFFU || (codePoint >= 0xD800U && codePoint <= 0xDFFFU)) {
        return false;
    }

    if (codePoint <= 0x7FU) {
        out.push_back(static_cast<char>(codePoint));
    } else if (codePoint <= 0x7FFU) {
        out.push_back(static_cast<char>(0xC0U | (codePoint >> 6U)));
        out.push_back(static_cast<char>(0x80U | (codePoint & 0x3FU)));
    } else if (codePoint <= 0xFFFFU) {
        out.push_back(static_cast<char>(0xE0U | (codePoint >> 12U)));
        out.push_back(static_cast<char>(0x80U | ((codePoint >> 6U) & 0x3FU)));
        out.push_back(static_cast<char>(0x80U | (codePoint & 0x3FU)));
    } else {
        out.push_back(static_cast<char>(0xF0U | (codePoint >> 18U)));
        out.push_back(static_cast<char>(0x80U | ((codePoint >> 12U) & 0x3FU)));
        out.push_back(static_cast<char>(0x80U | ((codePoint >> 6U) & 0x3FU)));
        out.push_back(static_cast<char>(0x80U | (codePoint & 0x3FU)));
    }

    return true;
}

auto parseUnicodeEscape(std::string_view payload, size_t& pos) -> std::optional<std::uint32_t> {
    auto parseCodeUnit = [&](size_t& cursor) -> std::optional<std::uint16_t> {
        if (cursor + 4U > payload.size()) {
            return std::nullopt;
        }

        std::uint16_t value = 0;
        for (size_t offset = 0; offset < 4U; ++offset) {
            const auto digit = parseHexDigit(payload[cursor + offset]);
            if (!digit) {
                return std::nullopt;
            }
            value = static_cast<std::uint16_t>((value << 4U) | *digit);
        }
        cursor += 4U;
        return value;
    };

    auto codeUnit = parseCodeUnit(pos);
    if (!codeUnit) {
        return std::nullopt;
    }

    std::uint32_t codePoint = *codeUnit;
    if (codePoint >= 0xD800U && codePoint <= 0xDBFFU) {
        if (pos + 2U > payload.size() || payload[pos] != '\\' || payload[pos + 1U] != 'u') {
            return std::nullopt;
        }
        pos += 2U;

        auto lowSurrogate = parseCodeUnit(pos);
        if (!lowSurrogate || *lowSurrogate < 0xDC00U || *lowSurrogate > 0xDFFFU) {
            return std::nullopt;
        }

        codePoint = 0x10000U + ((codePoint - 0xD800U) << 10U) + (*lowSurrogate - 0xDC00U);
    } else if (codePoint >= 0xDC00U && codePoint <= 0xDFFFU) {
        return std::nullopt;
    }

    return codePoint;
}

auto parseStringToken(std::string_view payload, size_t pos) -> std::optional<ParsedString> {
    pos = skipWhitespace(payload, pos);
    if (pos >= payload.size() || payload[pos] != '"') {
        return std::nullopt;
    }

    std::string value;
    ++pos;
    while (pos < payload.size()) {
        const auto ch = payload[pos++];
        if (ch == '"') {
            return ParsedString{std::move(value), pos};
        }
        if (ch == '\\') {
            if (pos >= payload.size()) {
                return std::nullopt;
            }
            const auto escaped = payload[pos++];
            if (escaped == 'u') {
                auto codePoint = parseUnicodeEscape(payload, pos);
                if (!codePoint || !appendUtf8CodePoint(value, *codePoint)) {
                    return std::nullopt;
                }
                continue;
            }
            appendEscaped(value, escaped);
        } else {
            value.push_back(ch);
        }
    }
    return std::nullopt;
}

auto skipBalanced(std::string_view payload, size_t pos, char open, char close) -> std::optional<size_t> {
    if (pos >= payload.size() || payload[pos] != open) {
        return std::nullopt;
    }

    int depth = 0;
    bool inString = false;
    bool escaped = false;
    for (; pos < payload.size(); ++pos) {
        const auto ch = payload[pos];
        if (inString) {
            if (escaped) {
                escaped = false;
            } else if (ch == '\\') {
                escaped = true;
            } else if (ch == '"') {
                inString = false;
            }
            continue;
        }

        if (ch == '"') {
            inString = true;
        } else if (ch == open) {
            ++depth;
        } else if (ch == close) {
            --depth;
            if (depth == 0) {
                return pos + 1U;
            }
        }
    }

    return std::nullopt;
}

auto skipJsonValue(std::string_view payload, size_t pos) -> std::optional<size_t> {
    pos = skipWhitespace(payload, pos);
    if (pos >= payload.size()) {
        return std::nullopt;
    }

    const auto ch = payload[pos];
    if (ch == '"') {
        auto parsed = parseStringToken(payload, pos);
        return parsed ? std::optional<size_t>(parsed->nextPos) : std::nullopt;
    }
    if (ch == '{') {
        return skipBalanced(payload, pos, '{', '}');
    }
    if (ch == '[') {
        return skipBalanced(payload, pos, '[', ']');
    }
    if (payload.substr(pos, 4U) == "true") {
        return pos + 4U;
    }
    if (payload.substr(pos, 5U) == "false") {
        return pos + 5U;
    }
    if (payload.substr(pos, 4U) == "null") {
        return pos + 4U;
    }
    if (ch == '-' || std::isdigit(static_cast<unsigned char>(ch))) {
        ++pos;
        while (pos < payload.size()) {
            const auto current = payload[pos];
            if (!std::isdigit(static_cast<unsigned char>(current)) && current != '.' && current != 'e' &&
                current != 'E' && current != '+' && current != '-') {
                break;
            }
            ++pos;
        }
        return pos;
    }

    return std::nullopt;
}

auto findFieldValue(std::string_view payload, std::string_view key) -> std::optional<size_t> {
    auto pos = skipWhitespace(payload, 0U);
    if (pos >= payload.size() || payload[pos] != '{') {
        return std::nullopt;
    }

    ++pos;
    while (pos < payload.size()) {
        pos = skipWhitespace(payload, pos);
        if (pos >= payload.size()) {
            return std::nullopt;
        }
        if (payload[pos] == '}') {
            return std::nullopt;
        }

        auto memberKey = parseStringToken(payload, pos);
        if (!memberKey) {
            return std::nullopt;
        }

        pos = skipWhitespace(payload, memberKey->nextPos);
        if (pos >= payload.size() || payload[pos] != ':') {
            return std::nullopt;
        }

        ++pos;
        const auto valuePos = skipWhitespace(payload, pos);
        if (memberKey->value == key) {
            return valuePos;
        }

        auto nextPos = skipJsonValue(payload, valuePos);
        if (!nextPos) {
            return std::nullopt;
        }

        pos = skipWhitespace(payload, *nextPos);
        if (pos < payload.size() && payload[pos] == ',') {
            ++pos;
        }
    }

    return std::nullopt;
}

auto parseStringField(std::string_view payload, std::string_view key) -> std::string {
    const auto field = findFieldValue(payload, key);
    if (!field) {
        return {};
    }
    auto parsed = parseStringToken(payload, *field);
    return parsed ? std::move(parsed->value) : std::string{};
}

auto parseBoolField(std::string_view payload, std::string_view key) -> bool {
    const auto field = findFieldValue(payload, key);
    if (!field) {
        return false;
    }
    const auto pos = skipWhitespace(payload, *field);
    return payload.substr(pos, 4) == "true";
}

auto findArray(std::string_view payload, std::string_view key) -> std::optional<std::string_view> {
    const auto field = findFieldValue(payload, key);
    if (!field) {
        return std::nullopt;
    }

    auto pos = skipWhitespace(payload, *field);
    if (pos >= payload.size() || payload[pos] != '[') {
        return std::nullopt;
    }

    const auto begin = pos + 1U;
    const auto end = skipBalanced(payload, pos, '[', ']');
    if (!end || *end == 0U) {
        return std::nullopt;
    }
    return payload.substr(begin, *end - begin - 1U);
}

auto nextObject(std::string_view payload, size_t& cursor) -> std::optional<std::string_view> {
    auto begin = payload.find('{', cursor);
    if (begin == std::string_view::npos) {
        return std::nullopt;
    }

    int depth = 0;
    bool inString = false;
    bool escaped = false;
    for (auto pos = begin; pos < payload.size(); ++pos) {
        const auto ch = payload[pos];
        if (inString) {
            if (escaped) {
                escaped = false;
            } else if (ch == '\\') {
                escaped = true;
            } else if (ch == '"') {
                inString = false;
            }
            continue;
        }

        if (ch == '"') {
            inString = true;
        } else if (ch == '{') {
            ++depth;
        } else if (ch == '}') {
            --depth;
            if (depth == 0) {
                cursor = pos + 1;
                return payload.substr(begin, pos - begin + 1);
            }
        }
    }
    cursor = payload.size();
    return std::nullopt;
}

}  // namespace

auto parseGithubRelease(std::string_view payload) -> std::optional<ReleaseInfo> {
    ReleaseInfo info;
    info.tagName = parseStringField(payload, "tag_name");
    info.name = parseStringField(payload, "name");
    info.htmlUrl = parseStringField(payload, "html_url");
    info.body = parseStringField(payload, "body");
    info.draft = parseBoolField(payload, "draft");
    info.prerelease = parseBoolField(payload, "prerelease");

    if (info.tagName.empty() || info.htmlUrl.empty()) {
        return std::nullopt;
    }

    if (const auto assets = findArray(payload, "assets")) {
        size_t cursor = 0;
        while (const auto object = nextObject(*assets, cursor)) {
            ReleaseAsset asset;
            asset.name = parseStringField(*object, "name");
            asset.downloadUrl = parseStringField(*object, "browser_download_url");
            if (!asset.name.empty() && !asset.downloadUrl.empty()) {
                info.assets.push_back(std::move(asset));
            }
        }
    }

    return info;
}

}  // namespace vn::update
