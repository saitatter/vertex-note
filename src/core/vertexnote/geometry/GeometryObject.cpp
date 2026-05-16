/*
 * VertexNote
 *
 * Object-local vertex and edge graph for future CAD-style elements.
 */

#include "GeometryObject.h"

#include <algorithm>
#include <cstddef>
#include <cmath>
#include <stdexcept>
#include <unordered_set>
#include <utility>

#include "model/Point.h"
#include "model/Stroke.h"
#include "vertexnote/geometry/GeometryProjection.h"

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

[[nodiscard]] auto faceHasAdjacentBoundary(const Face& face, VertexId start, VertexId end) -> bool {
    if (face.vertices.size() < 2U) {
        return false;
    }
    for (std::size_t index = 0; index < face.vertices.size(); ++index) {
        const VertexId lhs = face.vertices[index];
        const VertexId rhs = face.vertices[(index + 1U) % face.vertices.size()];
        if ((lhs == start && rhs == end) || (lhs == end && rhs == start)) {
            return true;
        }
    }
    return false;
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
    this->vertexList.push_back(Vertex{vertexId, position, Vec3{position.x, position.y, 0.0}, this->id, flags});
    return vertexId;
}

auto GeometryObject::addVertex3D(Vec3 modelPosition, Vec2 projectedPosition, VertexFlags flags) -> VertexId {
    const VertexId vertexId = nextVertexId();
    this->vertexList.push_back(Vertex{vertexId, projectedPosition, modelPosition, this->id, flags});
    return vertexId;
}

auto GeometryObject::addVertexWithId(VertexId id, Vec2 position, VertexFlags flags) -> VertexId {
    if (id == InvalidVertexId || containsVertex(id)) {
        throw std::invalid_argument("GeometryObject::addVertexWithId requires a unique valid vertex id");
    }

    this->vertexList.push_back(Vertex{id, position, Vec3{position.x, position.y, 0.0}, this->id, flags});
    this->nextLocalVertexId = std::max(this->nextLocalVertexId, id + 1U);
    return id;
}

