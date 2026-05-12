/*
 * VertexNote unit tests — PageBackgroundRenderModelFactory
 */

#include <gtest/gtest.h>

#include "model/NotePage.h"
#include "util/Util.h"
#include "view/render/PageBackgroundRenderModelFactory.h"

TEST(VertexNotePageBackgroundRenderModelFactory, mapsPageWithGraphBackground) {
    auto page = std::make_shared<NotePage>(400.0, 300.0);
    page->setBackgroundType(PageType(PageTypeFormat::Graph));
    page->setBackgroundColor(Color{0xee, 0xee, 0xee, 0xff});

    const auto model = vn::view::render::PageBackgroundRenderModelFactory::fromPage(page);

    EXPECT_EQ(model.backgroundFormat, PageTypeFormat::Graph);
    EXPECT_EQ(model.backgroundColor.red, 0xee);
    EXPECT_DOUBLE_EQ(model.pageWidth, 400.0);
    EXPECT_DOUBLE_EQ(model.pageHeight, 300.0);
    // Non-PDF pages have pdfPageNumber set to npos (sentinel)
    EXPECT_EQ(model.pdfPageNumber, npos);
}

TEST(VertexNotePageBackgroundRenderModelFactory, mapsPlainBackground) {
    auto page = std::make_shared<NotePage>(595.0, 842.0);
    page->setBackgroundType(PageType(PageTypeFormat::Plain));
    page->setBackgroundColor(Colors::white);

    const auto model = vn::view::render::PageBackgroundRenderModelFactory::fromPage(page);

    EXPECT_EQ(model.backgroundFormat, PageTypeFormat::Plain);
    EXPECT_EQ(model.backgroundColor.red, 0xff);
    EXPECT_EQ(model.backgroundColor.green, 0xff);
    EXPECT_EQ(model.backgroundColor.blue, 0xff);
}

TEST(VertexNotePageBackgroundRenderModelFactory, fromPageTypeUsesDefaults) {
    const auto model = vn::view::render::PageBackgroundRenderModelFactory::fromPageType(PageType(PageTypeFormat::Ruled));

    EXPECT_EQ(model.backgroundFormat, PageTypeFormat::Ruled);
    EXPECT_DOUBLE_EQ(model.pageWidth, 0.0);
    EXPECT_DOUBLE_EQ(model.pageHeight, 0.0);
    EXPECT_EQ(model.layerCount, 0U);
    EXPECT_FALSE(model.annotated);
}

TEST(VertexNotePageBackgroundRenderModelFactory, countsLayers) {
    auto page = std::make_shared<NotePage>(200.0, 200.0);
    // Page starts with one default layer
    const auto model = vn::view::render::PageBackgroundRenderModelFactory::fromPage(page);

    EXPECT_GE(model.layerCount, 1U);
}

TEST(VertexNotePageBackgroundRenderModelFactory, nullPageReturnsDefaults) {
    const auto model = vn::view::render::PageBackgroundRenderModelFactory::fromPage(nullptr);

    EXPECT_EQ(model.backgroundFormat, PageTypeFormat::Plain);
    EXPECT_DOUBLE_EQ(model.pageWidth, 0.0);
    EXPECT_DOUBLE_EQ(model.pageHeight, 0.0);
}
