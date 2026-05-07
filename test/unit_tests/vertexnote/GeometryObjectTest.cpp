/*
 * VertexNote unit tests
 */

#include <config-test.h>
#include <gtest/gtest.h>

#include <stdexcept>

#include "model/Stroke.h"
#include "vertexnote/geometry/GeometryObject.h"

using vn::geom::EdgeKind;
using vn::geom::GeometryObject;
using vn::geom::Vec2;

TEST(VertexNoteGeometryObject, createsVerticesAndEdges) {
    GeometryObject object(42);

    auto a = object.addVertex(Vec2{1.0, 2.0});
    auto b = object.addVertex(Vec2{5.0, 7.0});
    auto edge = object.addLine(a, b);

    ASSERT_NE(object.vertex(a), nullptr);
    ASSERT_NE(object.vertex(b), nullptr);
    ASSERT_NE(object.edge(edge), nullptr);

    EXPECT_EQ(object.vertex(a)->owner, 42U);
    EXPECT_EQ(object.edge(edge)->kind, EdgeKind::Line);
    EXPECT_EQ(object.edge(edge)->start, a);
    EXPECT_EQ(object.edge(edge)->end, b);
}

TEST(VertexNoteGeometryObject, computesBoundsFromVertices) {
    GeometryObject object(42);

    object.addVertex(Vec2{4.0, 8.0});
    object.addVertex(Vec2{-2.0, 3.0});
    object.addVertex(Vec2{10.0, -5.0});

    auto bounds = object.bounds();

    ASSERT_TRUE(bounds.has_value());
    EXPECT_DOUBLE_EQ(bounds->minX, -2.0);
    EXPECT_DOUBLE_EQ(bounds->minY, -5.0);
    EXPECT_DOUBLE_EQ(bounds->maxX, 10.0);
    EXPECT_DOUBLE_EQ(bounds->maxY, 8.0);
}

TEST(VertexNoteGeometryObject, createsStrokeFallbackForPolyline) {
    GeometryObject object(42);

    auto a = object.addVertex(Vec2{1.0, 2.0});
    auto b = object.addVertex(Vec2{5.0, 2.0});
    auto c = object.addVertex(Vec2{5.0, 7.0});
    object.addLine(a, b);
    object.addLine(b, c);

    auto stroke = object.makeStrokeFallback(1.5, Colors::black);

    ASSERT_NE(stroke, nullptr);
    EXPECT_DOUBLE_EQ(stroke->getWidth(), 1.5);
    ASSERT_EQ(stroke->getPointCount(), 3U);
    EXPECT_DOUBLE_EQ(stroke->getPoint(0).x, 1.0);
    EXPECT_DOUBLE_EQ(stroke->getPoint(1).x, 5.0);
    EXPECT_DOUBLE_EQ(stroke->getPoint(2).y, 7.0);
}

TEST(VertexNoteGeometryObject, rejectsEdgesWithMissingVertices) {
    GeometryObject object(42);
    auto a = object.addVertex(Vec2{1.0, 2.0});

    EXPECT_THROW(object.addLine(a, 999), std::invalid_argument);
}
