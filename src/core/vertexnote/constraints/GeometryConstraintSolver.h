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
};

class GeometryConstraintSolver {
public:
    [[nodiscard]] auto apply(geom::GeometryObject& object) const -> ConstraintSolveResult;

private:
    [[nodiscard]] auto applyCoincident(geom::GeometryObject& object, const geom::Constraint& constraint) const -> bool;
    [[nodiscard]] auto applyHorizontal(geom::GeometryObject& object, const geom::Constraint& constraint) const -> bool;
    [[nodiscard]] auto applyVertical(geom::GeometryObject& object, const geom::Constraint& constraint) const -> bool;
    [[nodiscard]] auto applyFixedLength(geom::GeometryObject& object, const geom::Constraint& constraint) const
            -> bool;
    [[nodiscard]] auto applyParallel(geom::GeometryObject& object, const geom::Constraint& constraint) const -> bool;
    [[nodiscard]] auto applyPerpendicular(geom::GeometryObject& object, const geom::Constraint& constraint) const
            -> bool;
};

}  // namespace vn::constraints
