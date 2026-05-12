/*
 * VertexNote unit tests
 */

#include <config-test.h>
#include <gtest/gtest.h>

#include "util/Color.h"
#include "vertexnote/geometry/GeometryElement.h"
#include "view/render/GeometryRenderModelFactory.h"

namespace {

auto makeGeometryElement() -> vn::geom::GeometryElement {
    vn::geom::GeometryObject object(77);
    const auto start = object.addVertex(vn::geom::Vec2{10.0, 20.0});
    const auto end = object.addVertex(vn::geom::Vec2{30.0, 40.0});
    const auto center = object.addVertex(vn::geom::Vec2{25.0, 25.0});
    object.addEdge(vn::geom::EdgeKind::ConstructionLine, start, end);
    object.addEdge(vn::geom::EdgeKind::ConstructionCircle, end, end, {center});

    vn::geom::GeometryElement element(std::move(object));
    element.setColor(Colors::xopp_royalblue);
    element.setStrokeWidth(3.5);
    return element;
}

}  // namespace

TEST(VertexNoteGeometryRenderModelFactory, preservesGeometryAppearanceAndTopology) {
    const auto element = makeGeometryElement();

    const auto model = vn::view::render::GeometryRenderModelFactory::fromGeometryElement(element);

    EXPECT_EQ(model.objectId, 77U);
    ASSERT_EQ(model.vertices.size(), 3U);
    ASSERT_EQ(model.edges.size(), 2U);
    EXPECT_EQ(model.color, Colors::xopp_royalblue);
    EXPECT_DOUBLE_EQ(model.strokeWidth, 3.5);

    EXPECT_EQ(model.vertices[0].id, 1U);
    EXPECT_DOUBLE_EQ(model.vertices[0].position.x, 10.0);
    EXPECT_DOUBLE_EQ(model.vertices[0].position.y, 20.0);

    EXPECT_EQ(model.edges[0].id, 1U);
    EXPECT_EQ(model.edges[0].kind, vn::geom::EdgeKind::ConstructionLine);
    EXPECT_FALSE(model.edges[0].closedLoop);
    EXPECT_DOUBLE_EQ(model.edges[0].start.x, 10.0);
    EXPECT_DOUBLE_EQ(model.edges[0].start.y, 20.0);
    EXPECT_DOUBLE_EQ(model.edges[0].end.x, 30.0);
    EXPECT_DOUBLE_EQ(model.edges[0].end.y, 40.0);

    EXPECT_EQ(model.edges[1].id, 2U);
    EXPECT_EQ(model.edges[1].kind, vn::geom::EdgeKind::ConstructionCircle);
    EXPECT_TRUE(model.edges[1].closedLoop);
    ASSERT_EQ(model.edges[1].controls.size(), 1U);
    EXPECT_DOUBLE_EQ(model.edges[1].controls.front().x, 25.0);
    EXPECT_DOUBLE_EQ(model.edges[1].controls.front().y, 25.0);
}
