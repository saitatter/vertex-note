/*
 * VertexNote
 *
 * Shared helpers for building geometry render models from core document data.
 */

#include "GeometryRenderModelFactory.h"

#include <algorithm>
#include <cstddef>
#include <cmath>
#include <numeric>
#include <vector>

#include "model/Point.h"
#include "vertexnote/geometry/GeometryElement.h"

namespace vn::view::render {

namespace {

constexpr double PolygonEpsilon = 1e-9;

auto signedArea(const std::vector<Point>& points) -> double {
    double area = 0.0;
    for (std::size_t index = 0; index < points.size(); ++index) {
        const Point& lhs = points[index];
        const Point& rhs = points[(index + 1U) % points.size()];
        area += lhs.x * rhs.y - rhs.x * lhs.y;
    }
    return 0.5 * area;
}

auto cross(const Point& a, const Point& b, const Point& c) -> double {
    return (b.x - a.x) * (c.y - a.y) - (b.y - a.y) * (c.x - a.x);
}

auto pointInTriangle(const Point& point, const Point& a, const Point& b, const Point& c) -> bool {
    const double c0 = cross(a, b, point);
    const double c1 = cross(b, c, point);
    const double c2 = cross(c, a, point);
    const bool hasNegative = c0 < -PolygonEpsilon || c1 < -PolygonEpsilon || c2 < -PolygonEpsilon;
    const bool hasPositive = c0 > PolygonEpsilon || c1 > PolygonEpsilon || c2 > PolygonEpsilon;
    return !(hasNegative && hasPositive);
}

auto triangulatePolygon(const std::vector<Point>& points) -> std::vector<GeometryTriangleRenderModel> {
    std::vector<GeometryTriangleRenderModel> triangles;
    if (points.size() < 3U) {
        return triangles;
    }

    std::vector<std::size_t> indices(points.size());
    std::iota(indices.begin(), indices.end(), 0U);
    const bool counterClockwise = signedArea(points) >= 0.0;
    std::size_t guard = points.size() * points.size();
    while (indices.size() > 3U && guard-- > 0U) {
        bool clipped = false;
        for (std::size_t i = 0; i < indices.size(); ++i) {
            const std::size_t prevIndex = indices[(i + indices.size() - 1U) % indices.size()];
            const std::size_t currIndex = indices[i];
            const std::size_t nextIndex = indices[(i + 1U) % indices.size()];
            const Point& prev = points[prevIndex];
            const Point& curr = points[currIndex];
            const Point& next = points[nextIndex];
            const double turn = cross(prev, curr, next);
            if ((counterClockwise && turn <= PolygonEpsilon) || (!counterClockwise && turn >= -PolygonEpsilon)) {
                continue;
            }

            bool containsOtherPoint = false;
            for (auto candidateIndex: indices) {
                if (candidateIndex == prevIndex || candidateIndex == currIndex || candidateIndex == nextIndex) {
                    continue;
                }
                if (pointInTriangle(points[candidateIndex], prev, curr, next)) {
                    containsOtherPoint = true;
                    break;
                }
            }
            if (containsOtherPoint) {
                continue;
            }

            if (counterClockwise) {
                triangles.push_back({prev, curr, next});
            } else {
                triangles.push_back({prev, next, curr});
            }
            indices.erase(indices.begin() + static_cast<std::ptrdiff_t>(i));
            clipped = true;
            break;
        }
        if (!clipped) {
            break;
        }
    }

    if (indices.size() == 3U) {
        const Point& a = points[indices[0]];
        const Point& b = points[indices[1]];
        const Point& c = points[indices[2]];
        if (counterClockwise) {
            triangles.push_back({a, b, c});
        } else {
            triangles.push_back({a, c, b});
        }
    }
    return triangles;
}

}  // namespace

auto GeometryRenderModelFactory::fromGeometryElement(const vn::geom::GeometryElement& geometry) -> GeometryRenderModel {
    GeometryRenderModel model;
    model.objectId = geometry.geometry().objectId();
    model.color = geometry.getColor();
    model.strokeWidth = geometry.getStrokeWidth();

    const auto& object = geometry.geometry();
    model.vertices.reserve(object.vertices().size());
    for (const auto& vertex: object.vertices()) {
        model.vertices.push_back(
                {.id = vertex.id, .position = Point(vertex.position.x, vertex.position.y)});
    }

    model.edges.reserve(object.edges().size());
    for (const auto& edge: object.edges()) {
        const auto* start = object.vertex(edge.start);
        const auto* end = object.vertex(edge.end);
        if (!start || !end) {
            continue;
        }

        GeometryEdgeRenderModel renderEdge;
        renderEdge.id = edge.id;
        renderEdge.kind = edge.kind;
        renderEdge.start = Point(start->position.x, start->position.y);
        renderEdge.end = Point(end->position.x, end->position.y);
        renderEdge.closedLoop = edge.start == edge.end;
        renderEdge.controls.reserve(edge.controls.size());
        for (const auto controlId: edge.controls) {
            const auto* control = object.vertex(controlId);
            if (!control) {
                continue;
            }
            renderEdge.controls.emplace_back(control->position.x, control->position.y);
        }

        model.edges.push_back(std::move(renderEdge));
    }

    model.faces.reserve(object.faces().size());
    for (const auto& face: object.faces()) {
        GeometryFaceRenderModel renderFace;
        renderFace.id = face.id;
        renderFace.fill = face.fill;
        renderFace.vertices.reserve(face.vertices.size());
        for (auto vertexId: face.vertices) {
            const auto* vertex = object.vertex(vertexId);
            if (!vertex) {
                renderFace.vertices.clear();
                break;
            }
            renderFace.vertices.emplace_back(vertex->position.x, vertex->position.y);
        }
        renderFace.triangles = triangulatePolygon(renderFace.vertices);
        if (!renderFace.triangles.empty()) {
            model.faces.push_back(std::move(renderFace));
        }
    }

    return model;
}

}  // namespace vn::view::render
