#include "util/StringUtils.h"

#include <cstring>
#include <sstream>  // std::istringstream
#include <string>
#include <string_view>
#include <utility>

#include <QString>

#include "util/Assert.h"

using std::string;
using std::vector;

auto StringUtils::toLowerCase(const string& input) -> string {
    return QString::fromUtf8(input.data(), static_cast<qsizetype>(input.size())).toLower().toUtf8().toStdString();
}

void StringUtils::replaceAllChars(string& input, const std::vector<replace_pair>& replaces) {
    string out;
    bool found = false;
    for (char c: input) {
        for (const replace_pair& p: replaces) {
            if (c == p.first) {
                out += p.second;
                found = true;
                break;
            }
        }
        if (!found) {
            out += c;
        }
        found = false;
    }
    input = out;
}

auto StringUtils::split(const string& input, char delimiter) -> vector<string> {
    vector<string> tokens;
    string token;
    std::istringstream tokenStream(input);
    while (std::getline(tokenStream, token, delimiter)) {
        tokens.push_back(token);
    }
    return tokens;
}

auto StringUtils::startsWith(std::string_view str, std::string_view start) -> bool {
    return str.compare(0, start.length(), start) == 0;
}

auto StringUtils::endsWith(std::string_view str, std::string_view end) -> bool {
    if (end.size() > str.size()) {
        return false;
    }

    return str.compare(str.length() - end.length(), end.length(), end) == 0;
}

const std::string TRIM_CHARS = "\t\n\v\f\r ";

auto StringUtils::ltrim(std::string str) -> std::string {
    str.erase(0, str.find_first_not_of(TRIM_CHARS));
    return str;
}

auto StringUtils::rtrim(std::string str) -> std::string {
    str.erase(str.find_last_not_of(TRIM_CHARS) + 1);
    return str;
}

auto StringUtils::trim(std::string str) -> std::string { return ltrim(rtrim(std::move(str))); }

auto StringUtils::iequals(const string& a, const string& b) -> bool {
    return QString::compare(QString::fromUtf8(a.data(), static_cast<qsizetype>(a.size())),
                            QString::fromUtf8(b.data(), static_cast<qsizetype>(b.size())), Qt::CaseInsensitive) == 0;
}

auto StringUtils::ellipsize(std::string_view sv, std::size_t max_width) -> std::string {
    constexpr std::string_view ELLIPSIS_STR = "...";
    xoj_assert(max_width > ELLIPSIS_STR.size());
    const auto text = QString::fromUtf8(sv.data(), static_cast<qsizetype>(sv.size()));

    if (text.size() <= static_cast<qsizetype>(max_width)) {
        return std::string{sv};
    }

    std::string str =
            text.left(static_cast<qsizetype>(max_width - ELLIPSIS_STR.size())).toUtf8().toStdString();
    str.append(ELLIPSIS_STR);
    return str;
}

auto StringUtils::markup_escape(std::string_view sv) -> std::string {
    return QString::fromUtf8(sv.data(), static_cast<qsizetype>(sv.size())).toHtmlEscaped().toStdString();
}
