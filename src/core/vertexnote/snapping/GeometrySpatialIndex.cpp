/*
 * VertexNote
 *
 * Lightweight spatial index for geometry snapping.
 */

#include "GeometrySpatialIndex.h"

#include <algorithm>
#include <cmath>
#include <functional>
#include <iterator>
#include <unordered_map>
#include <unordered_set>

namespace vn::snap {

namespace {
constexpr double DEFAULT_CELL_SIZE = 64.0;
constexpr double BOUNDS_EPSILON = 0.000001;

[[nodiscard]] auto normalizedCellSize(double cellSize) -> double {
    return cellSize > BOUNDS_EPSILON ? cellSize : DEFAULT_CELL_SIZE;
}

[[nodiscard]] auto normalizedPair(std::size_t lhs, std::size_t rhs) -> std::pair<std::size_t, std::size_t> {
    return lhs < rhs ? std::make_pair(lhs, rhs) : std::make_pair(rhs, lhs);
}

[[nodiscard]] constexpr auto hashCombineMagic() -> std::size_t {
    return sizeof(std::size_t) >= 8 ? std::size_t{0x9e3779b97f4a7c15ULL} : std::size_t{0x9e3779b9U};
}

[[nodiscard]] auto combineHash(std::size_t seed, std::size_t value) -> std::size_t {
    return seed ^ (value + hashCombineMagic() + (seed << 6U) + (seed >> 2U));
}
}  // namespace

GeometrySpatialIndex::GeometrySpatialIndex(double cellSize): cellSize(normalizedCellSize(cellSize)) {}

void GeometrySpatialIndex::clear() {
    this->pointCells.clear();
    this->segmentCells.clear();
}

void GeometrySpatialIndex::rebuild(std::span<const IndexedSegment> segments) { this->rebuildSegments(segments); }

void GeometrySpatialIndex::rebuildSegments(std::span<const IndexedSegment> segments) {
    this->segmentCells.clear();

    for (std::size_t index = 0; index < segments.size(); ++index) {
        for (const auto& cell: this->cellsFor(segmentBounds(segments[index]))) {
            this->segmentCells[cell].push_back(index);
        }
    }
}

void GeometrySpatialIndex::rebuildPoints(std::span<const IndexedPoint> points) {
    this->pointCells.clear();

    for (std::size_t index = 0; index < points.size(); ++index) {
        const auto& position = points[index].position;
        this->pointCells[CellKey{this->cellFor(position.x), this->cellFor(position.y)}].push_back(index);
    }
}

auto GeometrySpatialIndex::queryPointIndices(const SpatialBounds& bounds) const -> std::vector<std::size_t> {
    std::vector<std::size_t> result;

    for (const auto& cell: this->cellsFor(bounds)) {
        const auto it = this->pointCells.find(cell);
        if (it == this->pointCells.end()) {
            continue;
        }

        for (const auto pointIndex: it->second) {
            result.push_back(pointIndex);
        }
    }

    return result;
}

auto GeometrySpatialIndex::querySegmentIndices(const SpatialBounds& bounds) const -> std::vector<std::size_t> {
    std::vector<std::size_t> result;
    std::unordered_set<std::size_t> seen;

    for (const auto& cell: this->cellsFor(bounds)) {
        const auto it = this->segmentCells.find(cell);
        if (it == this->segmentCells.end()) {
            continue;
        }

        for (const auto segmentIndex: it->second) {
            if (seen.insert(segmentIndex).second) {
                result.push_back(segmentIndex);
            }
        }
    }

    return result;
}

auto GeometrySpatialIndex::querySegmentPairs(const SpatialBounds& bounds) const
        -> std::vector<std::pair<std::size_t, std::size_t>> {
    std::vector<std::pair<std::size_t, std::size_t>> result;
    std::unordered_set<std::pair<std::size_t, std::size_t>, SegmentPairHash> seenPairs;

    for (const auto& cell: this->cellsFor(bounds)) {
        const auto it = this->segmentCells.find(cell);
        if (it == this->segmentCells.end()) {
            continue;
        }

        const auto& segmentIndices = it->second;
        for (auto lhs = segmentIndices.begin(); lhs != segmentIndices.end(); ++lhs) {
            for (auto rhs = std::next(lhs); rhs != segmentIndices.end(); ++rhs) {
                const auto pair = normalizedPair(*lhs, *rhs);
                if (seenPairs.insert(pair).second) {
                    result.push_back(pair);
                }
            }
        }
    }

    return result;
}

auto GeometrySpatialIndex::CellKeyHash::operator()(const CellKey& key) const -> std::size_t {
    auto hash = std::hash<int>{}(key.x);
    return combineHash(hash, std::hash<int>{}(key.y));
}

auto GeometrySpatialIndex::SegmentPairHash::operator()(const std::pair<std::size_t, std::size_t>& pair) const
        -> std::size_t {
    return combineHash(pair.first, pair.second);
}

auto GeometrySpatialIndex::cellFor(double coordinate) const -> int {
    return static_cast<int>(std::floor(coordinate / this->cellSize));
}

auto GeometrySpatialIndex::cellsFor(const SpatialBounds& bounds) const -> std::vector<CellKey> {
    const int minCellX = this->cellFor(std::min(bounds.minX, bounds.maxX));
    const int maxCellX = this->cellFor(std::max(bounds.minX, bounds.maxX));
    const int minCellY = this->cellFor(std::min(bounds.minY, bounds.maxY));
    const int maxCellY = this->cellFor(std::max(bounds.minY, bounds.maxY));

    std::vector<CellKey> result;
    result.reserve(static_cast<std::size_t>(maxCellX - minCellX + 1) *
                   static_cast<std::size_t>(maxCellY - minCellY + 1));
    for (int y = minCellY; y <= maxCellY; ++y) {
        for (int x = minCellX; x <= maxCellX; ++x) {
            result.push_back(CellKey{x, y});
        }
    }
    return result;
}

auto pointBounds(const IndexedPoint& point) -> SpatialBounds {
    return SpatialBounds{point.position.x, point.position.y, point.position.x, point.position.y};
}

auto segmentBounds(const IndexedSegment& segment) -> SpatialBounds {
    return SpatialBounds{std::min(segment.start.x, segment.end.x), std::min(segment.start.y, segment.end.y),
                         std::max(segment.start.x, segment.end.x), std::max(segment.start.y, segment.end.y)};
}

auto overlaps(const SpatialBounds& lhs, const SpatialBounds& rhs) -> bool {
    return lhs.maxX + BOUNDS_EPSILON >= rhs.minX && lhs.minX - BOUNDS_EPSILON <= rhs.maxX &&
           lhs.maxY + BOUNDS_EPSILON >= rhs.minY && lhs.minY - BOUNDS_EPSILON <= rhs.maxY;
}

}  // namespace vn::snap
