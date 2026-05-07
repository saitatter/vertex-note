/*
 * VertexNote
 *
 * Process-local geometry ID generation.
 */

#pragma once

#include "vertexnote/geometry/GeometryTypes.h"

namespace vn::geom {

class GeometryIdGenerator {
public:
    [[nodiscard]] static auto nextObjectId() -> ObjectId;
    static void observeObjectId(ObjectId id);
    static void resetForTests(ObjectId nextId = InvalidObjectId + 1);
};

}  // namespace vn::geom
