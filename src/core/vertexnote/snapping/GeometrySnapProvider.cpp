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
constexpr double INTERSECTION_EPSILON = 0.000001;

[[nodiscard]] auto distance(geom::Vec2 lhs, geom::Vec2 rhs) -> double {
    return std::hypot(rhs.x - lhs.x, rhs.y - lhs.y);
}

[[nodiscard]] auto cross(geom::Vec2 lhs, geom::Vec2 rhs) -> double { return lhs.x * rhs.y - lhs.y * rhs.x; }

[[nodiscard]] auto subtract(geom::Vec2 lhs, geom::Vec2 rhs) -> geom::Vec2 { return {lhs.x - rhs.x, lhs.y - rhs.y}; }

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

[[nodiscard]] auto segmentIntersection(const IndexedSegment& lhs, const IndexedSegment& rhs)
        -> std::optional<geom::Vec2> {
    const geom::Vec2 r = subtract(lhs.end, lhs.start);
    const geom::Vec2 s = subtract(rhs.end, rhs.start);
    const double denominator = cross(r, s);
    if (std::abs(denominator) < INTERSECTION_EPSILON) {
        return std::nullopt;
    }

    const geom::Vec2 startDelta = subtract(rhs.start, lhs.start);
    const double t = cross(startDelta, s) / denominator;
    const double u = cross(startDelta, r) / denominator;
    if (t < 0.0 || t > 1.0 || u < 0.0 || u > 1.0) {
        return std::nullopt;
    }

    return geom::Vec2{lhs.start.x + t * r.x, lhs.start.y + t * r.y};
}

[[nodiscard]] auto queryBounds(const SnapQuery& query) -> SpatialBounds {
    const double pageRadius = query.zoom > INTERSECTION_EPSILON ? query.maxScreenDistance / query.zoom :
                                                                  query.maxScreenDistance;
    return SpatialBounds{query.pagePoint.x - pageRadius, query.pagePoint.y - pageRadius,
                         query.pagePoint.x + pageRadius, query.pagePoint.y + pageRadius};
}

void addCandidate(std::vector<SnapCandidate>& candidates, const SnapQuery& query, SnapKind kind, geom::Vec2 point,
                  double priority, geom::ObjectId object, geom::VertexId vertex = geom::InvalidVertexId,
                  geom::EdgeId edge = geom::InvalidEdgeId) {
    candidates.push_back(SnapCandidate{kind, point, distance(query.pagePoint, point) * query.zoom, priority, object,
                                       vertex, edge});
}

}  // namespace

GeometrySnapProvider::GeometrySnapProvider(std::vector<const geom::GeometryObject*> objects):
        objects(std::move(objects)) {
    this->rebuildExplicitVertices();
    this->rebuildLineSegments();
}

void GeometrySnapProvider::setObjects(std::vector<const geom::GeometryObject*> objects) {
    this->objects = std::move(objects);
    this->rebuildExplicitVertices();
    this->rebuildLineSegments();
}

void GeometrySnapProvider::query(const SnapQuery& query, std::vector<SnapCandidate>& candidates) const {
    const SpatialBounds snapBounds = queryBounds(query);
    const auto nearbyVertices = this->explicitVertexIndex.queryPointIndices(snapBounds);
    for (const auto vertexIndex: nearbyVertices) {
        const auto& vertex = this->explicitVertices[vertexIndex];
        if (!overlaps(pointBounds(vertex), snapBounds)) {
            continue;
        }

        addCandidate(candidates, query, SnapKind::ExplicitVertex, vertex.position,
                     query.priorities.priorityFor(SnapKind::ExplicitVertex), vertex.object, vertex.vertex);
    }

    const auto nearbySegments = this->lineSegmentIndex.querySegmentIndices(snapBounds);
    for (const auto segmentIndex: nearbySegments) {
        const auto& segment = this->lineSegments[segmentIndex];
        if (!overlaps(segmentBounds(segment), snapBounds)) {
            continue;
        }

        addCandidate(candidates, query, SnapKind::Midpoint, midpoint(segment.start, segment.end),
                     query.priorities.priorityFor(SnapKind::Midpoint), segment.object, geom::InvalidVertexId,
                     segment.edge);

        if (auto projection = projectionOnSegment(query.pagePoint, segment.start, segment.end)) {
            addCandidate(candidates, query, SnapKind::EdgeProjection, *projection,
                         query.priorities.priorityFor(SnapKind::EdgeProjection), segment.object, geom::InvalidVertexId,
                         segment.edge);
        }
    }

    for (const auto& [lhsIndex, rhsIndex]: this->lineSegmentIndex.querySegmentPairs(snapBounds)) {
        const auto& lhs = this->lineSegments[lhsIndex];
        const auto& rhs = this->lineSegments[rhsIndex];
        if (!overlaps(segmentBounds(lhs), snapBounds) || !overlaps(segmentBounds(rhs), snapBounds)) {
            continue;
        }

        if (auto intersection = segmentIntersection(lhs, rhs)) {
            const double screenDistance = distance(query.pagePoint, *intersection) * query.zoom;
            if (screenDistance <= query.maxScreenDistance + INTERSECTION_EPSILON) {
                addCandidate(candidates, query, SnapKind::Intersection, *intersection,
                             query.priorities.priorityFor(SnapKind::Intersection), lhs.object, geom::InvalidVertexId,
                             lhs.edge);
            }
        }
    }
}

void GeometrySnapProvider::rebuildExplicitVertices() {
    this->explicitVertices.clear();

    for (const auto* object: this->objects) {
        if (!object) {
            continue;
        }
        for (const auto& vertex: object->vertices()) {
            this->explicitVertices.push_back(IndexedPoint{object->objectId(), vertex.id, vertex.position});
        }
    }

    this->explicitVertexIndex.rebuildPoints(this->explicitVertices);
}

void GeometrySnapProvider::rebuildLineSegments() {
    this->lineSegments.clear();

    for (const auto* object: this->objects) {
        if (!object) {
            continue;
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

            this->lineSegments.push_back(IndexedSegment{object->objectId(), edge.id, start->position, end->position});
        }
    }

    this->lineSegmentIndex.rebuild(this->lineSegments);
}

}  // namespace vn::snap
