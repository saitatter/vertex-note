/*
 * VertexNote unit tests
 *
 * Snapping engine behavior.
 */

#include <algorithm>
#include <memory>
#include <utility>
#include <vector>

#include <config-test.h>
#include <gtest/gtest.h>

#include "vertexnote/geometry/GeometryObject.h"
#include "vertexnote/snapping/GeometrySnapProvider.h"
#include "vertexnote/snapping/GridSnapProvider.h"
#include "vertexnote/snapping/SnapEngine.h"

using vn::geom::GeometryObject;
using vn::geom::Vec2;
using vn::snap::GeometrySnapProvider;
using vn::snap::GridSnapProvider;
using vn::snap::SnapEngine;
using vn::snap::SnapKind;
using vn::snap::SnapQuery;

namespace {

struct FixedSnapCandidate {
    SnapKind kind = SnapKind::Grid;
    Vec2 pagePoint;
    double screenDistance = 0.0;
};

class FixedSnapProvider final: public vn::snap::ISnapProvider {
public:
    explicit FixedSnapProvider(std::vector<FixedSnapCandidate> fixedCandidates):
            fixedCandidates(std::move(fixedCandidates)) {}

    void query(const SnapQuery& query, std::vector<vn::snap::SnapCandidate>& candidates) const override {
        for (const auto& candidate: this->fixedCandidates) {
            candidates.push_back(vn::snap::SnapCandidate{.kind = candidate.kind,
                                                         .pagePoint = candidate.pagePoint,
                                                         .screenDistance = candidate.screenDistance,
                                                         .priority = query.priorities.priorityFor(candidate.kind)});
        }
    }

private:
    std::vector<FixedSnapCandidate> fixedCandidates;
};

}  // namespace

TEST(VertexNoteSnapEngine, returnsOriginalPointWithoutCandidates) {
    SnapEngine engine;

    auto result = engine.snap(SnapQuery{Vec2{1.0, 2.0}, 1.0, 8.0});

    EXPECT_FALSE(result.snapped());
    EXPECT_DOUBLE_EQ(result.pagePoint.x, 1.0);
    EXPECT_DOUBLE_EQ(result.pagePoint.y, 2.0);
}

TEST(VertexNoteSnapEngine, snapsToGridCandidateInsideTolerance) {
    SnapEngine engine;
    engine.addProvider(std::make_shared<GridSnapProvider>(10.0, 10.0, 0.5));

    auto result = engine.snap(SnapQuery{Vec2{9.0, 11.0}, 1.0, 8.0});

    ASSERT_TRUE(result.snapped());
    EXPECT_EQ(result.candidate->kind, SnapKind::Grid);
    EXPECT_DOUBLE_EQ(result.pagePoint.x, 10.0);
    EXPECT_DOUBLE_EQ(result.pagePoint.y, 10.0);
}

TEST(VertexNoteSnapEngine, prefersExplicitVertexOverGrid) {
    GeometryObject object(42);
    object.addVertex(Vec2{10.5, 10.5});

    SnapEngine engine;
    engine.addProvider(std::make_shared<GridSnapProvider>(10.0, 10.0, 0.5));
    engine.addProvider(std::make_shared<GeometrySnapProvider>(std::vector<const GeometryObject*>{&object}));

    auto result = engine.snap(SnapQuery{Vec2{10.4, 10.4}, 1.0, 8.0});

    ASSERT_TRUE(result.snapped());
    EXPECT_EQ(result.candidate->kind, SnapKind::ExplicitVertex);
    EXPECT_EQ(result.candidate->object, 42U);
    EXPECT_DOUBLE_EQ(result.pagePoint.x, 10.5);
    EXPECT_DOUBLE_EQ(result.pagePoint.y, 10.5);
}

TEST(VertexNoteSnapEngine, usesConfigurableSnapPriorities) {
    GeometryObject object(42);
    object.addVertex(Vec2{10.5, 10.5});

    SnapEngine engine;
    engine.addProvider(std::make_shared<GridSnapProvider>(10.0, 10.0, 0.5));
    engine.addProvider(std::make_shared<GeometrySnapProvider>(std::vector<const GeometryObject*>{&object}));

    SnapQuery query{Vec2{10.4, 10.4}, 1.0, 8.0};
    query.priorities.grid = 200.0;
    query.priorities.explicitVertex = 1.0;
    auto result = engine.snap(query);

    ASSERT_TRUE(result.snapped());
    EXPECT_EQ(result.candidate->kind, SnapKind::Grid);
}

TEST(VertexNoteSnapEngine, snapsToLineMidpoint) {
    GeometryObject object(42);
    const auto start = object.addVertex(Vec2{0.0, 0.0});
    const auto end = object.addVertex(Vec2{10.0, 0.0});
    const auto edge = object.addLine(start, end);

    SnapEngine engine;
    engine.addProvider(std::make_shared<GeometrySnapProvider>(std::vector<const GeometryObject*>{&object}));

    auto result = engine.snap(SnapQuery{Vec2{5.2, 0.2}, 1.0, 1.0});

    ASSERT_TRUE(result.snapped());
    EXPECT_EQ(result.candidate->kind, SnapKind::Midpoint);
    EXPECT_EQ(result.candidate->edge, edge);
    EXPECT_DOUBLE_EQ(result.pagePoint.x, 5.0);
    EXPECT_DOUBLE_EQ(result.pagePoint.y, 0.0);
}

