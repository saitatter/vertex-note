#include "QtColorPalette.h"

#include <filesystem>
#include <fstream>

#include <gtest/gtest.h>

namespace {

auto tempPalettePath(const std::string& name) -> std::filesystem::path {
    return std::filesystem::temp_directory_path() / ("vertexnote_" + name + ".gpl");
}

}  // namespace

TEST(VertexNoteQtColorPalette, parsesGplColorPalette) {
    const auto path = tempPalettePath("qt_palette_parse");
    {
        std::ofstream out(path, std::ios::binary);
        out << "GIMP Palette\n"
            << "Name: Test Palette\n"
            << "# comment\n"
            << "12 34 56 Ink\n"
            << "255 128 0 Orange\n";
    }

    std::string error;
    const auto palette = qtLoadGplColorPalette(path, &error);

    EXPECT_TRUE(error.empty());
    ASSERT_EQ(2U, palette.size());
    EXPECT_EQ("Ink", palette[0].name);
    EXPECT_EQ((Color{12, 34, 56, 0xff}), palette[0].color);
    EXPECT_EQ("Orange", palette[1].name);
    EXPECT_EQ((Color{255, 128, 0, 0xff}), palette[1].color);
    std::filesystem::remove(path);
}

TEST(VertexNoteQtColorPalette, fallsBackToDefaultPalette) {
    std::string error;
    const auto palette = qtLoadColorPaletteOrDefault(tempPalettePath("missing_palette"), &error);

    EXPECT_FALSE(error.empty());
    EXPECT_EQ(qtDefaultColorPalette().size(), palette.size());
}
