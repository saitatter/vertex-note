/*
 * VertexNote
 *
 * Overlay for click-based circle creation.
 */

#pragma once

#include "util/DispatchPool.h"
#include "view/overlays/OverlayView.h"

class CircleByCenterRadiusHandler;
class OverlayBase;
class Range;

namespace vn::view {
class Repaintable;

class CircleByCenterRadiusView final: public ToolView, public vn::util::Listener<CircleByCenterRadiusView> {
public:
    CircleByCenterRadiusView(const CircleByCenterRadiusHandler* handler, Repaintable* parent);
    ~CircleByCenterRadiusView() noexcept override;

    void draw(cairo_t* cr) const override;
    bool isViewOf(const OverlayBase* overlay) const override;

    static constexpr struct FlagDirtyRegionRequest {
    } FLAG_DIRTY_REGION = {};
    void on(FlagDirtyRegionRequest, const Range& range);

    static constexpr struct FinalizationRequest {
    } FINALIZATION_REQUEST = {};
    void deleteOn(FinalizationRequest, const Range& range);

    static constexpr struct CancellationRequest {
    } CANCELLATION_REQUEST = {};
    void deleteOn(CancellationRequest, const Range& range);

private:
    const CircleByCenterRadiusHandler* handler;
};

}  // namespace vn::view
