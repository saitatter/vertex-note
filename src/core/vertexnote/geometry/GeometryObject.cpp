/*
 * VertexNote
 *
 * Object-local vertex and edge graph for future CAD-style elements.
 */

#include "GeometryObject.h"

#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <utility>

#include "model/Point.h"
#include "model/Stroke.h"

namespace vn::geom {

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

auto GeometryObject::addLine(VertexId start, VertexId end) -> EdgeId { return addEdge(EdgeKind::Line, start, end); }

auto GeometryObject::vertex(VertexId id) -> Vertex* {
    auto it = std::ranges::find(this->vertexList, id, &Vertex::id);
    return it == this->vertexList.end() ? nullptr : &*it;
}

auto GeometryObject::vertex(VertexId id) const -> const Vertex* {
    auto it = std::ranges::find(this->vertexList, id, &Vertex::id);
    return it == this->vertexList.end() ? nullptr : &*it;
}

auto GeometryObject::edge(EdgeId id) const -> const Edge* {
    auto it = std::ranges::find(this->edgeList, id, &Edge::id);
    return it == this->edgeList.end() ? nullptr : &*it;
}

auto GeometryObject::vertices() const -> std::span<const Vertex> { return this->vertexList; }

auto GeometryObject::edges() const -> std::span<const Edge> { return this->edgeList; }

auto GeometryObject::bounds() const -> std::optional<Bounds> {
    if (this->vertexList.empty()) {
        return std::nullopt;
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

        if (points.empty() || !(points.back() == startVertex->position)) {
            points.push_back(startVertex->position);
        }
        points.push_back(endVertex->position);
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

auto GeometryObject::nextVertexId() -> VertexId { return this->nextLocalVertexId++; }

auto GeometryObject::nextEdgeId() -> EdgeId { return this->nextLocalEdgeId++; }

}  // namespace vn::geom
