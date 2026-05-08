/*
 * VertexNote unit tests
 *
 * Spatial index behavior for geometry snapping.
 */

#include <vector>

#include <config-test.h>
#include <gtest/gtest.h>

#include "vertexnote/snapping/GeometrySpatialIndex.h"

using vn::geom::Vec2;
using vn::snap::GeometrySpatialIndex;
using vn::snap::IndexedPoint;
using vn::snap::IndexedSegment;
using vn::snap::SpatialBounds;

TEST(VertexNoteGeometrySpatialIndex, queriesOnlyPointsInNearbyCells) {
    const std::vector<IndexedPoint> points{
            IndexedPoint{1, 1, Vec2{4.0, 4.0}},
            IndexedPoint{2, 2, Vec2{200.0, 200.0}},
    };

    GeometrySpatialIndex index(32.0);
    index.rebuildPoints(points);

    const auto result = index.queryPointIndices(SpatialBounds{0.0, 0.0, 8.0, 8.0});

    ASSERT_EQ(result.size(), 1U);
    EXPECT_EQ(result.front(), 0U);
}

TEST(VertexNoteGeometrySpatialIndex, queriesOnlySegmentsInNearbyCells) {
    const std::vector<IndexedSegment> segments{
            IndexedSegment{1, 1, Vec2{0.0, 0.0}, Vec2{10.0, 0.0}},
            IndexedSegment{2, 2, Vec2{200.0, 200.0}, Vec2{210.0, 200.0}},
    };

    GeometrySpatialIndex index(32.0);
    index.rebuild(segments);

    const auto result = index.querySegmentIndices(SpatialBounds{-5.0, -5.0, 15.0, 5.0});

    ASSERT_EQ(result.size(), 1U);
    EXPECT_EQ(result.front(), 0U);
}

TEST(VertexNoteGeometrySpatialIndex, deduplicatesPairsAcrossSharedCells) {
    const std::vector<IndexedSegment> segments{
            IndexedSegment{1, 1, Vec2{0.0, 0.0}, Vec2{96.0, 96.0}},
            IndexedSegment{2, 2, Vec2{0.0, 96.0}, Vec2{96.0, 0.0}},
    };

    GeometrySpatialIndex index(32.0);
    index.rebuild(segments);

    const auto pairs = index.querySegmentPairs(SpatialBounds{40.0, 40.0, 56.0, 56.0});

    ASSERT_EQ(pairs.size(), 1U);
    EXPECT_EQ(pairs.front().first, 0U);
    EXPECT_EQ(pairs.front().second, 1U);
}

TEST(VertexNoteGeometrySpatialIndex, returnsSegmentsCrossingNegativeCoordinates) {
    const std::vector<IndexedSegment> segments{
            IndexedSegment{1, 1, Vec2{-20.0, -20.0}, Vec2{-5.0, -5.0}},
            IndexedSegment{2, 2, Vec2{20.0, 20.0}, Vec2{30.0, 30.0}},
    };

    GeometrySpatialIndex index(16.0);
    index.rebuild(segments);

    const auto result = index.querySegmentIndices(SpatialBounds{-18.0, -18.0, -10.0, -10.0});

    ASSERT_EQ(result.size(), 1U);
    EXPECT_EQ(result.front(), 0U);
}
