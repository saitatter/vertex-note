/*
 * VertexNote
 *
 * Lightweight spatial index for geometry snapping.
 */

#pragma once

#include <cstddef>
#include <span>
#include <unordered_map>
#include <utility>
#include <vector>

#include "vertexnote/geometry/GeometryTypes.h"

namespace vn::snap {

struct SpatialBounds {
    double minX = 0.0;
    double minY = 0.0;
    double maxX = 0.0;
    double maxY = 0.0;
};

struct IndexedSegment {
    geom::ObjectId object = geom::InvalidObjectId;
    geom::EdgeId edge = geom::InvalidEdgeId;
    geom::Vec2 start;
    geom::Vec2 end;
};

class GeometrySpatialIndex {
public:
    explicit GeometrySpatialIndex(double cellSize = 64.0);

    void clear();
    void rebuild(std::span<const IndexedSegment> segments);

    [[nodiscard]] auto querySegmentIndices(const SpatialBounds& bounds) const -> std::vector<std::size_t>;
    [[nodiscard]] auto querySegmentPairs(const SpatialBounds& bounds) const
            -> std::vector<std::pair<std::size_t, std::size_t>>;

private:
    struct CellKey {
        int x = 0;
        int y = 0;

        [[nodiscard]] auto operator==(const CellKey&) const -> bool = default;
    };

    struct CellKeyHash {
        [[nodiscard]] auto operator()(const CellKey& key) const -> std::size_t;
    };

    struct SegmentPairHash {
        [[nodiscard]] auto operator()(const std::pair<std::size_t, std::size_t>& pair) const -> std::size_t;
    };

private:
    [[nodiscard]] auto cellFor(double coordinate) const -> int;
    [[nodiscard]] auto cellsFor(const SpatialBounds& bounds) const -> std::vector<CellKey>;

private:
    double cellSize = 64.0;
    std::unordered_map<CellKey, std::vector<std::size_t>, CellKeyHash> cells;
};

[[nodiscard]] auto segmentBounds(const IndexedSegment& segment) -> SpatialBounds;
[[nodiscard]] auto overlaps(const SpatialBounds& lhs, const SpatialBounds& rhs) -> bool;

}  // namespace vn::snap
