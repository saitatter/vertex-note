/*
 * VertexNote
 *
 * Snapping provider for VertexNote geometry objects.
 */

#include "GeometrySnapProvider.h"

#include <algorithm>
#include <cmath>
#include <optional>
#include <utility>

namespace vn::snap {

namespace {

[[nodiscard]] auto distance(geom::Vec2 lhs, geom::Vec2 rhs) -> double {
    return std::hypot(rhs.x - lhs.x, rhs.y - lhs.y);
}

[[nodiscard]] auto midpoint(geom::Vec2 lhs, geom::Vec2 rhs) -> geom::Vec2 {
    return {(lhs.x + rhs.x) * 0.5, (lhs.y + rhs.y) * 0.5};
}

[[nodiscard]] auto projectionOnSegment(geom::Vec2 point, geom::Vec2 start, geom::Vec2 end) -> std::optional<geom::Vec2> {
    const double dx = end.x - start.x;
    const double dy = end.y - start.y;
    const double lengthSquared = dx * dx + dy * dy;
    if (lengthSquared == 0.0) {
        return std::nullopt;
    }

    const double t = ((point.x - start.x) * dx + (point.y - start.y) * dy) / lengthSquared;
    if (t < 0.0 || t > 1.0) {
        return std::nullopt;
    }

    return geom::Vec2{start.x + t * dx, start.y + t * dy};
}

void addCandidate(std::vector<SnapCandidate>& candidates, const SnapQuery& query, SnapKind kind, geom::Vec2 point,
                  double priority, geom::ObjectId object, geom::VertexId vertex = geom::InvalidVertexId,
                  geom::EdgeId edge = geom::InvalidEdgeId) {
    candidates.push_back(SnapCandidate{kind, point, distance(query.pagePoint, point) * query.zoom, priority, object,
                                       vertex, edge});
}

}  // namespace

GeometrySnapProvider::GeometrySnapProvider(std::vector<const geom::GeometryObject*> objects):
        objects(std::move(objects)) {}

void GeometrySnapProvider::setObjects(std::vector<const geom::GeometryObject*> objects) {
    this->objects = std::move(objects);
}

void GeometrySnapProvider::query(const SnapQuery& query, std::vector<SnapCandidate>& candidates) const {
    for (const auto* object: this->objects) {
        if (!object) {
            continue;
        }

        for (const auto& vertex: object->vertices()) {
            addCandidate(candidates, query, SnapKind::ExplicitVertex, vertex.position, 100.0, object->objectId(),
                         vertex.id);
        }

        for (const auto& edge: object->edges()) {
            if (edge.kind != geom::EdgeKind::Line) {
                continue;
            }

            const auto* start = object->vertex(edge.start);
            const auto* end = object->vertex(edge.end);
            if (!start || !end) {
                continue;
            }

            addCandidate(candidates, query, SnapKind::Midpoint, midpoint(start->position, end->position), 70.0,
                         object->objectId(), geom::InvalidVertexId, edge.id);

            if (auto projection = projectionOnSegment(query.pagePoint, start->position, end->position)) {
                addCandidate(candidates, query, SnapKind::EdgeProjection, *projection, 50.0, object->objectId(),
                             geom::InvalidVertexId, edge.id);
            }
        }
    }
}

}  // namespace vn::snap
