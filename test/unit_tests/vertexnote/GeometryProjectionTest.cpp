/*
 * VertexNote unit tests
 */

#include <cmath>

#include <gtest/gtest.h>

#include "vertexnote/geometry/GeometryProjection.h"

TEST(VertexNoteGeometryProjection, defaultCameraPreservesXYAndDepth) {
    const vn::geom::ProjectionCamera camera;

    const auto projected = vn::geom::projectPoint(vn::geom::Vec3{.x = 12.0, .y = 8.0, .z = 4.0}, camera);

    EXPECT_DOUBLE_EQ(projected.pagePosition.x, 12.0);
    EXPECT_DOUBLE_EQ(projected.pagePosition.y, 8.0);
    EXPECT_DOUBLE_EQ(projected.depth, 4.0);
}

TEST(VertexNoteGeometryProjection, appliesYawPitchRollZoomAndOffset) {
    vn::geom::ProjectionCamera camera;
    camera.yaw = std::acos(-1.0) / 2.0;
    camera.zoom = 2.0;
    camera.offset = {.x = 10.0, .y = -3.0};

    const auto projected = vn::geom::projectPoint(vn::geom::Vec3{.x = 2.0, .y = 5.0, .z = 0.0}, camera);

    EXPECT_NEAR(projected.pagePosition.x, 10.0, 1e-9);
    EXPECT_NEAR(projected.pagePosition.y, 7.0, 1e-9);
    EXPECT_NEAR(projected.depth, -2.0, 1e-9);
}

TEST(VertexNoteGeometryProjection, buildsProjectedCacheFromSurfaceMesh) {
    vn::geom::SurfaceMesh mesh;
    mesh.objectId = 42U;
    mesh.vertices = {
            {.id = 10U, .position = {.x = 0.0, .y = 0.0, .z = 0.0}},
            {.id = 20U, .position = {.x = 10.0, .y = 0.0, .z = 5.0}},
            {.id = 30U, .position = {.x = 0.0, .y = 10.0, .z = 0.0}},
    };
    mesh.edges = {{.id = 50U, .kind = vn::geom::EdgeKind::Line, .start = 10U, .end = 20U}};
    mesh.faces = {{.id = 70U, .vertices = {10U, 20U, 30U}, .fill = 96}};

    vn::geom::ProjectionCamera camera;
    camera.offset = {.x = 5.0, .y = 6.0};
    const auto cache = vn::geom::projectSurfaceMesh(mesh, camera);

    EXPECT_EQ(cache.objectId, 42U);
    ASSERT_EQ(cache.vertices.size(), 3U);
    ASSERT_EQ(cache.edges.size(), 1U);
    ASSERT_EQ(cache.faces.size(), 1U);
    EXPECT_EQ(cache.edges.front().id, 50U);
    EXPECT_EQ(cache.faces.front().fill, 96);

    const auto vertex = cache.vertex(20U);
    ASSERT_TRUE(vertex.has_value());
    EXPECT_DOUBLE_EQ(vertex->pagePosition.x, 15.0);
    EXPECT_DOUBLE_EQ(vertex->pagePosition.y, 6.0);
    EXPECT_DOUBLE_EQ(vertex->depth, 5.0);
}
