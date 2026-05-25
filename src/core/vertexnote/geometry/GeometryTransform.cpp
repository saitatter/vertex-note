/*
 * VertexNote
 *
 * Topology-preserving transforms for object-local geometry.
 */

#include "vertexnote/geometry/GeometryTransform.h"

#include <cmath>

namespace vn::geom {

auto transformedGeometry(const GeometryObject& object, std::span<const VertexId> vertexIds,
                         const GeometryTransform2D& transform) -> GeometryObject {
    auto result = object;
    const double cosine = std::cos(transform.rotationRadians);
    const double sine = std::sin(transform.rotationRadians);

    for (auto vertexId: vertexIds) {
        const auto* sourceVertex = object.vertex(vertexId);
        if (!sourceVertex) {
            continue;
        }
        const double localX = (sourceVertex->position.x - transform.pivot.x) * transform.scaleX;
        const double localY = (sourceVertex->position.y - transform.pivot.y) * transform.scaleY;
        const Vec2 transformed{
                .x = transform.pivot.x + localX * cosine - localY * sine + transform.translation.x,
                .y = transform.pivot.y + localX * sine + localY * cosine + transform.translation.y,
        };
        static_cast<void>(result.setVertexPosition(vertexId, transformed));
    }
    return result;
}

}  // namespace vn::geom
