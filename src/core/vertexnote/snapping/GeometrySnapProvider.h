/*
 * VertexNote
 *
 * Snapping provider for VertexNote geometry objects.
 */

#pragma once

#include <vector>

#include "vertexnote/geometry/GeometryObject.h"
#include "vertexnote/snapping/GeometrySpatialIndex.h"
#include "vertexnote/snapping/ISnapProvider.h"

namespace vn::snap {

class GeometrySnapProvider final: public ISnapProvider {
public:
    GeometrySnapProvider() = default;
    explicit GeometrySnapProvider(std::vector<const geom::GeometryObject*> objects);

    void setObjects(std::vector<const geom::GeometryObject*> objects);
    void query(const SnapQuery& query, std::vector<SnapCandidate>& candidates) const override;

private:
    void rebuildExplicitVertices();
    void rebuildLineSegments();

private:
    std::vector<const geom::GeometryObject*> objects;
    std::vector<IndexedPoint> explicitVertices;
    std::vector<IndexedSegment> lineSegments;
    GeometrySpatialIndex explicitVertexIndex;
    GeometrySpatialIndex lineSegmentIndex;
};

}  // namespace vn::snap
