/*
 * VertexNote
 *
 * Provider-based snapping engine.
 */

#include "SnapEngine.h"

#include <cmath>
#include <limits>
#include <utility>

namespace vn::snap {

namespace {
constexpr double PRIORITY_EPSILON = 0.000001;
constexpr double DISTANCE_PRIORITY_PENALTY = 3.5;

[[nodiscard]] auto effectivePriority(const SnapCandidate& candidate) -> double {
    return candidate.priority - candidate.screenDistance * DISTANCE_PRIORITY_PENALTY;
}
}

void SnapEngine::addProvider(std::shared_ptr<const ISnapProvider> provider) {
    if (provider) {
        this->providers.push_back(std::move(provider));
    }
}

void SnapEngine::clearProviders() { this->providers.clear(); }

auto SnapEngine::snap(const SnapQuery& query) const -> SnapResult {
    std::vector<SnapCandidate> candidates;
    for (const auto& provider: this->providers) {
        provider->query(query, candidates);
    }

    SnapResult result{query.pagePoint, query.pagePoint, std::nullopt};
    SnapCandidate best;
    best.screenDistance = std::numeric_limits<double>::max();
    best.priority = -std::numeric_limits<double>::max();

    for (const auto& candidate: candidates) {
        if (candidate.screenDistance > query.maxScreenDistance) {
            continue;
        }
        if (!result.candidate || isBetterCandidate(candidate, best)) {
            best = candidate;
            result.candidate = candidate;
            result.pagePoint = candidate.pagePoint;
        }
    }

    return result;
}

auto SnapEngine::isBetterCandidate(const SnapCandidate& candidate, const SnapCandidate& current) -> bool {
    const double candidatePriority = effectivePriority(candidate);
    const double currentPriority = effectivePriority(current);
    if (std::abs(candidatePriority - currentPriority) > PRIORITY_EPSILON) {
        return candidatePriority > currentPriority;
    }
    return candidate.screenDistance < current.screenDistance;
}

}  // namespace vn::snap
