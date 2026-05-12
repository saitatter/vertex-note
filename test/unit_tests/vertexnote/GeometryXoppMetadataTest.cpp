/*
 * VertexNote unit tests
 */

#include <gtest/gtest.h>

#include "control/xojfile/LoadHandler.h"
#include "control/xojfile/SaveHandler.h"
#include "model/Document.h"
#include "model/Layer.h"
#include "model/NotePage.h"
#include "util/PathUtil.h"
#include "vertexnote/geometry/GeometryElement.h"
#include "vertexnote/io/GeometryXoppMetadata.h"

using vn::geom::ConstraintKind;
using vn::geom::EdgeKind;
using vn::geom::GeometryObject;
using vn::geom::Vec2;
using vn::geom::VertexFlags;

TEST(VertexNoteGeometryXoppMetadata, roundTripsObjectGraph) {
    GeometryObject object(42);
    auto a = object.addVertexWithId(10, Vec2{1.25, 2.5}, VertexFlags::Explicit);
    auto b = object.addVertexWithId(20, Vec2{5.0, 2.5}, VertexFlags::Locked);
    auto edge = object.addEdgeWithId(30, EdgeKind::Line, a, b);
    object.addConstraintWithId(40, ConstraintKind::FixedLength, {a, b}, {edge}, 3.75);

    const auto metadata = vn::io::serializeGeometryStrokeMetadata(object);
    std::string error;
    auto restored = vn::io::parseGeometryStrokeMetadata(metadata, &error);

    ASSERT_TRUE(restored.has_value()) << error;
    EXPECT_EQ(restored->objectId(), 42U);
    ASSERT_NE(restored->vertex(10), nullptr);
    ASSERT_NE(restored->vertex(20), nullptr);
    ASSERT_NE(restored->edge(30), nullptr);
    ASSERT_NE(restored->constraint(40), nullptr);
    EXPECT_DOUBLE_EQ(restored->vertex(10)->position.x, 1.25);
    EXPECT_EQ(restored->vertex(20)->flags, VertexFlags::Locked);
    EXPECT_EQ(restored->edge(30)->kind, EdgeKind::Line);
    EXPECT_DOUBLE_EQ(restored->constraint(40)->value, 3.75);
}

TEST(VertexNoteGeometryXoppMetadata, roundTripsConstructionLineKind) {
    GeometryObject object(42);
    auto a = object.addVertexWithId(10, Vec2{1.0, 1.0}, VertexFlags::Explicit);
    auto b = object.addVertexWithId(20, Vec2{9.0, 3.0}, VertexFlags::Explicit);
    object.addEdgeWithId(30, EdgeKind::ConstructionLine, a, b);

    const auto metadata = vn::io::serializeGeometryStrokeMetadata(object);
    std::string error;
    auto restored = vn::io::parseGeometryStrokeMetadata(metadata, &error);

    ASSERT_TRUE(restored.has_value()) << error;
    ASSERT_NE(restored->edge(30), nullptr);
    EXPECT_EQ(restored->edge(30)->kind, EdgeKind::ConstructionLine);
}

TEST(VertexNoteGeometryXoppMetadata, roundTripsConstructionCircleKind) {
    GeometryObject object(42);
    auto center = object.addVertexWithId(10, Vec2{4.0, 4.0}, VertexFlags::Explicit);
    auto radiusPoint = object.addVertexWithId(20, Vec2{7.0, 4.0}, VertexFlags::Explicit);
    object.addEdgeWithId(30, EdgeKind::ConstructionCircle, radiusPoint, radiusPoint, {center});

    const auto metadata = vn::io::serializeGeometryStrokeMetadata(object);
    std::string error;
    auto restored = vn::io::parseGeometryStrokeMetadata(metadata, &error);

    ASSERT_TRUE(restored.has_value()) << error;
    ASSERT_NE(restored->edge(30), nullptr);
    EXPECT_EQ(restored->edge(30)->kind, EdgeKind::ConstructionCircle);
}

TEST(VertexNoteGeometryXoppMetadata, rejectsUnsupportedFormat) {
    vn::io::GeometryStrokeMetadata metadata;
    metadata.format = "geometry-v99";
    metadata.objectId = "42";

    std::string error;
    auto restored = vn::io::parseGeometryStrokeMetadata(metadata, &error);

    EXPECT_FALSE(restored.has_value());
    EXPECT_FALSE(error.empty());
}

TEST(VertexNoteGeometryXoppMetadata, savesAndLoadsGeometryElementThroughXoppStrokeFallback) {
    Document document(nullptr);
    auto page = std::make_shared<NotePage>(200.0, 200.0);
    auto object = GeometryObject(42);
    auto a = object.addVertexWithId(10, Vec2{1.0, 2.0});
    auto b = object.addVertexWithId(20, Vec2{5.0, 7.0});
    object.addLine(a, b);

    auto geometry = std::make_unique<vn::geom::GeometryElement>(std::move(object));
    geometry->setStrokeWidth(2.5);
    geometry->setColor(Colors::red);
    page->getSelectedLayer()->addElement(std::move(geometry));
    document.addPage(page);

    const auto outputPath = Util::getTmpDirSubfolder() / "vertexnote-geometry-roundtrip.xopp";
    SaveHandler saver;
    saver.prepareSave(&document, outputPath);
    saver.saveTo(outputPath);

    auto loaded = LoadHandler().loadDocument(outputPath);
    ASSERT_EQ(loaded->getPageCount(), 1U);
    auto loadedElements = loaded->getPage(0)->getSelectedLayer()->getElementsView();
    ASSERT_EQ(loadedElements.size(), 1U);

    const auto* loadedGeometry = dynamic_cast<const vn::geom::GeometryElement*>(loadedElements.front());
    ASSERT_NE(loadedGeometry, nullptr);
    EXPECT_EQ(loadedGeometry->getType(), ELEMENT_GEOMETRY);
    EXPECT_DOUBLE_EQ(loadedGeometry->getStrokeWidth(), 2.5);
    EXPECT_EQ(loadedGeometry->getColor(), Colors::red);
    EXPECT_EQ(loadedGeometry->geometry().objectId(), 42U);
    ASSERT_NE(loadedGeometry->geometry().vertex(10), nullptr);
    EXPECT_DOUBLE_EQ(loadedGeometry->geometry().vertex(10)->position.x, 1.0);
    EXPECT_DOUBLE_EQ(loadedGeometry->geometry().vertex(20)->position.y, 7.0);
}
