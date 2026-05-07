/*
 * VertexNote unit tests
 *
 * Geometry ID generation.
 */

#include <config-test.h>
#include <gtest/gtest.h>

#include "vertexnote/geometry/GeometryIdGenerator.h"

using vn::geom::GeometryIdGenerator;
using vn::geom::InvalidObjectId;

TEST(VertexNoteGeometryIdGenerator, generatesNonInvalidMonotonicObjectIds) {
    GeometryIdGenerator::resetForTests();

    const auto first = GeometryIdGenerator::nextObjectId();
    const auto second = GeometryIdGenerator::nextObjectId();

    EXPECT_NE(first, InvalidObjectId);
    EXPECT_EQ(second, first + 1);
}

TEST(VertexNoteGeometryIdGenerator, observesLoadedObjectIds) {
    GeometryIdGenerator::resetForTests();

    GeometryIdGenerator::observeObjectId(99);

    EXPECT_EQ(GeometryIdGenerator::nextObjectId(), 100U);
}
