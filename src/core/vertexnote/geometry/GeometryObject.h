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
    auto addConstraint(ConstraintKind kind, std::vector<VertexId> vertices = {}, std::vector<EdgeId> edges = {},
                       double value = 0.0) -> ConstraintId;

    auto addVertexWithId(VertexId id, Vec2 position, VertexFlags flags = VertexFlags::Explicit) -> VertexId;
    auto addEdgeWithId(EdgeId id, EdgeKind kind, VertexId start, VertexId end, std::vector<VertexId> controls = {})
            -> EdgeId;
    auto addConstraintWithId(ConstraintId id, ConstraintKind kind, std::vector<VertexId> vertices = {},
                             std::vector<EdgeId> edges = {}, double value = 0.0) -> ConstraintId;

    [[nodiscard]] auto vertex(VertexId id) -> Vertex*;
    [[nodiscard]] auto vertex(VertexId id) const -> const Vertex*;
    [[nodiscard]] auto edge(EdgeId id) const -> const Edge*;
    [[nodiscard]] auto constraint(ConstraintId id) const -> const Constraint*;

    [[nodiscard]] auto vertices() const -> std::span<const Vertex>;
    [[nodiscard]] auto edges() const -> std::span<const Edge>;
    [[nodiscard]] auto constraints() const -> std::span<const Constraint>;

    [[nodiscard]] auto bounds() const -> std::optional<Bounds>;
    [[nodiscard]] auto toPolyline() const -> std::vector<Vec2>;
    [[nodiscard]] auto makeStrokeFallback(double width, Color color) const -> std::unique_ptr<Stroke>;

    [[nodiscard]] auto setVertexPosition(VertexId id, Vec2 position) -> bool;
    void move(double dx, double dy);
    void scale(double x0, double y0, double fx, double fy, double rotation);
    void rotate(double x0, double y0, double rotation);

private:
    [[nodiscard]] auto containsVertex(VertexId id) const -> bool;
    [[nodiscard]] auto containsEdge(EdgeId id) const -> bool;
    [[nodiscard]] auto nextVertexId() -> VertexId;
    [[nodiscard]] auto nextEdgeId() -> EdgeId;
    [[nodiscard]] auto nextConstraintId() -> ConstraintId;

private:
    ObjectId id = InvalidObjectId;
    VertexId nextLocalVertexId = InvalidVertexId + 1;
    EdgeId nextLocalEdgeId = InvalidEdgeId + 1;
    ConstraintId nextLocalConstraintId = InvalidConstraintId + 1;
    std::vector<Vertex> vertexList;
    std::vector<Edge> edgeList;
    std::vector<Constraint> constraintList;
};

}  // namespace vn::geom
