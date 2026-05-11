#include <gtest/gtest.h>

#include "qt/QtCanvasLayout.h"

namespace {

auto page(double width, double height) -> vn::view::render::PageRenderSnapshot {
    vn::view::render::PageRenderSnapshot snapshot;
    snapshot.width = width;
    snapshot.height = height;
    return snapshot;
}

void expectRect(const QRectF& rect, double x, double y, double width, double height) {
    EXPECT_DOUBLE_EQ(rect.x(), x);
    EXPECT_DOUBLE_EQ(rect.y(), y);
    EXPECT_DOUBLE_EQ(rect.width(), width);
    EXPECT_DOUBLE_EQ(rect.height(), height);
}

}  // namespace

TEST(VertexNoteQtCanvasLayout, laysOutSingleColumnByDefault) {
    const std::vector<vn::view::render::PageRenderSnapshot> pages = {
            page(100.0, 200.0),
            page(120.0, 220.0),
            page(80.0, 180.0),
    };

    const auto rects = layoutQtCanvasPages(pages, {.span = 1});

    ASSERT_EQ(rects.size(), 3U);
    expectRect(rects[0], 92.0, 84.0, 100.0, 200.0);
    expectRect(rects[1], 92.0, 332.0, 120.0, 220.0);
    expectRect(rects[2], 92.0, 600.0, 80.0, 180.0);
}

TEST(VertexNoteQtCanvasLayout, laysOutFixedColumns) {
    const std::vector<vn::view::render::PageRenderSnapshot> pages = {
            page(100.0, 200.0), page(120.0, 220.0), page(80.0, 180.0),
            page(90.0, 210.0),  page(130.0, 190.0),
    };

    const auto twoColumns = layoutQtCanvasPages(pages, {.span = 2});
    ASSERT_EQ(twoColumns.size(), 5U);
    expectRect(twoColumns[0], 92.0, 84.0, 100.0, 200.0);
    expectRect(twoColumns[1], 270.0, 84.0, 120.0, 220.0);
    expectRect(twoColumns[2], 92.0, 352.0, 80.0, 180.0);
    expectRect(twoColumns[3], 270.0, 352.0, 90.0, 210.0);
    expectRect(twoColumns[4], 92.0, 610.0, 130.0, 190.0);

    const auto threeColumns = layoutQtCanvasPages(pages, {.span = 3});
    ASSERT_EQ(threeColumns.size(), 5U);
    expectRect(threeColumns[0], 92.0, 84.0, 100.0, 200.0);
    expectRect(threeColumns[1], 240.0, 84.0, 120.0, 220.0);
    expectRect(threeColumns[2], 418.0, 84.0, 80.0, 180.0);
    expectRect(threeColumns[3], 92.0, 352.0, 90.0, 210.0);
    expectRect(threeColumns[4], 240.0, 352.0, 130.0, 190.0);
}

TEST(VertexNoteQtCanvasLayout, appliesPairedPageOffset) {
    const std::vector<vn::view::render::PageRenderSnapshot> pages = {
            page(100.0, 200.0), page(120.0, 220.0), page(80.0, 180.0),
    };

    const auto rects = layoutQtCanvasPages(pages, {.span = 2, .pairOffset = 1});

    ASSERT_EQ(rects.size(), 3U);
    expectRect(rects[0], 260.0, 84.0, 100.0, 200.0);
    expectRect(rects[1], 92.0, 332.0, 120.0, 220.0);
    expectRect(rects[2], 260.0, 332.0, 80.0, 180.0);
}

TEST(VertexNoteQtCanvasLayout, laysOutFixedRows) {
    const std::vector<vn::view::render::PageRenderSnapshot> pages = {
            page(100.0, 200.0), page(120.0, 220.0), page(80.0, 180.0),
            page(90.0, 210.0),  page(130.0, 190.0),
    };

    const auto oneRow = layoutQtCanvasPages(pages, {.span = -1});
    ASSERT_EQ(oneRow.size(), 5U);
    expectRect(oneRow[0], 92.0, 84.0, 100.0, 200.0);
    expectRect(oneRow[1], 240.0, 84.0, 120.0, 220.0);
    expectRect(oneRow[2], 408.0, 84.0, 80.0, 180.0);

    const auto twoRows = layoutQtCanvasPages(pages, {.span = -2});
    ASSERT_EQ(twoRows.size(), 5U);
    expectRect(twoRows[0], 92.0, 84.0, 100.0, 200.0);
    expectRect(twoRows[1], 92.0, 332.0, 120.0, 220.0);
    expectRect(twoRows[2], 260.0, 84.0, 80.0, 180.0);
    expectRect(twoRows[3], 260.0, 332.0, 90.0, 210.0);
    expectRect(twoRows[4], 398.0, 84.0, 130.0, 190.0);
}
