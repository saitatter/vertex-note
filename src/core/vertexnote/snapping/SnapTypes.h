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

struct SnapQuery {
    geom::Vec2 pagePoint;
    double zoom = 1.0;
    double maxScreenDistance = 8.0;
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
