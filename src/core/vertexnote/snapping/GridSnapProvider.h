/*
 * VertexNote
 *
 * Grid snapping provider.
 */

#pragma once

#include "vertexnote/snapping/ISnapProvider.h"

namespace vn::snap {

class GridSnapProvider final: public ISnapProvider {
public:
    GridSnapProvider(double columnSpacing, double rowSpacing, double tolerance, double xOffset = 0.0,
                     double yOffset = 0.0);

    void query(const SnapQuery& query, std::vector<SnapCandidate>& candidates) const override;

private:
    double columnSpacing;
    double rowSpacing;
    double tolerance;
    double xOffset;
    double yOffset;
};

}  // namespace vn::snap
