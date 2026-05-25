/*
 * VertexNote
 *
 * Shared hit-testing helpers for object-based geometry render models.
 */

#include "GeometryHitTest.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <vector>

namespace {

constexpr double Tau = 6.28318530717958647692;

auto distanceToSegment(double px, double py, double ax, double ay, double bx, double by) -> std::pair<double, Point> {
    const double dx = bx - ax;
    const double dy = by - ay;
    const double lengthSquared = dx * dx + dy * dy;
    if (lengthSquared == 0.0) {
        return {std::hypot(px - ax, py - ay), Point(ax, ay)};
    }

    const double t = std::clamp(((px - ax) * dx + (py - ay) * dy) / lengthSquared, 0.0, 1.0);
    const double projX = ax + t * dx;
    const double projY = ay + t * dy;
    return {std::hypot(px - projX, py - projY), Point(projX, projY)};
}

auto distanceToInfiniteLine(double px, double py, double ax, double ay, double bx, double by)
        -> std::pair<double, Point> {
    const double dx = bx - ax;
    const double dy = by - ay;
    const double lengthSquared = dx * dx + dy * dy;
    if (lengthSquared == 0.0) {
        return {std::hypot(px - ax, py - ay), Point(ax, ay)};
    }

    const double t = ((px - ax) * dx + (py - ay) * dy) / lengthSquared;
    const double projX = ax + t * dx;
    const double projY = ay + t * dy;
    return {std::hypot(px - projX, py - projY), Point(projX, projY)};
}

auto normalizeAngle(double angle) -> double {
    angle = std::fmod(angle, Tau);
    if (angle < 0.0) {
        angle += Tau;
    }
    return angle;
}

auto angleWithinSweep(double angle, double startAngle, double endAngle) -> bool {
    angle = normalizeAngle(angle);
    startAngle = normalizeAngle(startAngle);
    endAngle = normalizeAngle(endAngle);
    if (endAngle <= startAngle) {
        endAngle += Tau;
    }
    if (angle < startAngle) {
        angle += Tau;
    }
    return angle >= startAngle && angle <= endAngle;
}

auto pointInPolygon(double px, double py, const std::vector<Point>& polygon) -> bool {
    if (polygon.size() < 3U) {
        return false;
    }

    bool inside = false;
    for (std::size_t i = 0U, j = polygon.size() - 1U; i < polygon.size(); j = i++) {
        const auto& lhs = polygon[i];
        const auto& rhs = polygon[j];
        const bool crosses = (lhs.y > py) != (rhs.y > py);
        if (!crosses) {
            continue;
        }
        const double x = (rhs.x - lhs.x) * (py - lhs.y) / (rhs.y - lhs.y) + lhs.x;
        if (px < x) {
            inside = !inside;
        }
    }
    return inside;
}

}  // namespace

namespace vn::view::render {

auto hitTestGeometry(const GeometryRenderModel& geometry, double pageX, double pageY, double zoom, double maxScreenDistance)
        -> std::optional<GeometryHitResult> {
    if (zoom <= 0.0) {
        return std::nullopt;
    }

    std::optional<GeometryHitResult> bestVertex;
    for (const auto& vertex: geometry.vertices) {
        const double distance = std::hypot((vertex.position.x - pageX) * zoom, (vertex.position.y - pageY) * zoom);
        if (distance > maxScreenDistance) {
            continue;
        }

        if (!bestVertex || distance < bestVertex->screenDistance) {
            bestVertex = GeometryHitResult{.type = GeometryHitType::Vertex,
                                           .objectId = geometry.objectId,
                                           .vertexId = vertex.id,
                                           .point = vertex.position,
                                           .snapKind = vn::snap::SnapKind::ExplicitVertex,
                                           .screenDistance = distance};
        }
    }
    if (bestVertex) {
        return bestVertex;
    }

    for (const auto& face: geometry.faces) {
        if (!pointInPolygon(pageX, pageY, face.vertices)) {
            continue;
        }

        return GeometryHitResult{.type = GeometryHitType::Face,
                                 .objectId = geometry.objectId,
                                 .faceId = face.id,
                                 .point = Point(pageX, pageY),
                                 .snapKind = std::nullopt,
                                 .screenDistance = 0.0};
    }

    std::optional<GeometryHitResult> bestEdge;
    for (const auto& edge: geometry.edges) {
        double distance = std::numeric_limits<double>::infinity();
        Point projected;
        std::optional<vn::snap::SnapKind> snapKind = vn::snap::SnapKind::EdgeProjection;

        if (edge.kind == vn::geom::EdgeKind::Line) {
            std::tie(distance, projected) = distanceToSegment(pageX, pageY, edge.start.x, edge.start.y, edge.end.x, edge.end.y);
        } else if (edge.kind == vn::geom::EdgeKind::ConstructionLine) {
            std::tie(distance, projected) =
                    distanceToInfiniteLine(pageX, pageY, edge.start.x, edge.start.y, edge.end.x, edge.end.y);
        } else if ((edge.kind == vn::geom::EdgeKind::Arc || edge.kind == vn::geom::EdgeKind::ConstructionCircle) &&
                   !edge.controls.empty()) {
            const auto& center = edge.controls.front();
            const double radius = std::hypot(edge.start.x - center.x, edge.start.y - center.y);
            const double queryRadius = std::hypot(pageX - center.x, pageY - center.y);
            if (radius == 0.0 || queryRadius == 0.0) {
                continue;
            }

            const double queryAngle = std::atan2(pageY - center.y, pageX - center.x);
            if (!edge.closedLoop) {
                const double startAngle = std::atan2(edge.start.y - center.y, edge.start.x - center.x);
                const double endAngle = std::atan2(edge.end.y - center.y, edge.end.x - center.x);
                if (!angleWithinSweep(queryAngle, startAngle, endAngle)) {
                    continue;
                }
            }

            distance = std::abs(queryRadius - radius);
            const double scale = radius / queryRadius;
            projected = Point(center.x + (pageX - center.x) * scale, center.y + (pageY - center.y) * scale);
        } else {
            continue;
        }

        const double screenDistance = distance * zoom;
        if (screenDistance > maxScreenDistance) {
            continue;
        }

        if (!bestEdge || screenDistance < bestEdge->screenDistance) {
            bestEdge = GeometryHitResult{.type = GeometryHitType::Edge,
                                         .objectId = geometry.objectId,
                                         .edgeId = edge.id,
                                         .point = projected,
                                         .snapKind = snapKind,
                                         .screenDistance = screenDistance};
        }
    }

    return bestEdge;
}

}  // namespace vn::view::render
