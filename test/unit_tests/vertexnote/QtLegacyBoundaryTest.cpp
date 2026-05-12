#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

#include <config-test.h>
#include <gtest/gtest.h>

namespace {

auto readFile(const std::filesystem::path& path) -> std::string {
    std::ifstream in(path, std::ios::binary);
    std::ostringstream out;
    out << in.rdbuf();
    return out.str();
}

}  // namespace

TEST(VertexNoteQtLegacyBoundary, qtSourcesDoNotIncludeGtkGdkCairoOrGtkControl) {
    const std::filesystem::path qtRoot = std::filesystem::path(std::u8string(PROJECT_SOURCE_DIR)) / "src" / "qt";
    const std::vector<std::string> forbidden = {
            "#include <gtk",      "#include <gdk",      "#include <cairo",    "#include \"gtk",
            "#include \"gdk",     "#include \"cairo",   "control/Control.h", "#include \"control/Control",
    };

    ASSERT_TRUE(std::filesystem::is_directory(qtRoot));
    for (const auto& entry: std::filesystem::recursive_directory_iterator(qtRoot)) {
        if (!entry.is_regular_file()) {
            continue;
        }
        const auto extension = entry.path().extension();
        if (extension != ".cpp" && extension != ".h") {
            continue;
        }

        const auto content = readFile(entry.path());
        for (const auto& pattern: forbidden) {
            EXPECT_EQ(std::string::npos, content.find(pattern))
                    << "Qt shell source must keep GTK/GDK/Cairo/legacy Control dependencies behind explicit "
                       "non-Qt boundaries: "
                    << entry.path().string() << " matched " << pattern;
        }
    }
}

TEST(VertexNoteQtLegacyBoundary, qtSourceTreeDoesNotOwnSvgAssets) {
    const std::filesystem::path qtRoot = std::filesystem::path(std::u8string(PROJECT_SOURCE_DIR)) / "src" / "qt";

    ASSERT_TRUE(std::filesystem::is_directory(qtRoot));
    for (const auto& entry: std::filesystem::recursive_directory_iterator(qtRoot)) {
        if (!entry.is_regular_file()) {
            continue;
        }

        EXPECT_NE(entry.path().extension(), ".svg")
                << "Qt-specific SVG assets should live under ui/ so icons and styling assets stay centralized: "
                << entry.path().string();
    }
}
