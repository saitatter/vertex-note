/*
 * VertexNote
 *
 * Topology-preserving transforms for object-local geometry.
 */

#pragma once

#include <span>

#include "vertexnote/geometry/GeometryObject.h"
#include "vertexnote/geometry/GeometryTypes.h"

namespace vn::geom {

struct GeometryTransform2D {
    Vec2 pivot;
    Vec2 translation;
    double rotationRadians = 0.0;
    double scaleX = 1.0;
    double scaleY = 1.0;
};

[[nodiscard]] auto transformedGeometry(const GeometryObject& object, std::span<const VertexId> vertexIds,
                                       const GeometryTransform2D& transform) -> GeometryObject;

}  // namespace vn::geom
