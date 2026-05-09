/*
 * VertexNote
 *
 * Lightweight editable constraint application for 2D geometry objects.
 */

#pragma once

#include "vertexnote/geometry/GeometryObject.h"

namespace vn::constraints {

struct ConstraintSolveResult {
    bool changed = false;
    std::size_t appliedConstraints = 0;
    std::size_t iterations = 0;
};

class GeometryConstraintSolver {
public:
    explicit GeometryConstraintSolver(std::size_t maxIterations = 8U);

    [[nodiscard]] auto apply(geom::GeometryObject& object) const -> ConstraintSolveResult;

private:
    [[nodiscard]] auto applyOnce(geom::GeometryObject& object) const -> ConstraintSolveResult;

    [[nodiscard]] auto applyCoincident(geom::GeometryObject& object, const geom::Constraint& constraint) const -> bool;
    [[nodiscard]] auto applyHorizontal(geom::GeometryObject& object, const geom::Constraint& constraint) const -> bool;
    [[nodiscard]] auto applyVertical(geom::GeometryObject& object, const geom::Constraint& constraint) const -> bool;
    [[nodiscard]] auto applyFixedLength(geom::GeometryObject& object, const geom::Constraint& constraint) const
            -> bool;
    [[nodiscard]] auto applyRadius(geom::GeometryObject& object, const geom::Constraint& constraint) const -> bool;
    [[nodiscard]] auto applyParallel(geom::GeometryObject& object, const geom::Constraint& constraint) const -> bool;
    [[nodiscard]] auto applyPerpendicular(geom::GeometryObject& object, const geom::Constraint& constraint) const
            -> bool;

private:
    std::size_t maxIterations = 8U;
};

}  // namespace vn::constraints
