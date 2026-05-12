/*
 * VertexNote
 *
 * Small Qt-shell color palette helper. It intentionally does not use the GTK
 * toolbar palette classes, so the Qt shell can keep that dependency behind a
 * clear legacy boundary.
 */

#pragma once

#include <filesystem>
#include <string>
#include <vector>

#include "util/Color.h"

struct QtPaletteColor {
    std::string name;
    Color color;
};

[[nodiscard]] auto qtDefaultColorPalette() -> std::vector<QtPaletteColor>;
[[nodiscard]] auto qtLoadGplColorPalette(const std::filesystem::path& path, std::string* errorMessage = nullptr)
        -> std::vector<QtPaletteColor>;
[[nodiscard]] auto qtLoadColorPaletteOrDefault(const std::filesystem::path& path,
                                               std::string* errorMessage = nullptr) -> std::vector<QtPaletteColor>;
[[nodiscard]] auto qtPaletteColorsOnly(const std::vector<QtPaletteColor>& palette) -> std::vector<Color>;
