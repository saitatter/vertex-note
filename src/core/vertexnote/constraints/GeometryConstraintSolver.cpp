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
constexpr double TAU = 6.28318530717958647692;

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

auto normalizeAngle(double angle) -> double {
    angle = std::fmod(angle, TAU);
    if (angle < 0.0) {
        angle += TAU;
    }
    return angle;
}

auto angleWithinSweep(double angle, double startAngle, double endAngle) -> bool {
    angle = normalizeAngle(angle);
    startAngle = normalizeAngle(startAngle);
    endAngle = normalizeAngle(endAngle);
    if (endAngle <= startAngle) {
        endAngle += TAU;
    }
    if (angle < startAngle) {
        angle += TAU;
    }
    return angle >= startAngle - EPSILON && angle <= endAngle + EPSILON;
}

auto projectPointToLine(geom::Vec2 point, geom::Vec2 start, geom::Vec2 end, bool clampToSegment)
        -> std::optional<geom::Vec2> {
    const double dx = end.x - start.x;
    const double dy = end.y - start.y;
    const double lengthSquared = dx * dx + dy * dy;
    if (lengthSquared <= EPSILON) {
        return std::nullopt;
    }

    double t = ((point.x - start.x) * dx + (point.y - start.y) * dy) / lengthSquared;
    if (clampToSegment) {
        t = std::clamp(t, 0.0, 1.0);
    }
    return geom::Vec2{start.x + dx * t, start.y + dy * t};
}

auto projectPointToCircularEdge(const geom::GeometryObject& object, const geom::Edge& edge, geom::Vec2 point)
        -> std::optional<geom::Vec2> {
    if (edge.controls.empty()) {
        return std::nullopt;
    }
    const auto* center = object.vertex(edge.controls.front());
    const auto* start = object.vertex(edge.start);
    const auto* end = object.vertex(edge.end);
    if (!center || !start || !end) {
        return std::nullopt;
    }

    const double radius = std::hypot(start->position.x - center->position.x, start->position.y - center->position.y);
    if (radius <= EPSILON) {
        return std::nullopt;
    }

    geom::Vec2 direction{point.x - center->position.x, point.y - center->position.y};
    auto normalized = normalizeDirection(direction);
    if (!normalized) {
        normalized = normalizeDirection(
                geom::Vec2{start->position.x - center->position.x, start->position.y - center->position.y});
    }
    if (!normalized) {
        return std::nullopt;
    }

    const double angle = std::atan2(normalized->y, normalized->x);
    if (edge.kind == geom::EdgeKind::Arc && edge.start != edge.end) {
        const double startAngle =
                std::atan2(start->position.y - center->position.y, start->position.x - center->position.x);
        const double endAngle = std::atan2(end->position.y - center->position.y, end->position.x - center->position.x);
        if (!angleWithinSweep(angle, startAngle, endAngle)) {
            const double startDistance = std::hypot(point.x - start->position.x, point.y - start->position.y);
            const double endDistance = std::hypot(point.x - end->position.x, point.y - end->position.y);
            return startDistance <= endDistance ? start->position : end->position;
        }
    }

    return geom::Vec2{center->position.x + normalized->x * radius, center->position.y + normalized->y * radius};
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
                changed = applyEqualLength(object, constraint);
                break;
            case geom::ConstraintKind::FixedAngle:
                changed = applyFixedAngle(object, constraint);
                break;
            case geom::ConstraintKind::OnEdge:
                changed = applyOnEdge(object, constraint);
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

auto GeometryConstraintSolver::applyEqualLength(geom::GeometryObject& object,
                                                const geom::Constraint& constraint) const -> bool {
    if (constraint.edges.size() < 2U) {
        return false;
    }

    const auto* referenceEdge = object.edge(constraint.edges.front());
    const auto* targetEdge = object.edge(constraint.edges[1]);
    if (!referenceEdge || !targetEdge) {
        return false;
    }

    const double referenceLength = edgeLength(object, *referenceEdge);
    if (referenceLength <= EPSILON) {
        return false;
    }

    const auto* targetStart = object.vertex(targetEdge->start);
    const auto* targetEnd = object.vertex(targetEdge->end);
    if (!targetStart || !targetEnd) {
        return false;
    }

    auto targetDirection = normalizeDirection(geom::Vec2{targetEnd->position.x - targetStart->position.x,
                                                         targetEnd->position.y - targetStart->position.y});
    if (!targetDirection) {
        targetDirection = edgeDirection(object, referenceEdge->id);
    }
    if (!targetDirection) {
        targetDirection = geom::Vec2{1.0, 0.0};
    }

    return setPosition(object, targetEnd->id,
                       geom::Vec2{targetStart->position.x + targetDirection->x * referenceLength,
                                  targetStart->position.y + targetDirection->y * referenceLength});
}

auto GeometryConstraintSolver::applyFixedAngle(geom::GeometryObject& object,
                                               const geom::Constraint& constraint) const -> bool {
    if (constraint.edges.empty()) {
        return false;
    }

    const auto* edge = object.edge(constraint.edges.front());
    if (!edge) {
        return false;
    }
    const auto* start = object.vertex(edge->start);
    const auto* end = object.vertex(edge->end);
    if (!start || !end) {
        return false;
    }

    const double length = edgeLength(object, *edge);
    if (length <= EPSILON) {
        return false;
    }

    return setPosition(object, end->id,
                       geom::Vec2{start->position.x + std::cos(constraint.value) * length,
                                  start->position.y + std::sin(constraint.value) * length});
}

auto GeometryConstraintSolver::applyOnEdge(geom::GeometryObject& object, const geom::Constraint& constraint) const
        -> bool {
    if (constraint.vertices.empty() || constraint.edges.empty()) {
        return false;
    }

    const auto* target = object.vertex(constraint.vertices.front());
    const auto* edge = object.edge(constraint.edges.front());
    if (!target || !edge) {
        return false;
    }

    std::optional<geom::Vec2> projected;
    switch (edge->kind) {
        case geom::EdgeKind::Line:
        case geom::EdgeKind::ConstructionLine: {
            const auto* start = object.vertex(edge->start);
            const auto* end = object.vertex(edge->end);
            if (!start || !end) {
                return false;
            }
            projected = projectPointToLine(target->position, start->position, end->position,
                                           edge->kind == geom::EdgeKind::Line);
            break;
        }
        case geom::EdgeKind::Arc:
        case geom::EdgeKind::ConstructionCircle:
            projected = projectPointToCircularEdge(object, *edge, target->position);
            break;
        case geom::EdgeKind::CubicBezier:
            break;
    }

    return projected ? setPosition(object, target->id, *projected) : false;
}

}  // namespace vn::constraints
