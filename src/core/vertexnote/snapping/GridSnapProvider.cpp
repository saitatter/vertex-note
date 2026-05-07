/*
 * VertexNote
 *
 * Grid snapping provider.
 */

#include "GridSnapProvider.h"

#include <cmath>

namespace vn::snap {

namespace {

[[nodiscard]] auto roundToMultiple(double value, double multiple) -> double {
    return value - std::remainder(value, multiple);
}

[[nodiscard]] auto distance(geom::Vec2 lhs, geom::Vec2 rhs) -> double {
    return std::hypot(rhs.x - lhs.x, rhs.y - lhs.y);
}

}  // namespace

GridSnapProvider::GridSnapProvider(double columnSpacing, double rowSpacing, double tolerance, double xOffset,
                                   double yOffset):
        columnSpacing(columnSpacing), rowSpacing(rowSpacing), tolerance(tolerance), xOffset(xOffset), yOffset(yOffset) {}

void GridSnapProvider::query(const SnapQuery& query, std::vector<SnapCandidate>& candidates) const {
    if (this->columnSpacing <= 0.0 || this->rowSpacing <= 0.0 || this->tolerance <= 0.0) {
        return;
    }

    const geom::Vec2 snapped{roundToMultiple(query.pagePoint.x - this->xOffset, this->columnSpacing) + this->xOffset,
                             roundToMultiple(query.pagePoint.y - this->yOffset, this->rowSpacing) + this->yOffset};
    const double pageDistance = distance(query.pagePoint, snapped);
    const double pageThreshold = 0.5 * std::hypot(this->columnSpacing, this->rowSpacing) * this->tolerance;
    if (pageDistance > pageThreshold) {
        return;
    }

    candidates.push_back(SnapCandidate{SnapKind::Grid, snapped, pageDistance * query.zoom, 10.0});
}

}  // namespace vn::snap
