#include <gtest/gtest.h>

#include <algorithm>

#include "control/pagetype/PageTypeHandler.h"

TEST(PageTypeHandlerTest, loadsConfiguredPageTemplates) {
    PageTypeHandler handler(nullptr);
    const auto& types = handler.getPageTypes();

    EXPECT_GE(types.size(), 10U);
    const auto graphWithBorder = std::find_if(types.begin(), types.end(), [](const auto& type) {
        return type->name == "Graph with border";
    });
    ASSERT_NE(graphWithBorder, types.end());
    EXPECT_EQ(PageTypeFormat::Graph, (*graphWithBorder)->page.format);
    EXPECT_EQ("m1=40,rm=1", (*graphWithBorder)->page.config);
}
