/*
 * VertexNote
 *
 * Core geometry value types.
 */

#pragma once

#include <cstdint>
#include <vector>

namespace vn::geom {

using ObjectId = std::uint64_t;
using VertexId = std::uint64_t;
using EdgeId = std::uint64_t;
using ConstraintId = std::uint64_t;

constexpr ObjectId InvalidObjectId = 0;
constexpr VertexId InvalidVertexId = 0;
constexpr EdgeId InvalidEdgeId = 0;
constexpr ConstraintId InvalidConstraintId = 0;

struct Vec2 {
    double x = 0.0;
    double y = 0.0;

    [[nodiscard]] bool operator==(const Vec2&) const = default;
};

struct Vec3 {
    double x = 0.0;
    double y = 0.0;
    double z = 0.0;

    [[nodiscard]] bool operator==(const Vec3&) const = default;
};

enum class VertexFlags : std::uint32_t {
    None = 0,
    Explicit = 1U << 0U,
    Derived = 1U << 1U,
    Locked = 1U << 2U,
    Hidden = 1U << 3U,
};

[[nodiscard]] constexpr auto operator|(VertexFlags lhs, VertexFlags rhs) -> VertexFlags {
    return static_cast<VertexFlags>(static_cast<std::uint32_t>(lhs) | static_cast<std::uint32_t>(rhs));
}

[[nodiscard]] constexpr auto hasFlag(VertexFlags flags, VertexFlags flag) -> bool {
    return (static_cast<std::uint32_t>(flags) & static_cast<std::uint32_t>(flag)) != 0U;
}

struct Vertex {
    VertexId id = InvalidVertexId;
    Vec2 position;
    ObjectId owner = InvalidObjectId;
    VertexFlags flags = VertexFlags::Explicit;
};

enum class EdgeKind {
    Line,
    Arc,
    CubicBezier,
    ConstructionLine,
};

struct Edge {
    EdgeId id = InvalidEdgeId;
    EdgeKind kind = EdgeKind::Line;
    VertexId start = InvalidVertexId;
    VertexId end = InvalidVertexId;
    std::vector<VertexId> controls;
};

struct Bounds {
    double minX = 0.0;
    double minY = 0.0;
    double maxX = 0.0;
    double maxY = 0.0;
};

}  // namespace vn::geom
