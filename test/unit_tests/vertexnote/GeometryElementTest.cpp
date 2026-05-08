/*
 * VertexNote unit tests
 */

#include <config-test.h>
#include <gtest/gtest.h>

#include <string>
#include <utility>

#include <glib.h>

#include "model/Element.h"
#include "util/Color.h"
#include "util/serializing/BinObjectEncoding.h"
#include "util/serializing/ObjectInputStream.h"
#include "util/serializing/ObjectOutputStream.h"
#include "vertexnote/geometry/GeometryElement.h"
#include "vertexnote/geometry/GeometryIdGenerator.h"

using vn::geom::ConstraintKind;
using vn::geom::GeometryElement;
using vn::geom::GeometryIdGenerator;
using vn::geom::GeometryObject;
using vn::geom::Vec2;

namespace {

auto makeLineElement() -> GeometryElement {
    GeometryObject object(42);
    auto a = object.addVertex(Vec2{1.0, 2.0});
    auto b = object.addVertex(Vec2{5.0, 2.0});
    object.addLine(a, b);

    GeometryElement element(std::move(object));
    element.setColor(Colors::black);
    element.setStrokeWidth(2.0);
    return element;
}

}  // namespace

TEST(VertexNoteGeometryElement, exposesGeometryElementType) {
    GeometryElement element = makeLineElement();

    EXPECT_EQ(element.getType(), ELEMENT_GEOMETRY);
}

TEST(VertexNoteGeometryElement, computesPaddedBounds) {
    GeometryElement element = makeLineElement();

    EXPECT_DOUBLE_EQ(element.getX(), 0.0);
    EXPECT_DOUBLE_EQ(element.getY(), 1.0);
    EXPECT_DOUBLE_EQ(element.getElementWidth(), 6.0);
    EXPECT_DOUBLE_EQ(element.getElementHeight(), 2.0);
}

TEST(VertexNoteGeometryElement, computesDistanceToDrawnGeometry) {
    GeometryElement element = makeLineElement();

    EXPECT_DOUBLE_EQ(element.distanceTo(3.0, 2.0), 0.0);
    EXPECT_DOUBLE_EQ(element.distanceTo(3.0, 5.0), 2.0);
}

TEST(VertexNoteGeometryElement, movesGeometryAndCachedBounds) {
    GeometryElement element = makeLineElement();
    static_cast<void>(element.getX());  // Populate cached bounds before moving.

    element.move(10.0, -1.0);

    EXPECT_DOUBLE_EQ(element.getX(), 10.0);
    EXPECT_DOUBLE_EQ(element.getY(), 0.0);
    EXPECT_DOUBLE_EQ(element.distanceTo(13.0, 1.0), 0.0);
}

TEST(VertexNoteGeometryElement, movesIndividualVertex) {
    GeometryElement element = makeLineElement();
    auto firstVertexId = element.geometry().vertices().front().id;

    ASSERT_TRUE(element.setVertexPosition(firstVertexId, Vec2{9.0, 11.0}));

    EXPECT_DOUBLE_EQ(element.geometry().vertex(firstVertexId)->position.x, 9.0);
    EXPECT_DOUBLE_EQ(element.geometry().vertex(firstVertexId)->position.y, 11.0);
    EXPECT_FALSE(element.setVertexPosition(999, Vec2{1.0, 1.0}));
}

TEST(VertexNoteGeometryElement, clonesGeometryState) {
    GeometryElement element = makeLineElement();
    auto clone = element.clone();

    ASSERT_NE(clone, nullptr);
    EXPECT_EQ(clone->getType(), ELEMENT_GEOMETRY);
    EXPECT_DOUBLE_EQ(clone->getX(), element.getX());
    EXPECT_DOUBLE_EQ(clone->getElementWidth(), element.getElementWidth());
    EXPECT_EQ(clone->getColor(), Colors::black);
}

TEST(VertexNoteGeometryElement, serializesClipboardGeometryState) {
    GeometryObject object(84);
    const auto a = object.addVertex(Vec2{1.0, 2.0});
    const auto b = object.addVertex(Vec2{5.0, 2.0});
    const auto edge = object.addLine(a, b);
    object.addConstraint(ConstraintKind::FixedLength, {}, {edge}, 4.0);

    GeometryElement element(std::move(object));
    element.setColor(Colors::red);
    element.setStrokeWidth(3.0);

    ObjectOutputStream out(new BinObjectEncoding);
    element.serialize(out);
    auto* raw = out.stealData();
    const std::string serialized(raw->str, raw->len);
    g_string_free(raw, true);

    ObjectInputStream in;
    ASSERT_TRUE(in.read(serialized.c_str(), serialized.size() + 1U));
    ASSERT_EQ(in.getNextObjectName(), "GeometryElement");

    GeometryElement loaded;
    loaded.readSerialized(in);

    EXPECT_EQ(loaded.getColor(), Colors::red);
    EXPECT_DOUBLE_EQ(loaded.getStrokeWidth(), 3.0);
    EXPECT_EQ(loaded.geometry().objectId(), 84U);
    EXPECT_EQ(loaded.geometry().vertices().size(), 2U);
    EXPECT_EQ(loaded.geometry().edges().size(), 1U);
    EXPECT_EQ(loaded.geometry().constraints().size(), 1U);
    EXPECT_DOUBLE_EQ(loaded.geometry().vertex(a)->position.x, 1.0);
    EXPECT_DOUBLE_EQ(loaded.geometry().vertex(b)->position.x, 5.0);
}

TEST(VertexNoteGeometryElement, assignsNewObjectIdAndRetargetsVertices) {
    GeometryIdGenerator::resetForTests(200);
    GeometryElement element = makeLineElement();
    const auto oldObjectId = element.geometry().objectId();

    element.assignNewObjectId();

    EXPECT_NE(element.geometry().objectId(), oldObjectId);
    EXPECT_EQ(element.geometry().objectId(), 200U);
    for (const auto& vertex: element.geometry().vertices()) {
        EXPECT_EQ(vertex.owner, element.geometry().objectId());
    }
    GeometryIdGenerator::resetForTests();
}
