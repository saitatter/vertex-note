/*
 * VertexNote
 *
 * Object-local vertex and edge graph for future CAD-style elements.
 */

#include "GeometryObject.h"

#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <unordered_set>
#include <utility>

#include "model/Point.h"
#include "model/Stroke.h"

namespace vn::geom {

namespace {

constexpr std::size_t ArcApproximationSegments = 32U;

[[nodiscard]] auto distance(Vec2 a, Vec2 b) -> double { return std::hypot(a.x - b.x, a.y - b.y); }

[[nodiscard]] auto appendLinePoint(std::vector<Vec2>& points, Vec2 point) -> bool {
    if (points.empty() || !(points.back() == point)) {
        points.push_back(point);
        return true;
    }
    return false;
}

void appendArcPolyline(std::vector<Vec2>& points, Vec2 center, Vec2 start, Vec2 end, bool fullCircle) {
    const double radius = distance(center, start);
    if (radius == 0.0) {
        static_cast<void>(appendLinePoint(points, start));
        return;
    }

    const double startAngle = std::atan2(start.y - center.y, start.x - center.x);
    double endAngle = std::atan2(end.y - center.y, end.x - center.x);
    double sweep = 0.0;
    if (fullCircle) {
        sweep = 2.0 * M_PI;
    } else {
        if (endAngle <= startAngle) {
            endAngle += 2.0 * M_PI;
        }
        sweep = endAngle - startAngle;
    }

    const auto segments =
            std::max<std::size_t>(4U, static_cast<std::size_t>(std::ceil(ArcApproximationSegments * sweep / (2.0 * M_PI))));
    for (std::size_t index = 0; index <= segments; ++index) {
        const double t = static_cast<double>(index) / static_cast<double>(segments);
        const double angle = startAngle + sweep * t;
        static_cast<void>(
                appendLinePoint(points, {center.x + std::cos(angle) * radius, center.y + std::sin(angle) * radius}));
    }
}

}  // namespace

GeometryObject::GeometryObject(ObjectId id): id(id) {}

auto GeometryObject::objectId() const -> ObjectId { return this->id; }

void GeometryObject::setObjectId(ObjectId id) {
    this->id = id;
    for (auto& vertex: this->vertexList) {
        vertex.owner = id;
    }
}

auto GeometryObject::addVertex(Vec2 position, VertexFlags flags) -> VertexId {
    const VertexId vertexId = nextVertexId();
    this->vertexList.push_back(Vertex{vertexId, position, this->id, flags});
    return vertexId;
}

auto GeometryObject::addVertexWithId(VertexId id, Vec2 position, VertexFlags flags) -> VertexId {
    if (id == InvalidVertexId || containsVertex(id)) {
        throw std::invalid_argument("GeometryObject::addVertexWithId requires a unique valid vertex id");
    }

    this->vertexList.push_back(Vertex{id, position, this->id, flags});
    this->nextLocalVertexId = std::max(this->nextLocalVertexId, id + 1U);
    return id;
}

auto GeometryObject::addEdge(EdgeKind kind, VertexId start, VertexId end, std::vector<VertexId> controls) -> EdgeId {
    if (!containsVertex(start) || !containsVertex(end)) {
        throw std::invalid_argument("GeometryObject::addEdge requires existing endpoint vertices");
    }
    if (!std::ranges::all_of(controls, [this](VertexId id) { return containsVertex(id); })) {
        throw std::invalid_argument("GeometryObject::addEdge requires existing control vertices");
    }

    const EdgeId edgeId = nextEdgeId();
    this->edgeList.push_back(Edge{edgeId, kind, start, end, std::move(controls)});
    return edgeId;
}

auto GeometryObject::addEdgeWithId(EdgeId id, EdgeKind kind, VertexId start, VertexId end,
                                   std::vector<VertexId> controls) -> EdgeId {
    if (id == InvalidEdgeId || containsEdge(id)) {
        throw std::invalid_argument("GeometryObject::addEdgeWithId requires a unique valid edge id");
    }
    if (!containsVertex(start) || !containsVertex(end)) {
        throw std::invalid_argument("GeometryObject::addEdgeWithId requires existing endpoint vertices");
    }
    if (!std::ranges::all_of(controls, [this](VertexId vertexId) { return containsVertex(vertexId); })) {
        throw std::invalid_argument("GeometryObject::addEdgeWithId requires existing control vertices");
    }

    this->edgeList.push_back(Edge{id, kind, start, end, std::move(controls)});
    this->nextLocalEdgeId = std::max(this->nextLocalEdgeId, id + 1U);
    return id;
}

auto GeometryObject::addLine(VertexId start, VertexId end) -> EdgeId { return addEdge(EdgeKind::Line, start, end); }

auto GeometryObject::addConstraint(ConstraintKind kind, std::vector<VertexId> vertices, std::vector<EdgeId> edges,
                                   double value) -> ConstraintId {
    if (!std::ranges::all_of(vertices, [this](VertexId id) { return containsVertex(id); })) {
        throw std::invalid_argument("GeometryObject::addConstraint requires existing vertices");
    }
    if (!std::ranges::all_of(edges, [this](EdgeId id) { return containsEdge(id); })) {
        throw std::invalid_argument("GeometryObject::addConstraint requires existing edges");
    }

    const ConstraintId constraintId = nextConstraintId();
    this->constraintList.push_back(Constraint{constraintId, kind, std::move(vertices), std::move(edges), value});
    return constraintId;
}

auto GeometryObject::addConstraintWithId(ConstraintId id, ConstraintKind kind, std::vector<VertexId> vertices,
                                         std::vector<EdgeId> edges, double value) -> ConstraintId {
    if (id == InvalidConstraintId || constraint(id) != nullptr) {
        throw std::invalid_argument("GeometryObject::addConstraintWithId requires a unique valid constraint id");
    }
    if (!std::ranges::all_of(vertices, [this](VertexId vertexId) { return containsVertex(vertexId); })) {
        throw std::invalid_argument("GeometryObject::addConstraintWithId requires existing vertices");
    }
    if (!std::ranges::all_of(edges, [this](EdgeId edgeId) { return containsEdge(edgeId); })) {
        throw std::invalid_argument("GeometryObject::addConstraintWithId requires existing edges");
    }

    this->constraintList.push_back(Constraint{id, kind, std::move(vertices), std::move(edges), value});
    this->nextLocalConstraintId = std::max(this->nextLocalConstraintId, id + 1U);
    return id;
}

auto GeometryObject::vertex(VertexId id) -> Vertex* {
    auto it = std::ranges::find(this->vertexList, id, &Vertex::id);
    return it == this->vertexList.end() ? nullptr : &*it;
}

auto GeometryObject::vertex(VertexId id) const -> const Vertex* {
    auto it = std::ranges::find(this->vertexList, id, &Vertex::id);
    return it == this->vertexList.end() ? nullptr : &*it;
}

auto GeometryObject::edge(EdgeId id) -> Edge* {
    auto it = std::ranges::find(this->edgeList, id, &Edge::id);
    return it == this->edgeList.end() ? nullptr : &*it;
}

auto GeometryObject::edge(EdgeId id) const -> const Edge* {
    auto it = std::ranges::find(this->edgeList, id, &Edge::id);
    return it == this->edgeList.end() ? nullptr : &*it;
}

auto GeometryObject::constraint(ConstraintId id) -> Constraint* {
    auto it = std::ranges::find(this->constraintList, id, &Constraint::id);
    return it == this->constraintList.end() ? nullptr : &*it;
}

auto GeometryObject::constraint(ConstraintId id) const -> const Constraint* {
    auto it = std::ranges::find(this->constraintList, id, &Constraint::id);
    return it == this->constraintList.end() ? nullptr : &*it;
}

auto GeometryObject::vertices() const -> std::span<const Vertex> { return this->vertexList; }

auto GeometryObject::edges() const -> std::span<const Edge> { return this->edgeList; }

auto GeometryObject::constraints() const -> std::span<const Constraint> { return this->constraintList; }

auto GeometryObject::bounds() const -> std::optional<Bounds> {
    if (this->vertexList.empty()) {
        return std::nullopt;
    }

    if (std::ranges::any_of(this->edgeList, [](const Edge& edge) {
            return edge.kind == EdgeKind::Arc || edge.kind == EdgeKind::ConstructionCircle;
        })) {
        auto points = toPolyline();
        if (points.empty()) {
            return std::nullopt;
        }

        Bounds result{points.front().x, points.front().y, points.front().x, points.front().y};
        for (const auto& point: points) {
            result.minX = std::min(result.minX, point.x);
            result.minY = std::min(result.minY, point.y);
            result.maxX = std::max(result.maxX, point.x);
            result.maxY = std::max(result.maxY, point.y);
        }
        for (const auto& edge: this->edgeList) {
            if ((edge.kind != EdgeKind::Arc && edge.kind != EdgeKind::ConstructionCircle) || edge.start != edge.end ||
                edge.controls.empty()) {
                continue;
            }

            const auto* startVertex = vertex(edge.start);
            const auto* centerVertex = vertex(edge.controls.front());
            if (!startVertex || !centerVertex) {
                continue;
            }

            const double radius = distance(centerVertex->position, startVertex->position);
            result.minX = std::min(result.minX, centerVertex->position.x - radius);
            result.minY = std::min(result.minY, centerVertex->position.y - radius);
            result.maxX = std::max(result.maxX, centerVertex->position.x + radius);
            result.maxY = std::max(result.maxY, centerVertex->position.y + radius);
        }
        return result;
    }

    Bounds result{this->vertexList.front().position.x, this->vertexList.front().position.y,
                  this->vertexList.front().position.x, this->vertexList.front().position.y};
    for (const auto& vertex: this->vertexList) {
        result.minX = std::min(result.minX, vertex.position.x);
        result.minY = std::min(result.minY, vertex.position.y);
        result.maxX = std::max(result.maxX, vertex.position.x);
        result.maxY = std::max(result.maxY, vertex.position.y);
    }
    return result;
}

auto GeometryObject::toPolyline() const -> std::vector<Vec2> {
    std::vector<Vec2> points;
    points.reserve(this->edgeList.size() * 2U);

    for (const auto& edge: this->edgeList) {
        const auto* startVertex = vertex(edge.start);
        const auto* endVertex = vertex(edge.end);
        if (!startVertex || !endVertex) {
            continue;
        }

        if ((edge.kind == EdgeKind::Arc || edge.kind == EdgeKind::ConstructionCircle) && !edge.controls.empty()) {
            const auto* centerVertex = vertex(edge.controls.front());
            if (centerVertex) {
                appendArcPolyline(points, centerVertex->position, startVertex->position, endVertex->position,
                                  edge.start == edge.end);
                continue;
            }
        }

        static_cast<void>(appendLinePoint(points, startVertex->position));
        static_cast<void>(appendLinePoint(points, endVertex->position));
    }

    return points;
}

auto GeometryObject::makeStrokeFallback(double width, Color color) const -> std::unique_ptr<Stroke> {
    auto stroke = std::make_unique<Stroke>();
    stroke->setWidth(width);
    stroke->setColor(color);
    stroke->setToolType(StrokeTool::PEN);

    auto points = toPolyline();
    std::vector<Point> strokePoints;
    strokePoints.reserve(points.size());
    for (const auto& point: points) {
        strokePoints.emplace_back(point.x, point.y);
    }

    if (strokePoints.size() == 1U) {
        strokePoints.push_back(strokePoints.front());
    }
    stroke->setPointVector(std::move(strokePoints));
    return stroke;
}

auto GeometryObject::setVertexPosition(VertexId id, Vec2 position) -> bool {
    auto* target = vertex(id);
    if (!target) {
        return false;
    }

    target->position = position;
    return true;
}

auto GeometryObject::removeVertex(VertexId id) -> bool {
    auto vertexIt = std::ranges::find(this->vertexList, id, &Vertex::id);
    if (vertexIt == this->vertexList.end()) {
        return false;
    }

    std::vector<EdgeId> removedEdges;
    this->edgeList.erase(std::remove_if(this->edgeList.begin(), this->edgeList.end(),
                                        [id, &removedEdges](const Edge& edge) {
                                            const bool referenced = edge.start == id || edge.end == id ||
                                                                    std::ranges::find(edge.controls, id) !=
                                                                            edge.controls.end();
                                            if (referenced) {
                                                removedEdges.push_back(edge.id);
                                            }
                                            return referenced;
                                        }),
                         this->edgeList.end());

    this->constraintList.erase(
            std::remove_if(this->constraintList.begin(), this->constraintList.end(),
                           [id, &removedEdges](const Constraint& constraint) {
                               return std::ranges::find(constraint.vertices, id) != constraint.vertices.end() ||
                                      std::ranges::any_of(removedEdges, [&constraint](EdgeId edgeId) {
                                          return std::ranges::find(constraint.edges, edgeId) != constraint.edges.end();
                                      });
                           }),
            this->constraintList.end());

    this->vertexList.erase(vertexIt);
    return true;
}

auto GeometryObject::removeEdge(EdgeId id) -> bool {
    const auto oldSize = this->edgeList.size();
    this->edgeList.erase(std::remove_if(this->edgeList.begin(), this->edgeList.end(),
                                        [id](const Edge& edge) { return edge.id == id; }),
                         this->edgeList.end());
    if (this->edgeList.size() == oldSize) {
        return false;
    }

    this->constraintList.erase(
            std::remove_if(this->constraintList.begin(), this->constraintList.end(),
                           [id](const Constraint& constraint) {
                               return std::ranges::find(constraint.edges, id) != constraint.edges.end();
                           }),
            this->constraintList.end());
    cleanupDanglingVertices();
    return true;
}

auto GeometryObject::insertVertexOnEdge(EdgeId edgeId, Vec2 position) -> std::optional<VertexId> {
    auto* target = edge(edgeId);
    if (!target || target->kind != EdgeKind::Line || !containsVertex(target->start) || !containsVertex(target->end)) {
        return std::nullopt;
    }

    const VertexId originalEnd = target->end;
    const VertexId inserted = addVertex(position);
    target = edge(edgeId);
    target->end = inserted;
    addLine(inserted, originalEnd);
    return inserted;
}

auto GeometryObject::removeConstraint(ConstraintId id) -> bool {
    const auto oldSize = this->constraintList.size();
    this->constraintList.erase(std::remove_if(this->constraintList.begin(), this->constraintList.end(),
                                             [id](const Constraint& constraint) { return constraint.id == id; }),
                               this->constraintList.end());
    return this->constraintList.size() != oldSize;
}

auto GeometryObject::replaceConstraint(Constraint constraint) -> bool {
    if (constraint.id == InvalidConstraintId) {
        return false;
    }
    if (!std::ranges::all_of(constraint.vertices, [this](VertexId id) { return containsVertex(id); }) ||
        !std::ranges::all_of(constraint.edges, [this](EdgeId id) { return containsEdge(id); })) {
        return false;
    }

    auto* existing = this->constraint(constraint.id);
    if (!existing) {
        this->constraintList.push_back(std::move(constraint));
        return true;
    }

    *existing = std::move(constraint);
    return true;
}

void GeometryObject::move(double dx, double dy) {
    for (auto& vertex: this->vertexList) {
        vertex.position.x += dx;
        vertex.position.y += dy;
    }
}

void GeometryObject::scale(double x0, double y0, double fx, double fy, double rotation) {
    const double cosRotation = std::cos(rotation);
    const double sinRotation = std::sin(rotation);

    for (auto& vertex: this->vertexList) {
        double x = vertex.position.x - x0;
        double y = vertex.position.y - y0;

        double rotatedX = cosRotation * x - sinRotation * y;
        double rotatedY = sinRotation * x + cosRotation * y;

        rotatedX *= fx;
        rotatedY *= fy;

        vertex.position.x = x0 + cosRotation * rotatedX + sinRotation * rotatedY;
        vertex.position.y = y0 - sinRotation * rotatedX + cosRotation * rotatedY;
    }
}

void GeometryObject::rotate(double x0, double y0, double rotation) {
    const double cosRotation = std::cos(rotation);
    const double sinRotation = std::sin(rotation);

    for (auto& vertex: this->vertexList) {
        const double x = vertex.position.x - x0;
        const double y = vertex.position.y - y0;
        vertex.position.x = x0 + cosRotation * x - sinRotation * y;
        vertex.position.y = y0 + sinRotation * x + cosRotation * y;
    }
}

auto GeometryObject::containsVertex(VertexId id) const -> bool { return vertex(id) != nullptr; }

auto GeometryObject::containsEdge(EdgeId id) const -> bool { return edge(id) != nullptr; }

void GeometryObject::cleanupDanglingVertices() {
    std::unordered_set<VertexId> existingVertices;
    existingVertices.reserve(this->vertexList.size());
    for (const auto& vertex: this->vertexList) {
        existingVertices.insert(vertex.id);
    }

    std::unordered_set<EdgeId> existingEdges;
    existingEdges.reserve(this->edgeList.size());
    for (const auto& edge: this->edgeList) {
        existingEdges.insert(edge.id);
    }

    this->constraintList.erase(
            std::remove_if(this->constraintList.begin(), this->constraintList.end(),
                           [&existingVertices, &existingEdges](const Constraint& constraint) {
                               return std::ranges::any_of(constraint.vertices,
                                                          [&existingVertices](VertexId vertexId) {
                                                              return !existingVertices.contains(vertexId);
                                                          }) ||
                                      std::ranges::any_of(constraint.edges, [&existingEdges](EdgeId edgeId) {
                                          return !existingEdges.contains(edgeId);
                                      });
                           }),
            this->constraintList.end());

    // Build a set of all vertex IDs referenced by edges and constraints (O(E + C))
    // so the vertex sweep below is O(V) instead of O(V * (E + C)).
    std::unordered_set<VertexId> referenced;
    for (const auto& edge: this->edgeList) {
        referenced.insert(edge.start);
        referenced.insert(edge.end);
        for (auto cid: edge.controls) {
            referenced.insert(cid);
        }
    }
    for (const auto& constraint: this->constraintList) {
        for (auto vid: constraint.vertices) {
            referenced.insert(vid);
        }
    }

    this->vertexList.erase(std::remove_if(this->vertexList.begin(), this->vertexList.end(),
                                          [&referenced](const Vertex& v) { return !referenced.contains(v.id); }),
                           this->vertexList.end());

    std::unordered_set<VertexId> keptVertices;
    keptVertices.reserve(this->vertexList.size());
    for (const auto& vertex: this->vertexList) {
        keptVertices.insert(vertex.id);
    }

    this->constraintList.erase(
            std::remove_if(this->constraintList.begin(), this->constraintList.end(),
                           [&keptVertices, &existingEdges](const Constraint& constraint) {
                               return std::ranges::any_of(constraint.vertices,
                                                          [&keptVertices](VertexId vertexId) {
                                                              return !keptVertices.contains(vertexId);
                                                          }) ||
                                      std::ranges::any_of(constraint.edges, [&existingEdges](EdgeId edgeId) {
                                          return !existingEdges.contains(edgeId);
                                      });
                           }),
            this->constraintList.end());
}

auto GeometryObject::nextVertexId() -> VertexId { return this->nextLocalVertexId++; }

auto GeometryObject::nextEdgeId() -> EdgeId { return this->nextLocalEdgeId++; }

auto GeometryObject::nextConstraintId() -> ConstraintId { return this->nextLocalConstraintId++; }

}  // namespace vn::geom
