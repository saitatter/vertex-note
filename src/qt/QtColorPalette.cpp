/*
 * VertexNote
 *
 * Qt-shell color palette helper.
 */

#include "QtColorPalette.h"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <sstream>
#include <utility>

namespace {

auto trim(std::string value) -> std::string {
    const auto first = std::ranges::find_if(value, [](unsigned char ch) { return !std::isspace(ch); });
    const auto last = std::ranges::find_if(value.rbegin(), value.rend(),
                                           [](unsigned char ch) { return !std::isspace(ch); })
                              .base();
    if (first >= last) {
        return {};
    }
    return std::string(first, last);
}

auto setError(std::string* errorMessage, std::string message) {
    if (errorMessage) {
        *errorMessage = std::move(message);
    }
}

auto isHeaderLine(const std::string& line) -> bool {
    const auto colon = line.find(':');
    if (colon == std::string::npos) {
        return false;
    }
    const auto firstWhitespace = line.find_first_of(" \t");
    return firstWhitespace == std::string::npos || colon < firstWhitespace;
}

auto parseColorLine(const std::string& line, std::size_t index, QtPaletteColor* color) -> bool {
    std::istringstream stream(line);
    int red = 0;
    int green = 0;
    int blue = 0;
    if (!(stream >> red >> green >> blue)) {
        return false;
    }
    if (red < 0 || red > 255 || green < 0 || green > 255 || blue < 0 || blue > 255) {
        return false;
    }

    std::string name;
    std::getline(stream, name);
    name = trim(name);
    if (name.empty()) {
        name = "Color " + std::to_string(index + 1U);
    }

    *color = QtPaletteColor{std::move(name), Color{static_cast<uint8_t>(red), static_cast<uint8_t>(green),
                                                   static_cast<uint8_t>(blue), 0xffU}};
    return true;
}

}  // namespace

auto qtDefaultColorPalette() -> std::vector<QtPaletteColor> {
    return {
            {"Black", Color{0x00, 0x00, 0x00, 0xff}},
            {"Green", Color{0x00, 0x7a, 0x2f, 0xff}},
            {"Cyan", Color{0x00, 0xa0, 0xa0, 0xff}},
            {"Blue", Color{0x29, 0x40, 0xd0, 0xff}},
            {"Gray", Color{0x5d, 0x5d, 0x5d, 0xff}},
            {"Red", Color{0xc3, 0x00, 0x10, 0xff}},
            {"Magenta", Color{0xc8, 0x00, 0x96, 0xff}},
            {"Orange", Color{0xff, 0x8c, 0x00, 0xff}},
            {"Yellow", Color{0xff, 0xcc, 0x00, 0xff}},
            {"Purple", Color{0x63, 0x39, 0xc8, 0xff}},
            {"White", Color{0xff, 0xff, 0xff, 0xff}},
    };
}

auto qtLoadGplColorPalette(const std::filesystem::path& path, std::string* errorMessage) -> std::vector<QtPaletteColor> {
    if (errorMessage) {
        errorMessage->clear();
    }
    if (path.empty()) {
        setError(errorMessage, "No palette file selected.");
        return {};
    }

    std::ifstream file(path, std::ios::binary);
    if (!file) {
        setError(errorMessage, "Could not open palette file.");
        return {};
    }

    std::string line;
    if (!std::getline(file, line) || trim(line) != "GIMP Palette") {
        setError(errorMessage, "Palette files must start with 'GIMP Palette'.");
        return {};
    }

    std::vector<QtPaletteColor> colors;
    std::size_t lineNumber = 1U;
    while (std::getline(file, line)) {
        ++lineNumber;
        line = trim(line);
        if (line.empty() || line.starts_with('#') || isHeaderLine(line)) {
            continue;
        }

        QtPaletteColor color;
        if (!parseColorLine(line, colors.size(), &color)) {
            setError(errorMessage, "Malformed palette line " + std::to_string(lineNumber) + ".");
            return {};
        }
        colors.push_back(std::move(color));
    }

    if (colors.empty()) {
        setError(errorMessage, "Palette file does not contain any colors.");
    }
    return colors;
}

auto qtLoadColorPaletteOrDefault(const std::filesystem::path& path, std::string* errorMessage)
        -> std::vector<QtPaletteColor> {
    if (path.empty()) {
        if (errorMessage) {
            errorMessage->clear();
        }
        return qtDefaultColorPalette();
    }

    auto colors = qtLoadGplColorPalette(path, errorMessage);
    if (!colors.empty()) {
        return colors;
    }
    return qtDefaultColorPalette();
}

auto qtPaletteColorsOnly(const std::vector<QtPaletteColor>& palette) -> std::vector<Color> {
    std::vector<Color> colors;
    colors.reserve(palette.size());
    for (const auto& entry: palette) {
        colors.push_back(entry.color);
    }
    return colors;
}