auto GeometryObject::addVertex3DWithId(VertexId id, Vec3 modelPosition, Vec2 projectedPosition, VertexFlags flags)
        -> VertexId {
    if (id == InvalidVertexId || containsVertex(id)) {
        throw std::invalid_argument("GeometryObject::addVertex3DWithId requires a unique valid vertex id");
    }

    this->vertexList.push_back(Vertex{id, projectedPosition, modelPosition, this->id, flags});
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

auto GeometryObject::addFace(std::vector<VertexId> vertices, int fill) -> FaceId {
    vertices = sanitizedFaceVertices(std::move(vertices));
    if (!validateFaceVertices(vertices)) {
        throw std::invalid_argument("GeometryObject::addFace requires at least three existing face vertices");
    }

    const FaceId faceId = nextFaceId();
    this->faceList.push_back(Face{faceId, std::move(vertices), fill});
    return faceId;
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

auto GeometryObject::addFaceWithId(FaceId id, std::vector<VertexId> vertices, int fill) -> FaceId {
    if (id == InvalidFaceId || containsFace(id)) {
        throw std::invalid_argument("GeometryObject::addFaceWithId requires a unique valid face id");
    }

    vertices = sanitizedFaceVertices(std::move(vertices));
    if (!validateFaceVertices(vertices)) {
        throw std::invalid_argument("GeometryObject::addFaceWithId requires at least three existing face vertices");
    }

    this->faceList.push_back(Face{id, std::move(vertices), fill});
    this->nextLocalFaceId = std::max(this->nextLocalFaceId, id + 1U);
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

auto GeometryObject::face(FaceId id) -> Face* {
    auto it = std::ranges::find(this->faceList, id, &Face::id);
    return it == this->faceList.end() ? nullptr : &*it;
}

auto GeometryObject::face(FaceId id) const -> const Face* {
    auto it = std::ranges::find(this->faceList, id, &Face::id);
    return it == this->faceList.end() ? nullptr : &*it;
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

auto GeometryObject::faces() const -> std::span<const Face> { return this->faceList; }

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
    if (!this->faceList.empty()) {
        stroke->setFill(std::clamp(this->faceList.front().fill, 0, 255));
    }

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

auto GeometryObject::makeStrokeFallbacks(double width, Color color) const -> std::vector<std::unique_ptr<Stroke>> {
    const auto makeStroke = [width, color](const std::vector<Vec2>& points, int fill) -> std::unique_ptr<Stroke> {
        if (points.size() < 2U) {
            return nullptr;
        }
        auto stroke = std::make_unique<Stroke>();
        stroke->setWidth(width);
        stroke->setColor(color);
        stroke->setToolType(StrokeTool::PEN);
        if (fill >= 0) {
            stroke->setFill(std::clamp(fill, 0, 255));
        }

        std::vector<Point> strokePoints;
        strokePoints.reserve(points.size());
        for (const auto& point: points) {
            strokePoints.emplace_back(point.x, point.y);
        }
        stroke->setPointVector(std::move(strokePoints));
        return stroke;
    };

    std::vector<std::unique_ptr<Stroke>> strokes;

    for (const auto& face: this->faceList) {
        std::vector<Vec2> points;
        points.reserve(face.vertices.size() + 1U);
        for (auto vertexId: face.vertices) {
            const auto* vertex = this->vertex(vertexId);
            if (!vertex) {
                points.clear();
                break;
            }
            points.push_back(vertex->position);
        }
        if (!points.empty()) {
            points.push_back(points.front());
        }
        if (auto stroke = makeStroke(points, face.fill)) {
            strokes.push_back(std::move(stroke));
        }
    }

    for (const auto& edge: this->edgeList) {
        const auto* startVertex = vertex(edge.start);
        const auto* endVertex = vertex(edge.end);
        if (!startVertex || !endVertex) {
            continue;
        }

        std::vector<Vec2> points;
        if ((edge.kind == EdgeKind::Arc || edge.kind == EdgeKind::ConstructionCircle) && !edge.controls.empty()) {
            const auto* centerVertex = vertex(edge.controls.front());
            if (centerVertex) {
                appendArcPolyline(points, centerVertex->position, startVertex->position, endVertex->position,
                                  edge.start == edge.end);
            }
        } else {
            points.push_back(startVertex->position);
            points.push_back(endVertex->position);
        }
        if (auto stroke = makeStroke(points, -1)) {
            strokes.push_back(std::move(stroke));
        }
    }

    if (strokes.empty()) {
        if (auto fallback = makeStrokeFallback(width, color)) {
            strokes.push_back(std::move(fallback));
        }
    }
    return strokes;
}

auto GeometryObject::setVertexPosition(VertexId id, Vec2 position) -> bool {
    auto* target = vertex(id);
    if (!target) {
        return false;
    }

    const double dx = position.x - target->position.x;
    const double dy = position.y - target->position.y;
    target->position = position;
    target->modelPosition.x += dx;
    target->modelPosition.y += dy;
    return true;
}

auto GeometryObject::setVertexModelPosition(VertexId id, Vec3 position) -> bool {
    auto* target = vertex(id);
    if (!target) {
        return false;
    }

    target->modelPosition = position;
    return true;
}

auto GeometryObject::setVertexZ(VertexId id, double z) -> bool {
    auto* target = vertex(id);
    if (!target || target->modelPosition.z == z) {
        return false;
    }

    target->modelPosition.z = z;
    return true;
}

auto GeometryObject::applyProjection(const ProjectionCamera& camera) -> bool {
    bool changed = false;
    for (auto& vertex: this->vertexList) {
        const auto projected = projectPoint(vertex.modelPosition, camera);
        if (vertex.position.x != projected.pagePosition.x || vertex.position.y != projected.pagePosition.y) {
            vertex.position = projected.pagePosition;
            changed = true;
        }
    }
    return changed;
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

    this->faceList.erase(std::remove_if(this->faceList.begin(), this->faceList.end(),
                                        [id](const Face& face) {
                                            return std::ranges::find(face.vertices, id) != face.vertices.end();
                                        }),
                         this->faceList.end());

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
    const auto edgeIt = std::ranges::find(this->edgeList, id, &Edge::id);
    if (edgeIt == this->edgeList.end()) {
        return false;
    }
    const VertexId removedStart = edgeIt->start;
    const VertexId removedEnd = edgeIt->end;

    this->edgeList.erase(std::remove_if(this->edgeList.begin(), this->edgeList.end(),
                                        [id](const Edge& edge) { return edge.id == id; }),
                         this->edgeList.end());

    this->constraintList.erase(
            std::remove_if(this->constraintList.begin(), this->constraintList.end(),
                           [id](const Constraint& constraint) {
                               return std::ranges::find(constraint.edges, id) != constraint.edges.end();
                           }),
            this->constraintList.end());
    this->faceList.erase(std::remove_if(this->faceList.begin(), this->faceList.end(),
                                        [removedStart, removedEnd](const Face& face) {
                                            return faceHasAdjacentBoundary(face, removedStart, removedEnd);
                                        }),
                         this->faceList.end());
    cleanupDanglingVertices();
    return true;
}

auto GeometryObject::removeFace(FaceId id) -> bool {
    const auto oldSize = this->faceList.size();
    this->faceList.erase(std::remove_if(this->faceList.begin(), this->faceList.end(),
                                        [id](const Face& face) { return face.id == id; }),
                         this->faceList.end());
    if (this->faceList.size() == oldSize) {
        return false;
    }
    cleanupDanglingVertices();
    return true;
}

auto GeometryObject::insertVertexOnEdge(EdgeId edgeId, Vec2 position) -> std::optional<VertexId> {
    auto* target = edge(edgeId);
    if (!target || target->kind != EdgeKind::Line || !containsVertex(target->start) || !containsVertex(target->end)) {
        return std::nullopt;
    }

    const VertexId originalStart = target->start;
    const VertexId originalEnd = target->end;
    Vec3 modelPosition{position.x, position.y, 0.0};
    const auto* startVertex = vertex(originalStart);
    const auto* endVertex = vertex(originalEnd);
    if (startVertex && endVertex) {
        const double dx = endVertex->position.x - startVertex->position.x;
        const double dy = endVertex->position.y - startVertex->position.y;
        const double lengthSquared = dx * dx + dy * dy;
        const double t = lengthSquared <= 1e-9 ? 0.0 :
                         std::clamp(((position.x - startVertex->position.x) * dx +
                                     (position.y - startVertex->position.y) * dy) /
                                            lengthSquared,
                                    0.0, 1.0);
        modelPosition = Vec3{.x = startVertex->modelPosition.x +
                                  (endVertex->modelPosition.x - startVertex->modelPosition.x) * t,
                             .y = startVertex->modelPosition.y +
                                  (endVertex->modelPosition.y - startVertex->modelPosition.y) * t,
                             .z = startVertex->modelPosition.z +
                                  (endVertex->modelPosition.z - startVertex->modelPosition.z) * t};
    }
    const VertexId inserted = addVertex3D(modelPosition, position);
    target = edge(edgeId);
    target->end = inserted;
    addLine(inserted, originalEnd);
    insertVertexIntoFaces(originalStart, originalEnd, inserted);
    return inserted;
}

auto GeometryObject::splitEdgeAtVertex(EdgeId edgeId, VertexId vertexId) -> bool {
    auto* target = edge(edgeId);
    if (!target || !containsVertex(vertexId) || !containsVertex(target->start) || !containsVertex(target->end) ||
        target->start == vertexId || target->end == vertexId ||
        (target->kind != EdgeKind::Line && target->kind != EdgeKind::ConstructionLine)) {
        return false;
    }

    const VertexId originalEnd = target->end;
    const VertexId originalStart = target->start;
    const EdgeKind edgeKind = target->kind;
    target->end = vertexId;
    addEdge(edgeKind, vertexId, originalEnd);
    insertVertexIntoFaces(originalStart, originalEnd, vertexId);
    return true;
}

auto GeometryObject::mergeVertexInto(VertexId source, VertexId target) -> bool {
    if (source == target || !containsVertex(source) || !containsVertex(target)) {
        return false;
    }

    std::unordered_set<EdgeId> existingClosedCurveEdges;
    for (const auto& edge: this->edgeList) {
        if (edge.start == edge.end && (edge.kind == EdgeKind::Arc || edge.kind == EdgeKind::ConstructionCircle)) {
            existingClosedCurveEdges.insert(edge.id);
        }
    }

    for (auto& edge: this->edgeList) {
        if (edge.start == source) {
            edge.start = target;
        }
        if (edge.end == source) {
            edge.end = target;
        }
        for (auto& control: edge.controls) {
            if (control == source) {
                control = target;
            }
        }
    }

    this->edgeList.erase(std::remove_if(this->edgeList.begin(), this->edgeList.end(),
                                        [&existingClosedCurveEdges](const Edge& edge) {
                                            if (edge.start != edge.end) {
                                                return false;
                                            }
                                            if (edge.kind == EdgeKind::Line || edge.kind == EdgeKind::ConstructionLine) {
                                                return true;
                                            }
                                            if (edge.kind == EdgeKind::CubicBezier) {
                                                return true;
                                            }
                                            if (edge.kind != EdgeKind::Arc &&
                                                edge.kind != EdgeKind::ConstructionCircle) {
                                                return false;
                                            }
                                            if (edge.controls.empty() || edge.controls.front() == edge.start) {
                                                return true;
                                            }
                                            return !existingClosedCurveEdges.contains(edge.id);
                                        }),
                         this->edgeList.end());

    for (auto& constraint: this->constraintList) {
        for (auto& vertex: constraint.vertices) {
            if (vertex == source) {
                vertex = target;
            }
        }
        std::ranges::sort(constraint.vertices);
        constraint.vertices.erase(std::unique(constraint.vertices.begin(), constraint.vertices.end()),
                                  constraint.vertices.end());
    }

    for (auto& face: this->faceList) {
        for (auto& vertex: face.vertices) {
            if (vertex == source) {
                vertex = target;
            }
        }
        face.vertices = sanitizedFaceVertices(std::move(face.vertices));
    }

    this->vertexList.erase(std::remove_if(this->vertexList.begin(), this->vertexList.end(),
                                          [source](const Vertex& vertex) { return vertex.id == source; }),
                           this->vertexList.end());
    cleanupDegenerateFaces();
    cleanupDanglingVertices();
    return true;
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
        vertex.modelPosition.x += dx;
        vertex.modelPosition.y += dy;
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
        vertex.modelPosition.x = vertex.position.x;
        vertex.modelPosition.y = vertex.position.y;
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
        vertex.modelPosition.x = vertex.position.x;
        vertex.modelPosition.y = vertex.position.y;
    }
}

auto GeometryObject::containsVertex(VertexId id) const -> bool { return vertex(id) != nullptr; }

auto GeometryObject::containsEdge(EdgeId id) const -> bool { return edge(id) != nullptr; }

auto GeometryObject::containsFace(FaceId id) const -> bool { return face(id) != nullptr; }

void GeometryObject::cleanupDegenerateFaces() {
    this->faceList.erase(std::remove_if(this->faceList.begin(), this->faceList.end(),
                                        [this](const Face& face) { return !validateFaceVertices(face.vertices); }),
                         this->faceList.end());
}

auto GeometryObject::sanitizedFaceVertices(std::vector<VertexId> vertices) -> std::vector<VertexId> {
    std::vector<VertexId> result;
    result.reserve(vertices.size());
    for (auto vertexId: vertices) {
        if (vertexId == InvalidVertexId) {
            continue;
        }
        if (result.empty() || result.back() != vertexId) {
            result.push_back(vertexId);
        }
    }
    if (result.size() > 1U && result.front() == result.back()) {
        result.pop_back();
    }
    return result;
}

auto GeometryObject::validateFaceVertices(const std::vector<VertexId>& vertices) const -> bool {
    if (vertices.size() < 3U) {
        return false;
    }
    std::unordered_set<VertexId> uniqueVertices;
    uniqueVertices.reserve(vertices.size());
    for (auto vertexId: vertices) {
        if (!containsVertex(vertexId)) {
            return false;
        }
        uniqueVertices.insert(vertexId);
    }
    return uniqueVertices.size() >= 3U;
}

void GeometryObject::insertVertexIntoFaces(VertexId start, VertexId end, VertexId inserted) {
    if (start == end || inserted == InvalidVertexId) {
        return;
    }
    for (auto& face: this->faceList) {
        for (std::size_t index = 0; index < face.vertices.size(); ++index) {
            const std::size_t nextIndex = (index + 1U) % face.vertices.size();
            const VertexId lhs = face.vertices[index];
            const VertexId rhs = face.vertices[nextIndex];
            if (lhs == start && rhs == end) {
                face.vertices.insert(face.vertices.begin() + static_cast<std::ptrdiff_t>(nextIndex), inserted);
                break;
            }
            if (lhs == end && rhs == start) {
                face.vertices.insert(face.vertices.begin() + static_cast<std::ptrdiff_t>(nextIndex), inserted);
                break;
            }
        }
    }
}

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
    for (const auto& face: this->faceList) {
        for (auto vid: face.vertices) {
            referenced.insert(vid);
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
}

auto GeometryObject::nextVertexId() -> VertexId { return this->nextLocalVertexId++; }

auto GeometryObject::nextEdgeId() -> EdgeId { return this->nextLocalEdgeId++; }

auto GeometryObject::nextFaceId() -> FaceId { return this->nextLocalFaceId++; }

auto GeometryObject::nextConstraintId() -> ConstraintId { return this->nextLocalConstraintId++; }

}  // namespace vn::geom
