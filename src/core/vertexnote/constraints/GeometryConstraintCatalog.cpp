/*
 * VertexNote
 *
 * Constraint metadata used by editing UI and command surfaces.
 */

#include "GeometryConstraintCatalog.h"

#include <array>
#include <ranges>

namespace vn::constraints {

namespace {

using geom::ConstraintKind;

constexpr std::array<ConstraintDescriptor, 10> Descriptors{{
        {ConstraintKind::Coincident, "coincident", "Coincident", 2, 0, false, true},
        {ConstraintKind::Horizontal, "horizontal", "Horizontal", 2, 0, false, true},
        {ConstraintKind::Vertical, "vertical", "Vertical", 2, 0, false, true},
        {ConstraintKind::Parallel, "parallel", "Parallel", 0, 2, false, true},
        {ConstraintKind::Perpendicular, "perpendicular", "Perpendicular", 0, 2, false, true},
        {ConstraintKind::EqualLength, "equal-length", "Equal length", 0, 2, false, false},
        {ConstraintKind::FixedLength, "fixed-length", "Fixed length", 2, 0, true, true},
        {ConstraintKind::FixedAngle, "fixed-angle", "Fixed angle", 0, 1, true, false},
        {ConstraintKind::Radius, "radius", "Radius", 0, 1, true, true},
        {ConstraintKind::OnEdge, "on-edge", "On edge", 1, 1, false, false},
}};

}  // namespace

auto constraintDescriptors() -> std::span<const ConstraintDescriptor> { return Descriptors; }

auto descriptorFor(geom::ConstraintKind kind) -> const ConstraintDescriptor* {
    const auto it = std::ranges::find(Descriptors, kind, &ConstraintDescriptor::kind);
    return it == Descriptors.end() ? nullptr : &*it;
}

auto stableIdFor(geom::ConstraintKind kind) -> std::string_view {
    const auto* descriptor = descriptorFor(kind);
    return descriptor ? descriptor->stableId : std::string_view{};
}

auto displayNameFor(geom::ConstraintKind kind) -> std::string_view {
    const auto* descriptor = descriptorFor(kind);
    return descriptor ? descriptor->displayName : std::string_view{};
}

auto isSolverSupported(geom::ConstraintKind kind) -> bool {
    const auto* descriptor = descriptorFor(kind);
    return descriptor && descriptor->solverSupported;
}

}  // namespace vn::constraints
