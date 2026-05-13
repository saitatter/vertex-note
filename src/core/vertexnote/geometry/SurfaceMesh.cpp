/*
 * VertexNote
 *
 * Lightweight topology view for object-local geometry editing.
 */

#include "vertexnote/geometry/SurfaceMesh.h"

#include <ranges>
#include <unordered_set>

namespace vn::geom {

auto SurfaceMesh::fromGeometryObject(const GeometryObject& object) -> SurfaceMesh {
    SurfaceMesh mesh;
    mesh.objectId = object.objectId();
    mesh.vertices.reserve(object.vertices().size());
    for (const auto& vertex: object.vertices()) {
        mesh.vertices.push_back(SurfaceMeshVertex{
                .id = vertex.id,
                .position = Vec3{.x = vertex.position.x, .y = vertex.position.y, .z = 0.0},
        });
    }

    mesh.edges.reserve(object.edges().size());
    for (const auto& edge: object.edges()) {
        mesh.edges.push_back(SurfaceMeshEdge{
                .id = edge.id,
                .kind = edge.kind,
                .start = edge.start,
                .end = edge.end,
                .controls = edge.controls,
        });
    }
    return mesh;
}

auto SurfaceMesh::containsVertex(VertexId id) const -> bool {
    return std::ranges::find(this->vertices, id, &SurfaceMeshVertex::id) != this->vertices.end();
}

auto SurfaceMesh::validate() const -> SurfaceMeshValidationResult {
    SurfaceMeshValidationResult result;
    std::unordered_set<VertexId> vertexIds;
    vertexIds.reserve(this->vertices.size());
    for (const auto& vertex: this->vertices) {
        if (vertex.id == InvalidVertexId) {
            result.valid = false;
            result.errors.emplace_back("surface mesh contains an invalid vertex id");
            continue;
        }
        if (!vertexIds.insert(vertex.id).second) {
            result.valid = false;
            result.errors.emplace_back("surface mesh contains duplicate vertex ids");
        }
    }

    std::unordered_set<EdgeId> edgeIds;
    edgeIds.reserve(this->edges.size());
    for (const auto& edge: this->edges) {
        if (edge.id == InvalidEdgeId) {
            result.valid = false;
            result.errors.emplace_back("surface mesh contains an invalid edge id");
        } else if (!edgeIds.insert(edge.id).second) {
            result.valid = false;
            result.errors.emplace_back("surface mesh contains duplicate edge ids");
        }
        if (!vertexIds.contains(edge.start) || !vertexIds.contains(edge.end)) {
            result.valid = false;
            result.errors.emplace_back("surface mesh edge references a missing endpoint vertex");
        }
        for (auto control: edge.controls) {
            if (!vertexIds.contains(control)) {
                result.valid = false;
                result.errors.emplace_back("surface mesh edge references a missing control vertex");
            }
        }
    }

    for (const auto& face: this->faces) {
        for (auto vertexId: face.vertices) {
            if (!vertexIds.contains(vertexId)) {
                result.valid = false;
                result.errors.emplace_back("surface mesh face references a missing vertex");
            }
        }
    }
    return result;
}

auto validateGeometryTopology(const GeometryObject& object) -> SurfaceMeshValidationResult {
    return SurfaceMesh::fromGeometryObject(object).validate();
}

}  // namespace vn::geom
