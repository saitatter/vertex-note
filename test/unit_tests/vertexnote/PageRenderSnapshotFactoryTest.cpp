/*
 * VertexNote unit tests
 */

#include <gtest/gtest.h>

#include "model/Document.h"
#include "model/NotePage.h"
#include "view/render/PageRenderSnapshotFactory.h"
#include "vertexnote/geometry/GeometryElement.h"

TEST(VertexNotePageRenderSnapshotFactory, buildsSharedSnapshotsFromGeometryPages) {
    Document document(nullptr);
    auto page = std::make_shared<NotePage>(320.0, 240.0);

    vn::geom::GeometryObject object(42);
    const auto a = object.addVertex(vn::geom::Vec2{10.0, 20.0});
    const auto b = object.addVertex(vn::geom::Vec2{80.0, 50.0});
    object.addLine(a, b);

    page->getSelectedLayer()->addElement(std::make_unique<vn::geom::GeometryElement>(std::move(object)));
    document.addPage(page);

    const auto snapshots = vn::view::render::buildPageRenderSnapshots(document);

    ASSERT_EQ(snapshots.size(), 1U);
    EXPECT_DOUBLE_EQ(snapshots.front().width, 320.0);
    EXPECT_DOUBLE_EQ(snapshots.front().height, 240.0);
    ASSERT_EQ(snapshots.front().drawables.size(), 1U);

    const auto* geometry = std::get_if<vn::view::render::GeometryRenderModel>(&snapshots.front().drawables.front());
    ASSERT_NE(geometry, nullptr);
    EXPECT_EQ(geometry->objectId, 42U);
    ASSERT_EQ(geometry->vertices.size(), 2U);
    ASSERT_EQ(geometry->edges.size(), 1U);
    EXPECT_DOUBLE_EQ(geometry->vertices.front().position.x, 10.0);
    EXPECT_DOUBLE_EQ(geometry->edges.front().end.y, 50.0);
}
