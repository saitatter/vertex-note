/*
 * VertexNote unit tests
 */

#include <config-test.h>
#include <gtest/gtest.h>

#include <utility>

#include "model/Element.h"
#include "util/Color.h"
#include "vertexnote/geometry/GeometryElement.h"

using vn::geom::GeometryElement;
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

TEST(VertexNoteGeometryElement, clonesGeometryState) {
    GeometryElement element = makeLineElement();
    auto clone = element.clone();

    ASSERT_NE(clone, nullptr);
    EXPECT_EQ(clone->getType(), ELEMENT_GEOMETRY);
    EXPECT_DOUBLE_EQ(clone->getX(), element.getX());
    EXPECT_DOUBLE_EQ(clone->getElementWidth(), element.getElementWidth());
    EXPECT_EQ(clone->getColor(), Colors::black);
}
