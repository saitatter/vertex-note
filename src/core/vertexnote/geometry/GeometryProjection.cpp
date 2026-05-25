/*
 * VertexNote
 *
 * Projection helpers for 3D geometry wireframes.
 */

#include "vertexnote/geometry/GeometryProjection.h"

#include <algorithm>
#include <cmath>

namespace vn::geom {

auto GeometryProjectionCache::vertex(VertexId id) const -> std::optional<ProjectedVertex> {
    const auto it = std::ranges::find(this->vertices, id, &ProjectedVertex::id);
    return it == this->vertices.end() ? std::nullopt : std::optional<ProjectedVertex>{*it};
}

auto projectPoint(Vec3 point, const ProjectionCamera& camera) -> ProjectedPoint {
    const double cosYaw = std::cos(camera.yaw);
    const double sinYaw = std::sin(camera.yaw);
    const double yawX = cosYaw * point.x + sinYaw * point.z;
    const double yawY = point.y;
    const double yawZ = -sinYaw * point.x + cosYaw * point.z;

    const double cosPitch = std::cos(camera.pitch);
    const double sinPitch = std::sin(camera.pitch);
    const double pitchX = yawX;
    const double pitchY = cosPitch * yawY - sinPitch * yawZ;
    const double pitchZ = sinPitch * yawY + cosPitch * yawZ;

    const double cosRoll = std::cos(camera.roll);
    const double sinRoll = std::sin(camera.roll);
    const double rollX = cosRoll * pitchX - sinRoll * pitchY;
    const double rollY = sinRoll * pitchX + cosRoll * pitchY;

    return ProjectedPoint{
            .pagePosition = Vec2{.x = camera.offset.x + rollX * camera.zoom,
                                 .y = camera.offset.y + rollY * camera.zoom},
            .depth = pitchZ,
    };
}

auto projectSurfaceMesh(const SurfaceMesh& mesh, const ProjectionCamera& camera) -> GeometryProjectionCache {
    GeometryProjectionCache cache;
    cache.objectId = mesh.objectId;
    cache.camera = camera;

    cache.vertices.reserve(mesh.vertices.size());
    for (const auto& vertex: mesh.vertices) {
        const auto projected = projectPoint(vertex.position, camera);
        cache.vertices.push_back(ProjectedVertex{
                .id = vertex.id,
                .pagePosition = projected.pagePosition,
                .depth = projected.depth,
        });
    }

    cache.edges.reserve(mesh.edges.size());
    for (const auto& edge: mesh.edges) {
        cache.edges.push_back(ProjectedEdge{
                .id = edge.id,
                .kind = edge.kind,
                .start = edge.start,
                .end = edge.end,
                .controls = edge.controls,
        });
    }

    cache.faces.reserve(mesh.faces.size());
    for (const auto& face: mesh.faces) {
        cache.faces.push_back(ProjectedFace{
                .id = face.id,
                .vertices = face.vertices,
                .fill = face.fill,
        });
    }

    return cache;
}

}  // namespace vn::geom
