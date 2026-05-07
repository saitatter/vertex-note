/*
 * VertexNote
 *
 * Collect geometry objects from a Xournal++ page.
 */

#pragma once

#include <vector>

#include "model/PageRef.h"
#include "vertexnote/geometry/GeometryObject.h"

namespace vn::snap {

[[nodiscard]] auto collectGeometryObjects(const PageRef& page) -> std::vector<const geom::GeometryObject*>;

}  // namespace vn::snap