TEST(VertexNoteSnapEngine, snapsToLineProjection) {
    GeometryObject object(42);
    const auto start = object.addVertex(Vec2{0.0, 0.0});
    const auto end = object.addVertex(Vec2{10.0, 0.0});
    const auto edge = object.addLine(start, end);

    SnapEngine engine;
    engine.addProvider(std::make_shared<GeometrySnapProvider>(std::vector<const GeometryObject*>{&object}));

    auto result = engine.snap(SnapQuery{Vec2{7.0, 0.2}, 1.0, 1.0});

    ASSERT_TRUE(result.snapped());
    EXPECT_EQ(result.candidate->kind, SnapKind::EdgeProjection);
    EXPECT_EQ(result.candidate->edge, edge);
    EXPECT_DOUBLE_EQ(result.pagePoint.x, 7.0);
    EXPECT_DOUBLE_EQ(result.pagePoint.y, 0.0);
}

TEST(VertexNoteSnapEngine, prefersCloseEdgeProjectionOverFarVertex) {
    SnapEngine engine;
    engine.addProvider(std::make_shared<FixedSnapProvider>(std::vector<FixedSnapCandidate>{
            {.kind = SnapKind::ExplicitVertex, .pagePoint = Vec2{20.0, 0.0}, .screenDistance = 16.0},
            {.kind = SnapKind::EdgeProjection, .pagePoint = Vec2{16.0, 0.0}, .screenDistance = 0.2},
    }));

    auto result = engine.snap(SnapQuery{Vec2{16.0, 0.2}, 1.0, 20.0});

    ASSERT_TRUE(result.snapped());
    EXPECT_EQ(result.candidate->kind, SnapKind::EdgeProjection);
    EXPECT_DOUBLE_EQ(result.pagePoint.x, 16.0);
    EXPECT_DOUBLE_EQ(result.pagePoint.y, 0.0);
}

TEST(VertexNoteSnapEngine, keepsNearbyVertexAboveEdgeProjection) {
    GeometryObject object(42);
    const auto start = object.addVertex(Vec2{0.0, 0.0});
    const auto end = object.addVertex(Vec2{40.0, 0.0});
    object.addLine(start, end);

    SnapEngine engine;
    engine.addProvider(std::make_shared<GeometrySnapProvider>(std::vector<const GeometryObject*>{&object}));

    auto result = engine.snap(SnapQuery{Vec2{5.0, 0.2}, 1.0, 20.0});

    ASSERT_TRUE(result.snapped());
    EXPECT_EQ(result.candidate->kind, SnapKind::ExplicitVertex);
    EXPECT_EQ(result.candidate->vertex, start);
    EXPECT_DOUBLE_EQ(result.pagePoint.x, 0.0);
    EXPECT_DOUBLE_EQ(result.pagePoint.y, 0.0);
}

TEST(VertexNoteSnapEngine, snapsToLineIntersection) {
    GeometryObject first(42);
    const auto a = first.addVertex(Vec2{0.0, 0.0});
    const auto b = first.addVertex(Vec2{10.0, 10.0});
    first.addLine(a, b);

    GeometryObject second(43);
    const auto c = second.addVertex(Vec2{0.0, 10.0});
    const auto d = second.addVertex(Vec2{10.0, 0.0});
    second.addLine(c, d);

    SnapEngine engine;
    engine.addProvider(
            std::make_shared<GeometrySnapProvider>(std::vector<const GeometryObject*>{&first, &second}));

    auto result = engine.snap(SnapQuery{Vec2{5.2, 5.1}, 1.0, 1.0});

    ASSERT_TRUE(result.snapped());
    EXPECT_EQ(result.candidate->kind, SnapKind::Intersection);
    EXPECT_DOUBLE_EQ(result.pagePoint.x, 5.0);
    EXPECT_DOUBLE_EQ(result.pagePoint.y, 5.0);
}

TEST(VertexNoteSnapEngine, ignoresIntersectionsOutsideSnapWindow) {
    GeometryObject first(42);
    const auto a = first.addVertex(Vec2{40.0, 40.0});
    const auto b = first.addVertex(Vec2{60.0, 60.0});
    first.addLine(a, b);

    GeometryObject second(43);
    const auto c = second.addVertex(Vec2{40.0, 60.0});
    const auto d = second.addVertex(Vec2{60.0, 40.0});
    second.addLine(c, d);

    GeometrySnapProvider provider(std::vector<const GeometryObject*>{&first, &second});
    std::vector<vn::snap::SnapCandidate> candidates;
    provider.query(SnapQuery{Vec2{0.0, 0.0}, 1.0, 1.0}, candidates);

    EXPECT_TRUE(std::none_of(candidates.begin(), candidates.end(), [](const auto& candidate) {
        return candidate.kind == SnapKind::Intersection;
    }));
}
