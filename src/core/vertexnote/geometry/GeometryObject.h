/*
 * VertexNote
 *
 * Object-local vertex and edge graph for future CAD-style elements.
 */

#pragma once

#include <memory>
#include <optional>
#include <span>
#include <vector>

#include "util/Color.h"
#include "vertexnote/geometry/GeometryTypes.h"

class Stroke;

namespace vn::geom {

class GeometryObject {
public:
    explicit GeometryObject(ObjectId id = InvalidObjectId);

    [[nodiscard]] auto objectId() const -> ObjectId;
    void setObjectId(ObjectId id);

    auto addVertex(Vec2 position, VertexFlags flags = VertexFlags::Explicit) -> VertexId;
    auto addEdge(EdgeKind kind, VertexId start, VertexId end, std::vector<VertexId> controls = {}) -> EdgeId;
    auto addLine(VertexId start, VertexId end) -> EdgeId;

    [[nodiscard]] auto vertex(VertexId id) -> Vertex*;
    [[nodiscard]] auto vertex(VertexId id) const -> const Vertex*;
    [[nodiscard]] auto edge(EdgeId id) const -> const Edge*;

    [[nodiscard]] auto vertices() const -> std::span<const Vertex>;
    [[nodiscard]] auto edges() const -> std::span<const Edge>;

    [[nodiscard]] auto bounds() const -> std::optional<Bounds>;
    [[nodiscard]] auto toPolyline() const -> std::vector<Vec2>;
    [[nodiscard]] auto makeStrokeFallback(double width, Color color) const -> std::unique_ptr<Stroke>;

private:
    [[nodiscard]] auto containsVertex(VertexId id) const -> bool;
    [[nodiscard]] auto nextVertexId() -> VertexId;
    [[nodiscard]] auto nextEdgeId() -> EdgeId;

private:
    ObjectId id = InvalidObjectId;
    VertexId nextLocalVertexId = InvalidVertexId + 1;
    EdgeId nextLocalEdgeId = InvalidEdgeId + 1;
    std::vector<Vertex> vertexList;
    std::vector<Edge> edgeList;
};

}  // namespace vn::geom
