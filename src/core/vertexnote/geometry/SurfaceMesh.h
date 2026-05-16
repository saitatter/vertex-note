/*
 * VertexNote
 *
 * Lightweight topology view for object-local geometry editing.
 */

#pragma once

#include <optional>
#include <span>
#include <string>
#include <vector>

#include "vertexnote/geometry/GeometryObject.h"
#include "vertexnote/geometry/GeometryTypes.h"

namespace vn::geom {

struct SurfaceMeshVertex {
    VertexId id = InvalidVertexId;
    Vec3 position;
};

struct SurfaceMeshEdge {
    EdgeId id = InvalidEdgeId;
    EdgeKind kind = EdgeKind::Line;
    VertexId start = InvalidVertexId;
    VertexId end = InvalidVertexId;
    std::vector<VertexId> controls;
};

struct SurfaceMeshFace {
    FaceId id = InvalidFaceId;
    std::vector<VertexId> vertices;
    int fill = 64;
};

struct SurfaceMeshValidationResult {
    bool valid = true;
    std::vector<std::string> errors;
};

struct SurfaceMesh {
    ObjectId objectId = InvalidObjectId;
    std::vector<SurfaceMeshVertex> vertices;
    std::vector<SurfaceMeshEdge> edges;
    std::vector<SurfaceMeshFace> faces;

    [[nodiscard]] static auto fromGeometryObject(const GeometryObject& object) -> SurfaceMesh;
    [[nodiscard]] auto validate() const -> SurfaceMeshValidationResult;
    [[nodiscard]] auto containsVertex(VertexId id) const -> bool;
};

[[nodiscard]] auto validateGeometryTopology(const GeometryObject& object) -> SurfaceMeshValidationResult;

}  // namespace vn::geom
