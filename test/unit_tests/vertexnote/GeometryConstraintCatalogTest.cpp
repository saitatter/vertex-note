/*
 * VertexNote unit tests
 */

#include <gtest/gtest.h>

#include "vertexnote/constraints/GeometryConstraintCatalog.h"

using vn::constraints::constraintDescriptors;
using vn::constraints::descriptorFor;
using vn::constraints::displayNameFor;
using vn::constraints::isSolverSupported;
using vn::constraints::stableIdFor;
using vn::geom::ConstraintKind;

TEST(VertexNoteGeometryConstraintCatalog, describesEveryConstraintKind) {
    EXPECT_EQ(constraintDescriptors().size(), 10U);

    EXPECT_NE(descriptorFor(ConstraintKind::Coincident), nullptr);
    EXPECT_NE(descriptorFor(ConstraintKind::Horizontal), nullptr);
    EXPECT_NE(descriptorFor(ConstraintKind::Vertical), nullptr);
    EXPECT_NE(descriptorFor(ConstraintKind::Parallel), nullptr);
    EXPECT_NE(descriptorFor(ConstraintKind::Perpendicular), nullptr);
    EXPECT_NE(descriptorFor(ConstraintKind::EqualLength), nullptr);
    EXPECT_NE(descriptorFor(ConstraintKind::FixedLength), nullptr);
    EXPECT_NE(descriptorFor(ConstraintKind::FixedAngle), nullptr);
    EXPECT_NE(descriptorFor(ConstraintKind::Radius), nullptr);
    EXPECT_NE(descriptorFor(ConstraintKind::OnEdge), nullptr);
}

TEST(VertexNoteGeometryConstraintCatalog, exposesUiAndSolverMetadata) {
    const auto* fixedLength = descriptorFor(ConstraintKind::FixedLength);
    ASSERT_NE(fixedLength, nullptr);

    EXPECT_EQ(stableIdFor(ConstraintKind::FixedLength), "fixed-length");
    EXPECT_EQ(displayNameFor(ConstraintKind::FixedLength), "Fixed length");
    EXPECT_EQ(fixedLength->minVertices, 2U);
    EXPECT_TRUE(fixedLength->requiresValue);
    EXPECT_TRUE(isSolverSupported(ConstraintKind::FixedLength));

    const auto* equalLength = descriptorFor(ConstraintKind::EqualLength);
    ASSERT_NE(equalLength, nullptr);
    EXPECT_FALSE(equalLength->solverSupported);
}
