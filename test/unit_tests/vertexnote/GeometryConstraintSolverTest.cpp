/*
 * VertexNote unit tests
 */

#include <gtest/gtest.h>

#include "vertexnote/constraints/GeometryConstraintSolver.h"

using vn::constraints::GeometryConstraintSolver;
using vn::geom::ConstraintKind;
using vn::geom::GeometryObject;
using vn::geom::Vec2;

TEST(VertexNoteGeometryConstraintSolver, appliesHorizontalAndVerticalConstraints) {
    GeometryObject object(42);
    auto a = object.addVertex(Vec2{1.0, 2.0});
    auto b = object.addVertex(Vec2{4.0, 9.0});
    object.addConstraint(ConstraintKind::Horizontal, {a, b});

    GeometryConstraintSolver solver;
    auto result = solver.apply(object);

    EXPECT_TRUE(result.changed);
    EXPECT_DOUBLE_EQ(object.vertex(b)->position.y, 2.0);

    object.addConstraint(ConstraintKind::Vertical, {a, b});
    result = solver.apply(object);

    EXPECT_TRUE(result.changed);
    EXPECT_DOUBLE_EQ(object.vertex(b)->position.x, 1.0);
}

TEST(VertexNoteGeometryConstraintSolver, appliesFixedLengthConstraint) {
    GeometryObject object(42);
    auto a = object.addVertex(Vec2{0.0, 0.0});
    auto b = object.addVertex(Vec2{10.0, 0.0});
    object.addConstraint(ConstraintKind::FixedLength, {a, b}, {}, 4.0);

    GeometryConstraintSolver solver;
    auto result = solver.apply(object);

    EXPECT_TRUE(result.changed);
    EXPECT_DOUBLE_EQ(object.vertex(b)->position.x, 4.0);
    EXPECT_DOUBLE_EQ(object.vertex(b)->position.y, 0.0);
}

TEST(VertexNoteGeometryConstraintSolver, appliesParallelAndPerpendicularEdgeConstraints) {
    GeometryObject object(42);
    auto a = object.addVertex(Vec2{0.0, 0.0});
    auto b = object.addVertex(Vec2{4.0, 0.0});
    auto c = object.addVertex(Vec2{1.0, 1.0});
    auto d = object.addVertex(Vec2{2.0, 3.0});
    auto first = object.addLine(a, b);
    auto second = object.addLine(c, d);
    object.addConstraint(ConstraintKind::Parallel, {}, {first, second});

    GeometryConstraintSolver solver;
    auto result = solver.apply(object);

    EXPECT_TRUE(result.changed);
    EXPECT_DOUBLE_EQ(object.vertex(d)->position.y, 1.0);

    object.addConstraint(ConstraintKind::Perpendicular, {}, {first, second});
    result = solver.apply(object);

    EXPECT_TRUE(result.changed);
    EXPECT_DOUBLE_EQ(object.vertex(d)->position.x, 1.0);
}
