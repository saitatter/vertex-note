/*
 * VertexNote unit tests
 */

#include <config-test.h>
#include <gtest/gtest.h>

#include <stdexcept>

#include "model/Stroke.h"
#include "vertexnote/geometry/GeometryObject.h"

using vn::geom::EdgeKind;
using vn::geom::ConstraintKind;
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

TEST(VertexNoteGeometryObject, createsStrokeFallbackAndBoundsForFullCircleArc) {
    GeometryObject object(42);

    const auto center = object.addVertex(Vec2{10.0, 10.0});
    const auto radiusPoint = object.addVertex(Vec2{14.0, 10.0});
    object.addEdge(EdgeKind::Arc, radiusPoint, radiusPoint, {center});

    auto stroke = object.makeStrokeFallback(1.5, Colors::black);
    auto bounds = object.bounds();

    ASSERT_NE(stroke, nullptr);
    EXPECT_GT(stroke->getPointCount(), 16U);
    ASSERT_TRUE(bounds.has_value());
    EXPECT_DOUBLE_EQ(bounds->minX, 6.0);
    EXPECT_DOUBLE_EQ(bounds->minY, 6.0);
    EXPECT_DOUBLE_EQ(bounds->maxX, 14.0);
    EXPECT_DOUBLE_EQ(bounds->maxY, 14.0);
}

TEST(VertexNoteGeometryObject, createsStrokeFallbackForPartialArc) {
    GeometryObject object(42);

    const auto center = object.addVertex(Vec2{10.0, 10.0});
    const auto start = object.addVertex(Vec2{14.0, 10.0});
    const auto end = object.addVertex(Vec2{10.0, 14.0});
    object.addEdge(EdgeKind::Arc, start, end, {center});

    auto stroke = object.makeStrokeFallback(1.5, Colors::black);
    auto bounds = object.bounds();

    ASSERT_NE(stroke, nullptr);
    EXPECT_GT(stroke->getPointCount(), 4U);
    ASSERT_TRUE(bounds.has_value());
    EXPECT_DOUBLE_EQ(bounds->minX, 10.0);
    EXPECT_DOUBLE_EQ(bounds->minY, 10.0);
    EXPECT_DOUBLE_EQ(bounds->maxX, 14.0);
    EXPECT_DOUBLE_EQ(bounds->maxY, 14.0);
}

TEST(VertexNoteGeometryObject, rejectsEdgesWithMissingVertices) {
    GeometryObject object(42);
    auto a = object.addVertex(Vec2{1.0, 2.0});

    EXPECT_THROW(object.addLine(a, 999), std::invalid_argument);
}

TEST(VertexNoteGeometryObject, createsConstraints) {
    GeometryObject object(42);
    auto a = object.addVertex(Vec2{1.0, 2.0});
    auto b = object.addVertex(Vec2{5.0, 2.0});
    auto edge = object.addLine(a, b);

    auto constraint = object.addConstraint(ConstraintKind::Horizontal, {a, b}, {edge});

    ASSERT_NE(object.constraint(constraint), nullptr);
    EXPECT_EQ(object.constraint(constraint)->kind, ConstraintKind::Horizontal);
    EXPECT_EQ(object.constraints().size(), 1U);
}

TEST(VertexNoteGeometryObject, rejectsConstraintsWithMissingReferences) {
    GeometryObject object(42);
    auto a = object.addVertex(Vec2{1.0, 2.0});

    EXPECT_THROW(object.addConstraint(ConstraintKind::Coincident, {a, 999}), std::invalid_argument);
    EXPECT_THROW(object.addConstraint(ConstraintKind::OnEdge, {}, {999}), std::invalid_argument);
}

TEST(VertexNoteGeometryObject, insertsVertexOnLineEdge) {
    GeometryObject object(42);
    auto a = object.addVertex({0.0, 0.0});
    auto b = object.addVertex({10.0, 0.0});
    auto edge = object.addLine(a, b);

    auto inserted = object.insertVertexOnEdge(edge, {4.0, 0.0});

    ASSERT_TRUE(inserted.has_value());
    ASSERT_EQ(object.vertices().size(), 3U);
    ASSERT_EQ(object.edges().size(), 2U);
    EXPECT_EQ(object.edge(edge)->end, *inserted);
}

TEST(VertexNoteGeometryObject, removesVertexWithDependentEdgesAndConstraints) {
    GeometryObject object(42);
    auto a = object.addVertex({0.0, 0.0});
    auto b = object.addVertex({10.0, 0.0});
    auto edge = object.addLine(a, b);
    object.addConstraint(ConstraintKind::FixedLength, {a, b}, {edge}, 10.0);

    EXPECT_TRUE(object.removeVertex(b));

    EXPECT_EQ(object.vertices().size(), 1U);
    EXPECT_TRUE(object.edges().empty());
    EXPECT_TRUE(object.constraints().empty());
}

TEST(VertexNoteGeometryObject, replacesAndRemovesConstraints) {
    GeometryObject object(42);
    auto a = object.addVertex({0.0, 0.0});
    auto b = object.addVertex({10.0, 0.0});
    auto constraint = object.addConstraint(ConstraintKind::FixedLength, {a, b}, {}, 10.0);

    EXPECT_TRUE(object.replaceConstraint({constraint, ConstraintKind::FixedLength, {a, b}, {}, 5.0}));
    ASSERT_NE(object.constraint(constraint), nullptr);
    EXPECT_DOUBLE_EQ(object.constraint(constraint)->value, 5.0);

    EXPECT_TRUE(object.removeConstraint(constraint));
    EXPECT_TRUE(object.constraints().empty());
}
