#include <algorithm>
#include <fstream>
#include <vector>

#include <gtest/gtest.h>

#include "filesystem.h"
#include "QtToolbarLayoutEngine.h"
#include "QtToolbarProfileStore.h"

TEST(VertexNoteQtToolbarLayoutEngine, parsesToolbarProfilesAndTokens) {
    const auto path = fs::temp_directory_path() / "vertexnote-qt-toolbar-layout-test.ini";
    {
        std::ofstream output(path, std::ios::trunc);
        output << "[custom]\n";
        output << "name = Custom Toolbar\n";
        output << "toolbarTop1 = SAVE, DRAW, DRAW_STROKE, DRAW_VERTEX, SEPARATOR\n";
        output << "toolbarFloat1 = PDF_TOOL\n";
    }

    const auto profile = QtToolbarLayoutEngine::loadProfile(path, "CUSTOM");
    ASSERT_TRUE(profile.has_value());
    EXPECT_EQ(profile->displayName, "Custom Toolbar");

    const auto* top = profile->itemsFor("toolbarTop1");
    ASSERT_NE(top, nullptr);
    const std::vector<std::string> expectedTop = {"SAVE", "DRAW", "DRAW_STROKE", "DRAW_VERTEX", "SEPARATOR"};
    EXPECT_EQ(*top, expectedTop);

    const auto* floating = profile->itemsFor("TOOLBARFLOAT1");
    ASSERT_NE(floating, nullptr);
    const std::vector<std::string> expectedFloating = {"PDF_TOOL"};
    EXPECT_EQ(*floating, expectedFloating);

    fs::remove(path);
}

TEST(VertexNoteQtToolbarLayoutEngine, expandsDrawAliasToStrokeAndVertexTokens) {
    const std::vector<std::string> tokens = {"SAVE", "DRAW", "DRAW_LEGACY", "DRAW_STROKE", "DRAW_VERTEX"};

    const auto expanded = QtToolbarLayoutEngine::expandTokenAliases(tokens);

    const std::vector<std::string> expected = {"SAVE", "DRAW_STROKE", "DRAW_VERTEX", "DRAW_STROKE", "DRAW_STROKE",
                                               "DRAW_VERTEX"};
    EXPECT_EQ(expanded, expected);
}

TEST(VertexNoteQtToolbarLayoutEngine, normalizesLegacyDrawingButtonsToStrokeAndVertexFamilies) {
    const std::vector<std::string> tokens = {
            "PEN", "SEPARATOR", "DRAW_RECTANGLE", "DRAW_ELLIPSE", "SHAPE_RECOGNIZER", "HAND"};

    const auto normalized = QtToolbarLayoutEngine::normalizeQtDrawingFamilies(tokens);

    const std::vector<std::string> expected = {"PEN", "SEPARATOR", "DRAW_STROKE", "DRAW_VERTEX", "HAND"};
    EXPECT_EQ(normalized, expected);
}

TEST(VertexNoteQtToolbarProfileStore, splitsAndJoinsToolbarTokensWithCompatibilityAliases) {
    const auto tokens = splitToolbarTokens(QStringLiteral("SAVE, DRAW_LEGACY, DRAW_VERTEX"));

    const std::vector<std::string> expected = {"SAVE", "DRAW_STROKE", "DRAW_VERTEX"};
    EXPECT_EQ(tokens, expected);
    EXPECT_EQ(joinToolbarTokens(tokens), QStringLiteral("SAVE,DRAW_STROKE,DRAW_VERTEX"));
}

TEST(VertexNoteQtToolbarProfileStore, exposesGeometryWorkspaceProfiles) {
    const auto geometryProfile = QtToolbarLayoutEngine::loadProfile(toolbarProfilePath(), QT_GEOMETRY_PROFILE_ID);
    ASSERT_TRUE(geometryProfile.has_value());
    const auto* geometryTop = geometryProfile->itemsFor("toolbarTop1");
    ASSERT_NE(geometryTop, nullptr);
    EXPECT_NE(std::ranges::find(*geometryTop, "WORKSPACE"), geometryTop->end());
    EXPECT_NE(std::ranges::find(*geometryTop, "GEOMETRY_FILL_FACE"), geometryTop->end());
    EXPECT_NE(std::ranges::find(*geometryTop, "GEOMETRY_WELD"), geometryTop->end());

    const auto modelingProfile = QtToolbarLayoutEngine::loadProfile(toolbarProfilePath(), QT_3D_PROFILE_ID);
    ASSERT_TRUE(modelingProfile.has_value());
    const auto* modelingTop = modelingProfile->itemsFor("toolbarTop1");
    ASSERT_NE(modelingTop, nullptr);
    EXPECT_NE(std::ranges::find(*modelingTop, "WORKSPACE"), modelingTop->end());
    EXPECT_NE(std::ranges::find(*modelingTop, "GEOMETRY_3D_VERTEX"), modelingTop->end());
    EXPECT_NE(std::ranges::find(*modelingTop, "GEOMETRY_3D_BOX"), modelingTop->end());
    EXPECT_NE(std::ranges::find(*modelingTop, "GEOMETRY_PROJECT_ISO"), modelingTop->end());
}
