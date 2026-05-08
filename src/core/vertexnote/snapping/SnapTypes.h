/*
 * VertexNote
 *
 * Shared snapping data structures.
 */

#pragma once

#include <optional>

#include "vertexnote/geometry/GeometryTypes.h"

namespace vn::snap {

enum class SnapKind {
    Grid,
    ExplicitVertex,
    EdgeEndpoint,
    Midpoint,
    EdgeProjection,
    Intersection,
    ConstraintGuide,
};

struct SnapPriorities {
    double grid = 10.0;
    double explicitVertex = 100.0;
    double edgeEndpoint = 100.0;
    double midpoint = 70.0;
    double edgeProjection = 50.0;
    double intersection = 90.0;
    double constraintGuide = 80.0;

    [[nodiscard]] auto priorityFor(SnapKind kind) const -> double {
        switch (kind) {
            case SnapKind::Grid:
                return grid;
            case SnapKind::ExplicitVertex:
                return explicitVertex;
            case SnapKind::EdgeEndpoint:
                return edgeEndpoint;
            case SnapKind::Midpoint:
                return midpoint;
            case SnapKind::EdgeProjection:
                return edgeProjection;
            case SnapKind::Intersection:
                return intersection;
            case SnapKind::ConstraintGuide:
                return constraintGuide;
        }
        return 0.0;
    }
};

struct SnapQuery {
    geom::Vec2 pagePoint;
    double zoom = 1.0;
    double maxScreenDistance = 8.0;
    SnapPriorities priorities;
};

struct SnapCandidate {
    SnapKind kind = SnapKind::Grid;
    geom::Vec2 pagePoint;
    double screenDistance = 0.0;
    double priority = 0.0;
    geom::ObjectId object = geom::InvalidObjectId;
    geom::VertexId vertex = geom::InvalidVertexId;
    geom::EdgeId edge = geom::InvalidEdgeId;
};

struct SnapResult {
    geom::Vec2 originalPoint;
    geom::Vec2 pagePoint;
    std::optional<SnapCandidate> candidate;

    [[nodiscard]] auto snapped() const -> bool { return this->candidate.has_value(); }
};

}  // namespace vn::snap
