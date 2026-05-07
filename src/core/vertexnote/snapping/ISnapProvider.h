/*
 * VertexNote
 *
 * Provider interface for snapping candidates.
 */

#pragma once

#include <vector>

#include "vertexnote/snapping/SnapTypes.h"

namespace vn::snap {

class ISnapProvider {
public:
    virtual ~ISnapProvider() = default;

    virtual void query(const SnapQuery& query, std::vector<SnapCandidate>& candidates) const = 0;
};

}  // namespace vn::snap
