/*
 * VertexNote unit tests
 */

#include <config-test.h>
#include <gtest/gtest.h>

#include "control/xojfile/LoadHandler.h"
#include "filesystem.h"
#include "model/Document.h"
#include "model/Font.h"
#include "model/NotePage.h"
#include "model/Stroke.h"
#include "model/TexImage.h"
#include "model/Text.h"
#include "view/render/PageRenderSnapshotFactory.h"
#include "vertexnote/geometry/GeometryElement.h"

static auto loadTestDocument(const fs::path& filepath) -> std::unique_ptr<Document> {
    EXPECT_NO_THROW(return LoadHandler{}.loadDocument(filepath)) << "Error while loading \"" << filepath << '\"';
    return {};
}

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

TEST(VertexNotePageRenderSnapshotFactory, includesStrokesInSnapshot) {
    Document document(nullptr);
    auto page = std::make_shared<NotePage>(400.0, 300.0);

    auto stroke = std::make_unique<Stroke>();
    stroke->setColor(Color{0xff, 0x00, 0x00, 0xff});
    stroke->setWidth(2.0);
    stroke->setToolType(StrokeTool(StrokeTool::PEN));
    stroke->addPoint(Point(10.0, 20.0));
    stroke->addPoint(Point(80.0, 90.0));

    page->getSelectedLayer()->addElement(std::move(stroke));
    document.addPage(page);

    const auto snapshots = vn::view::render::buildPageRenderSnapshots(document);

    ASSERT_EQ(snapshots.size(), 1U);
    ASSERT_EQ(snapshots.front().drawables.size(), 1U);

    const auto* strokeModel =
            std::get_if<vn::view::render::StrokeRenderModel>(&snapshots.front().drawables.front());
    ASSERT_NE(strokeModel, nullptr);
    EXPECT_EQ(strokeModel->color.red, 0xff);
    EXPECT_DOUBLE_EQ(strokeModel->width, 2.0);
    ASSERT_EQ(strokeModel->points.size(), 2U);
    EXPECT_DOUBLE_EQ(strokeModel->points[0].x, 10.0);
    EXPECT_DOUBLE_EQ(strokeModel->points[1].y, 90.0);
}

TEST(VertexNotePageRenderSnapshotFactory, includesTextInSnapshot) {
    Document document(nullptr);
    auto page = std::make_shared<NotePage>(400.0, 300.0);

    auto text = std::make_unique<Text>();
    text->setText("Test note");
    text->setFont(NoteFont("Sans", 12.0));
    text->setColor(Color{0x00, 0x00, 0x00, 0xff});
    text->setX(50.0);
    text->setY(60.0);

    page->getSelectedLayer()->addElement(std::move(text));
    document.addPage(page);

    const auto snapshots = vn::view::render::buildPageRenderSnapshots(document);

    ASSERT_EQ(snapshots.size(), 1U);
    ASSERT_EQ(snapshots.front().drawables.size(), 1U);

    const auto* textModel =
            std::get_if<vn::view::render::TextRenderModel>(&snapshots.front().drawables.front());
    ASSERT_NE(textModel, nullptr);
    EXPECT_EQ(textModel->content, "Test note");
    EXPECT_DOUBLE_EQ(textModel->x, 50.0);
    EXPECT_DOUBLE_EQ(textModel->y, 60.0);
}

TEST(VertexNotePageRenderSnapshotFactory, skipsStrokesWithTooFewPoints) {
    Document document(nullptr);
    auto page = std::make_shared<NotePage>(400.0, 300.0);

    auto stroke = std::make_unique<Stroke>();
    stroke->addPoint(Point(10.0, 20.0));  // Only 1 point — should be skipped

    page->getSelectedLayer()->addElement(std::move(stroke));
    document.addPage(page);

    const auto snapshots = vn::view::render::buildPageRenderSnapshots(document);

    ASSERT_EQ(snapshots.size(), 1U);
    EXPECT_EQ(snapshots.front().drawables.size(), 0U);
}

