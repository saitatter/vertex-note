/*
 * VertexNote unit tests — StrokeRenderModelFactory
 */

#include <gtest/gtest.h>

#include "model/Stroke.h"
#include "view/render/StrokeRenderModelFactory.h"

TEST(VertexNoteStrokeRenderModelFactory, mapsBasicPenStroke) {
    Stroke stroke;
    stroke.setColor(Color{0xff, 0x00, 0x00, 0xff});
    stroke.setWidth(2.5);
    stroke.setToolType(StrokeTool(StrokeTool::PEN));
    stroke.addPoint(Point(10.0, 20.0));
    stroke.addPoint(Point(30.0, 40.0));
    stroke.addPoint(Point(50.0, 60.0));

    const auto model = vn::view::render::StrokeRenderModelFactory::fromStroke(stroke);

    EXPECT_EQ(model.color.red, 0xff);
    EXPECT_EQ(model.color.green, 0x00);
    EXPECT_EQ(model.color.blue, 0x00);
    EXPECT_EQ(model.color.alpha, 0xff);
    EXPECT_DOUBLE_EQ(model.width, 2.5);
    EXPECT_FALSE(model.highlighter);
    EXPECT_FALSE(model.pressureSensitive);
    ASSERT_EQ(model.points.size(), 3U);
    EXPECT_DOUBLE_EQ(model.points[0].x, 10.0);
    EXPECT_DOUBLE_EQ(model.points[1].y, 40.0);
    EXPECT_DOUBLE_EQ(model.points[2].x, 50.0);
}

TEST(VertexNoteStrokeRenderModelFactory, detectsHighlighter) {
    Stroke stroke;
    stroke.setToolType(StrokeTool(StrokeTool::HIGHLIGHTER));
    stroke.setWidth(8.5);
    stroke.addPoint(Point(0.0, 0.0));
    stroke.addPoint(Point(100.0, 0.0));

    const auto model = vn::view::render::StrokeRenderModelFactory::fromStroke(stroke);

    EXPECT_TRUE(model.highlighter);
    // Highlighter strokes are never marked pressure-sensitive even if points have z
    EXPECT_FALSE(model.pressureSensitive);
}

TEST(VertexNoteStrokeRenderModelFactory, detectsPressureSensitivePen) {
    Stroke stroke;
    stroke.setToolType(StrokeTool(StrokeTool::PEN));
    stroke.setWidth(1.41);
    // Point(x, y, z) — z stores pressure * width
    stroke.addPoint(Point(0.0, 0.0, 0.7));
    stroke.addPoint(Point(10.0, 10.0, 1.2));

    const auto model = vn::view::render::StrokeRenderModelFactory::fromStroke(stroke);

    EXPECT_TRUE(model.pressureSensitive);
    EXPECT_FALSE(model.highlighter);
}

TEST(VertexNoteStrokeRenderModelFactory, mapsFillValue) {
    Stroke stroke;
    stroke.setToolType(StrokeTool(StrokeTool::HIGHLIGHTER));
    stroke.setFill(128);
    stroke.addPoint(Point(0.0, 0.0));
    stroke.addPoint(Point(10.0, 10.0));

    const auto model = vn::view::render::StrokeRenderModelFactory::fromStroke(stroke);

    EXPECT_EQ(model.fill, 128);
}

TEST(VertexNoteStrokeRenderModelFactory, mapsCapStyle) {
    Stroke stroke;
    stroke.setStrokeCapStyle(StrokeCapStyle::BUTT);
    stroke.addPoint(Point(0.0, 0.0));
    stroke.addPoint(Point(10.0, 10.0));

    const auto model = vn::view::render::StrokeRenderModelFactory::fromStroke(stroke);

    EXPECT_EQ(model.capStyle, static_cast<int>(StrokeCapStyle::BUTT));
}
