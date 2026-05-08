/*
 * VertexNote
 *
 * Constraint metadata used by editing UI and command surfaces.
 */

#pragma once

#include <cstddef>
#include <span>
#include <string_view>

#include "vertexnote/geometry/GeometryTypes.h"

namespace vn::constraints {

struct ConstraintDescriptor {
    geom::ConstraintKind kind = geom::ConstraintKind::Coincident;
    std::string_view stableId;
    std::string_view displayName;
    std::size_t minVertices = 0;
    std::size_t minEdges = 0;
    bool requiresValue = false;
    bool solverSupported = false;
};

[[nodiscard]] auto constraintDescriptors() -> std::span<const ConstraintDescriptor>;
[[nodiscard]] auto descriptorFor(geom::ConstraintKind kind) -> const ConstraintDescriptor*;
[[nodiscard]] auto stableIdFor(geom::ConstraintKind kind) -> std::string_view;
[[nodiscard]] auto displayNameFor(geom::ConstraintKind kind) -> std::string_view;
[[nodiscard]] auto isSolverSupported(geom::ConstraintKind kind) -> bool;

}  // namespace vn::constraints
