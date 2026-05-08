#include "GithubReleaseParser.h"

#include <cctype>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

namespace vn::update {
namespace {

auto findField(std::string_view payload, std::string_view key) -> std::optional<size_t> {
    const auto quotedKey = std::string{"\""} + std::string{key} + "\":";
    const auto pos = payload.find(quotedKey);
    if (pos == std::string_view::npos) {
        return std::nullopt;
    }
    return pos + quotedKey.size();
}

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

auto parseStringAt(std::string_view payload, size_t pos) -> std::optional<std::string> {
    pos = skipWhitespace(payload, pos);
    if (pos >= payload.size() || payload[pos] != '"') {
        return std::nullopt;
    }

    std::string value;
    for (++pos; pos < payload.size(); ++pos) {
        const auto ch = payload[pos];
        if (ch == '"') {
            return value;
        }
        if (ch == '\\') {
            if (++pos >= payload.size()) {
                return std::nullopt;
            }
            if (payload[pos] == 'u') {
                value.push_back('?');
                pos += 4;
                if (pos >= payload.size()) {
                    return std::nullopt;
                }
                continue;
            }
            appendEscaped(value, payload[pos]);
        } else {
            value.push_back(ch);
        }
    }
    return std::nullopt;
}

auto parseStringField(std::string_view payload, std::string_view key) -> std::string {
    const auto field = findField(payload, key);
    if (!field) {
        return {};
    }
    return parseStringAt(payload, *field).value_or(std::string{});
}

auto parseBoolField(std::string_view payload, std::string_view key) -> bool {
    const auto field = findField(payload, key);
    if (!field) {
        return false;
    }
    const auto pos = skipWhitespace(payload, *field);
    return payload.substr(pos, 4) == "true";
}

auto findArray(std::string_view payload, std::string_view key) -> std::optional<std::string_view> {
    const auto field = findField(payload, key);
    if (!field) {
        return std::nullopt;
    }

    auto pos = skipWhitespace(payload, *field);
    if (pos >= payload.size() || payload[pos] != '[') {
        return std::nullopt;
    }

    const auto begin = ++pos;
    int depth = 1;
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
        } else if (ch == '[') {
            ++depth;
        } else if (ch == ']') {
            --depth;
            if (depth == 0) {
                return payload.substr(begin, pos - begin);
            }
        }
    }
    return std::nullopt;
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
