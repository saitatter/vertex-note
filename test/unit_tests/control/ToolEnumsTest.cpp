/*
 * VertexNote
 *
 * This file is part of the Xournal UnitTests
 *
 * @author VertexNote Team
 * https://github.com/vertex-note/vertex-note
 *
 * @license GNU GPLv2 or later
 */

#include <cstddef>
#include <string>

#include <config-test.h>
#include <gtest/gtest.h>

#include "control/ToolEnums.h"

/**
 * Test whether the invariant
 *     fromString(toString(x)) == x
 * holds.
 */
TEST(ToolEnumsTest, testToolSizeSerialization) {
    for (unsigned int i = 0; i <= TOOL_SIZE_NONE; i++) {
        auto toolSize = static_cast<ToolSize>(i);
        std::string s = toolSizeToString(toolSize).data();
        EXPECT_FALSE(s.empty());
        EXPECT_EQ(toolSize, toolSizeFromString(s));
    }
}

/**
 * Test whether the invariant
 *     fromString(toString(x)) == x
 * holds.
 */
TEST(ToolEnumsTest, testToolTypeSerialization) {
    for (unsigned int i = 0; i < TOOL_END_ENTRY; i++) {
        auto toolType = static_cast<ToolType>(i);
        std::string s = toolTypeToString(toolType).data();
        EXPECT_FALSE(s.empty());
        EXPECT_EQ(toolType, toolTypeFromString(s));
    }
}

/**
 * Test whether the invariant
 *     fromString(toString(x)) == x
 * holds.
 */
TEST(ToolEnumsTest, testDrawingTypeSerialization) {
    for (size_t i = 0; i < drawingTypeNames.size(); i++) {
        auto drawingType = static_cast<DrawingType>(i);
        std::string s = drawingTypeToString(drawingType).data();
        EXPECT_FALSE(s.empty());
        EXPECT_EQ(drawingType, drawingTypeFromString(s));
    }
}
