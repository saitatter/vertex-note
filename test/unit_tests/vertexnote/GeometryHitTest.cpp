/*
 * VertexNote unit tests
 */

#include <config-test.h>
#include <gtest/gtest.h>

#include "view/render/GeometryHitTest.h"

namespace {

auto makeLinearGeometry() -> vn::view::render::GeometryRenderModel {
    vn::view::render::GeometryRenderModel geometry;
    geometry.objectId = 101;
    geometry.vertices = {
            {.id = 11, .position = Point(10.0, 10.0)},
            {.id = 12, .position = Point(30.0, 10.0)},
    };
    geometry.edges = {
            {.id = 21, .kind = vn::geom::EdgeKind::Line, .start = Point(10.0, 10.0), .end = Point(30.0, 10.0)},
    };
    return geometry;
}

auto makeCircularGeometry() -> vn::view::render::GeometryRenderModel {
    vn::view::render::GeometryRenderModel geometry;
    geometry.objectId = 102;
    geometry.vertices = {
            {.id = 31, .position = Point(20.0, 20.0)},
            {.id = 32, .position = Point(28.0, 20.0)},
    };
    geometry.edges = {
            {.id = 41,
             .kind = vn::geom::EdgeKind::ConstructionCircle,
             .start = Point(28.0, 20.0),
             .end = Point(28.0, 20.0),
             .controls = {Point(20.0, 20.0)},
             .closedLoop = true},
    };
    return geometry;
}

}  // namespace

TEST(VertexNoteGeometryHitTest, prefersVertexHitsWithinRadius) {
    const auto geometry = makeLinearGeometry();

    const auto hit = vn::view::render::hitTestGeometry(geometry, 10.2, 10.1, 2.0, 8.0);

    ASSERT_TRUE(hit.has_value());
    EXPECT_EQ(hit->type, vn::view::render::GeometryHitType::Vertex);
    EXPECT_EQ(hit->objectId, 101U);
    EXPECT_EQ(hit->vertexId, 11U);
    EXPECT_EQ(hit->snapKind, vn::snap::SnapKind::ExplicitVertex);
}

TEST(VertexNoteGeometryHitTest, projectsLineHitsToEdge) {
    const auto geometry = makeLinearGeometry();

    const auto hit = vn::view::render::hitTestGeometry(geometry, 20.0, 12.0, 2.0, 8.0);

    ASSERT_TRUE(hit.has_value());
    EXPECT_EQ(hit->type, vn::view::render::GeometryHitType::Edge);
    EXPECT_EQ(hit->edgeId, 21U);
    EXPECT_EQ(hit->snapKind, vn::snap::SnapKind::EdgeProjection);
    EXPECT_DOUBLE_EQ(hit->point.x, 20.0);
    EXPECT_DOUBLE_EQ(hit->point.y, 10.0);
}

TEST(VertexNoteGeometryHitTest, supportsCircularEdgeHits) {
    const auto geometry = makeCircularGeometry();

    const auto hit = vn::view::render::hitTestGeometry(geometry, 20.0, 27.0, 1.5, 8.0);

    ASSERT_TRUE(hit.has_value());
    EXPECT_EQ(hit->type, vn::view::render::GeometryHitType::Edge);
    EXPECT_EQ(hit->edgeId, 41U);
    EXPECT_NEAR(hit->point.x, 20.0, 1e-6);
    EXPECT_NEAR(hit->point.y, 28.0, 1e-6);
}
