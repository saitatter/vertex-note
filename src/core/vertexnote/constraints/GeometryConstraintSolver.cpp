/*
 * VertexNote
 *
 * Lightweight editable constraint application for 2D geometry objects.
 */

#include "GeometryConstraintSolver.h"

#include <algorithm>
#include <cmath>
#include <iterator>
#include <optional>

namespace vn::constraints {

namespace {

constexpr double EPSILON = 1e-9;

auto almostEqual(double lhs, double rhs) -> bool { return std::abs(lhs - rhs) <= EPSILON; }

auto setPosition(geom::GeometryObject& object, geom::VertexId vertex, geom::Vec2 position) -> bool {
    const auto* current = object.vertex(vertex);
    if (!current || (almostEqual(current->position.x, position.x) && almostEqual(current->position.y, position.y))) {
        return false;
    }
    return object.setVertexPosition(vertex, position);
}

auto edgeDirection(const geom::GeometryObject& object, geom::EdgeId edgeId) -> std::optional<geom::Vec2> {
    const auto* edge = object.edge(edgeId);
    if (!edge) {
        return std::nullopt;
    }
    const auto* start = object.vertex(edge->start);
    const auto* end = object.vertex(edge->end);
    if (!start || !end) {
        return std::nullopt;
    }
    geom::Vec2 direction{end->position.x - start->position.x, end->position.y - start->position.y};
    const double length = std::hypot(direction.x, direction.y);
    if (length <= EPSILON) {
        return std::nullopt;
    }
    direction.x /= length;
    direction.y /= length;
    return direction;
}

auto edgeLength(const geom::GeometryObject& object, const geom::Edge& edge) -> double {
    const auto* start = object.vertex(edge.start);
    const auto* end = object.vertex(edge.end);
    if (!start || !end) {
        return 0.0;
    }
    return std::hypot(end->position.x - start->position.x, end->position.y - start->position.y);
}

auto alignEdgeToDirection(geom::GeometryObject& object, geom::EdgeId edgeId, geom::Vec2 direction) -> bool {
    const auto* edge = object.edge(edgeId);
    if (!edge) {
        return false;
    }
    const auto* start = object.vertex(edge->start);
    if (!start) {
        return false;
    }
    const double length = edgeLength(object, *edge);
    if (length <= EPSILON) {
        return false;
    }
    return setPosition(object, edge->end,
                       geom::Vec2{start->position.x + direction.x * length, start->position.y + direction.y * length});
}

auto normalizeDirection(geom::Vec2 direction) -> std::optional<geom::Vec2> {
    const double length = std::hypot(direction.x, direction.y);
    if (length <= EPSILON) {
        return std::nullopt;
    }
    return geom::Vec2{direction.x / length, direction.y / length};
}

}  // namespace

GeometryConstraintSolver::GeometryConstraintSolver(std::size_t maxIterations): maxIterations(std::max<std::size_t>(1U, maxIterations)) {}

auto GeometryConstraintSolver::apply(geom::GeometryObject& object) const -> ConstraintSolveResult {
    ConstraintSolveResult result;
    for (std::size_t iteration = 0; iteration < this->maxIterations; ++iteration) {
        auto pass = applyOnce(object);
        result.changed = result.changed || pass.changed;
        result.appliedConstraints += pass.appliedConstraints;
        result.iterations = iteration + 1U;
        if (!pass.changed) {
            break;
        }
    }
    return result;
}

auto GeometryConstraintSolver::applyOnce(geom::GeometryObject& object) const -> ConstraintSolveResult {
    ConstraintSolveResult result;
    for (const auto& constraint: object.constraints()) {
        bool changed = false;
        switch (constraint.kind) {
            case geom::ConstraintKind::Coincident:
                changed = applyCoincident(object, constraint);
                break;
            case geom::ConstraintKind::Horizontal:
                changed = applyHorizontal(object, constraint);
                break;
            case geom::ConstraintKind::Vertical:
                changed = applyVertical(object, constraint);
                break;
            case geom::ConstraintKind::Parallel:
                changed = applyParallel(object, constraint);
                break;
            case geom::ConstraintKind::Perpendicular:
                changed = applyPerpendicular(object, constraint);
                break;
            case geom::ConstraintKind::FixedLength:
                changed = applyFixedLength(object, constraint);
                break;
            case geom::ConstraintKind::Radius:
                changed = applyRadius(object, constraint);
                break;
            case geom::ConstraintKind::EqualLength:
            case geom::ConstraintKind::FixedAngle:
            case geom::ConstraintKind::OnEdge:
                break;
        }
        if (changed) {
            result.changed = true;
            result.appliedConstraints++;
        }
    }
    return result;
}

auto GeometryConstraintSolver::applyCoincident(geom::GeometryObject& object, const geom::Constraint& constraint) const
        -> bool {
    if (constraint.vertices.size() < 2U) {
        return false;
    }
    const auto* anchor = object.vertex(constraint.vertices.front());
    if (!anchor) {
        return false;
    }

    bool changed = false;
    for (auto it = std::next(constraint.vertices.begin()); it != constraint.vertices.end(); ++it) {
        changed = setPosition(object, *it, anchor->position) || changed;
    }
    return changed;
}

auto GeometryConstraintSolver::applyHorizontal(geom::GeometryObject& object, const geom::Constraint& constraint) const
        -> bool {
    if (constraint.vertices.size() < 2U) {
        return false;
    }
    const auto* anchor = object.vertex(constraint.vertices.front());
    const auto* target = object.vertex(constraint.vertices[1]);
    if (!anchor || !target) {
        return false;
    }
    return setPosition(object, target->id, geom::Vec2{target->position.x, anchor->position.y});
}

auto GeometryConstraintSolver::applyVertical(geom::GeometryObject& object, const geom::Constraint& constraint) const
        -> bool {
    if (constraint.vertices.size() < 2U) {
        return false;
    }
    const auto* anchor = object.vertex(constraint.vertices.front());
    const auto* target = object.vertex(constraint.vertices[1]);
    if (!anchor || !target) {
        return false;
    }
    return setPosition(object, target->id, geom::Vec2{anchor->position.x, target->position.y});
}

auto GeometryConstraintSolver::applyFixedLength(geom::GeometryObject& object, const geom::Constraint& constraint) const
        -> bool {
    if (constraint.vertices.size() < 2U || constraint.value <= EPSILON) {
        return false;
    }
    const auto* start = object.vertex(constraint.vertices.front());
    const auto* end = object.vertex(constraint.vertices[1]);
    if (!start || !end) {
        return false;
    }

    geom::Vec2 direction{end->position.x - start->position.x, end->position.y - start->position.y};
    double length = std::hypot(direction.x, direction.y);
    if (length <= EPSILON) {
        direction = {1.0, 0.0};
        length = 1.0;
    }

    return setPosition(object, end->id,
                       geom::Vec2{start->position.x + direction.x / length * constraint.value,
                                  start->position.y + direction.y / length * constraint.value});
}

auto GeometryConstraintSolver::applyRadius(geom::GeometryObject& object, const geom::Constraint& constraint) const
        -> bool {
    if (constraint.edges.empty() || constraint.value <= EPSILON) {
        return false;
    }

    const auto* edge = object.edge(constraint.edges.front());
    if (!edge || (edge->kind != geom::EdgeKind::Arc && edge->kind != geom::EdgeKind::ConstructionCircle) ||
        edge->controls.empty()) {
        return false;
    }

    const auto* center = object.vertex(edge->controls.front());
    const auto* start = object.vertex(edge->start);
    const auto* end = object.vertex(edge->end);
    if (!center || !start || !end) {
        return false;
    }

    bool changed = false;
    if (auto startDirection = normalizeDirection(
                geom::Vec2{start->position.x - center->position.x, start->position.y - center->position.y})) {
        changed = setPosition(object, start->id,
                              geom::Vec2{center->position.x + startDirection->x * constraint.value,
                                         center->position.y + startDirection->y * constraint.value}) ||
                  changed;
    }

    if (edge->end != edge->start) {
        if (auto endDirection = normalizeDirection(
                    geom::Vec2{end->position.x - center->position.x, end->position.y - center->position.y})) {
            changed = setPosition(object, end->id,
                                  geom::Vec2{center->position.x + endDirection->x * constraint.value,
                                             center->position.y + endDirection->y * constraint.value}) ||
                      changed;
        }
    }

    return changed;
}

auto GeometryConstraintSolver::applyParallel(geom::GeometryObject& object, const geom::Constraint& constraint) const
        -> bool {
    if (constraint.edges.size() < 2U) {
        return false;
    }
    const auto direction = edgeDirection(object, constraint.edges.front());
    if (!direction) {
        return false;
    }
    return alignEdgeToDirection(object, constraint.edges[1], *direction);
}

auto GeometryConstraintSolver::applyPerpendicular(geom::GeometryObject& object,
                                                  const geom::Constraint& constraint) const -> bool {
    if (constraint.edges.size() < 2U) {
        return false;
    }
    const auto direction = edgeDirection(object, constraint.edges.front());
    if (!direction) {
        return false;
    }
    return alignEdgeToDirection(object, constraint.edges[1], geom::Vec2{-direction->y, direction->x});
}

}  // namespace vn::constraints
