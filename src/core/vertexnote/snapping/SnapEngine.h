/*
 * VertexNote
 *
 * Provider-based snapping engine.
 */

#pragma once

#include <memory>
#include <vector>

#include "vertexnote/snapping/ISnapProvider.h"
#include "vertexnote/snapping/SnapTypes.h"

namespace vn::snap {

class SnapEngine {
public:
    void addProvider(std::shared_ptr<const ISnapProvider> provider);
    void clearProviders();

    [[nodiscard]] auto snap(const SnapQuery& query) const -> SnapResult;

private:
    [[nodiscard]] static auto isBetterCandidate(const SnapCandidate& candidate, const SnapCandidate& current) -> bool;

private:
    std::vector<std::shared_ptr<const ISnapProvider>> providers;
};

}  // namespace vn::snap
