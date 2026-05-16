/*
 * VertexNote
 *
 * Projection helpers for 3D geometry wireframes.
 */

#pragma once

#include <optional>
#include <vector>

#include "vertexnote/geometry/GeometryTypes.h"
#include "vertexnote/geometry/SurfaceMesh.h"

namespace vn::geom {

struct ProjectionCamera {
    double yaw = 0.0;
    double pitch = 0.0;
    double roll = 0.0;
    double zoom = 1.0;
    Vec2 offset;
};

struct ProjectedPoint {
    Vec2 pagePosition;
    double depth = 0.0;
};

struct ProjectedVertex {
    VertexId id = InvalidVertexId;
    Vec2 pagePosition;
    double depth = 0.0;
};

struct ProjectedEdge {
    EdgeId id = InvalidEdgeId;
    EdgeKind kind = EdgeKind::Line;
    VertexId start = InvalidVertexId;
    VertexId end = InvalidVertexId;
    std::vector<VertexId> controls;
};

struct ProjectedFace {
    FaceId id = InvalidFaceId;
    std::vector<VertexId> vertices;
    int fill = 64;
};

struct GeometryProjectionCache {
    ObjectId objectId = InvalidObjectId;
    ProjectionCamera camera;
    std::vector<ProjectedVertex> vertices;
    std::vector<ProjectedEdge> edges;
    std::vector<ProjectedFace> faces;

    [[nodiscard]] auto vertex(VertexId id) const -> std::optional<ProjectedVertex>;
};

[[nodiscard]] auto projectPoint(Vec3 point, const ProjectionCamera& camera) -> ProjectedPoint;
[[nodiscard]] auto projectSurfaceMesh(const SurfaceMesh& mesh, const ProjectionCamera& camera)
        -> GeometryProjectionCache;

}  // namespace vn::geom