TEST(VertexNotePageRenderSnapshotFactory, skipsEmptyText) {
    Document document(nullptr);
    auto page = std::make_shared<NotePage>(400.0, 300.0);

    auto text = std::make_unique<Text>();
    // Empty text — should be skipped
    text->setFont(NoteFont("Sans", 12.0));

    page->getSelectedLayer()->addElement(std::move(text));
    document.addPage(page);

    const auto snapshots = vn::view::render::buildPageRenderSnapshots(document);

    ASSERT_EQ(snapshots.size(), 1U);
    EXPECT_EQ(snapshots.front().drawables.size(), 0U);
}

TEST(VertexNotePageRenderSnapshotFactory, mixedElementsPreserveOrder) {
    Document document(nullptr);
    auto page = std::make_shared<NotePage>(400.0, 300.0);

    // Add a stroke then a text
    auto stroke = std::make_unique<Stroke>();
    stroke->setWidth(1.0);
    stroke->addPoint(Point(0.0, 0.0));
    stroke->addPoint(Point(10.0, 10.0));
    page->getSelectedLayer()->addElement(std::move(stroke));

    auto text = std::make_unique<Text>();
    text->setText("After stroke");
    text->setFont(NoteFont("Sans", 12.0));
    page->getSelectedLayer()->addElement(std::move(text));

    document.addPage(page);

    const auto snapshots = vn::view::render::buildPageRenderSnapshots(document);

    ASSERT_EQ(snapshots.size(), 1U);
    ASSERT_EQ(snapshots.front().drawables.size(), 2U);

    // First drawable should be a stroke
    EXPECT_NE(std::get_if<vn::view::render::StrokeRenderModel>(&snapshots.front().drawables[0]), nullptr);
    // Second should be text
    EXPECT_NE(std::get_if<vn::view::render::TextRenderModel>(&snapshots.front().drawables[1]), nullptr);
}

TEST(VertexNotePageRenderSnapshotFactory, multiPageDocument) {
    Document document(nullptr);
    document.addPage(std::make_shared<NotePage>(100.0, 200.0));
    document.addPage(std::make_shared<NotePage>(300.0, 400.0));

    const auto snapshots = vn::view::render::buildPageRenderSnapshots(document);

    ASSERT_EQ(snapshots.size(), 2U);
    EXPECT_DOUBLE_EQ(snapshots[0].width, 100.0);
    EXPECT_DOUBLE_EQ(snapshots[0].height, 200.0);
    EXPECT_DOUBLE_EQ(snapshots[1].width, 300.0);
    EXPECT_DOUBLE_EQ(snapshots[1].height, 400.0);
}

TEST(VertexNotePageRenderSnapshotFactory, capturesBackgroundFormat) {
    Document document(nullptr);
    auto page = std::make_shared<NotePage>(400.0, 300.0);
    page->setBackgroundType(PageType(PageTypeFormat::Graph));
    page->setBackgroundColor(Color{0xdd, 0xdd, 0xdd, 0xff});
    document.addPage(page);

    const auto snapshots = vn::view::render::buildPageRenderSnapshots(document);

    ASSERT_EQ(snapshots.size(), 1U);
    EXPECT_EQ(snapshots.front().background.backgroundFormat, PageTypeFormat::Graph);
    EXPECT_EQ(snapshots.front().background.backgroundColor.red, 0xdd);
}

TEST(VertexNotePageRenderSnapshotFactory, includesLatexImagesInSnapshot) {
    auto document = loadTestDocument(GET_TESTFILE(u8"load/latex.xopp"));
    ASSERT_TRUE(document);

    const auto snapshots = vn::view::render::buildPageRenderSnapshots(*document);

    ASSERT_EQ(snapshots.size(), 1U);
    ASSERT_EQ(snapshots.front().drawables.size(), 1U);
    const auto* image = std::get_if<vn::view::render::ImageRenderModel>(&snapshots.front().drawables.front());
    ASSERT_NE(image, nullptr);
    EXPECT_FALSE(image->rasterContent.empty());
    EXPECT_GT(image->width, 0.0);
    EXPECT_GT(image->height, 0.0);
}
