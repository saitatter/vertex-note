/*
 * VertexNote
 *
 * Overlay for click-based polyline creation.
 */

#pragma once

#include "util/DispatchPool.h"
#include "view/overlays/OverlayView.h"

class OverlayBase;
class PolylineByClicksHandler;
class Range;

namespace vn::view {
class Repaintable;

class PolylineByClicksView final: public ToolView, public xoj::util::Listener<PolylineByClicksView> {
public:
    PolylineByClicksView(const PolylineByClicksHandler* handler, Repaintable* parent);
    ~PolylineByClicksView() noexcept override;

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
    const PolylineByClicksHandler* handler;
};

}  // namespace vn::view
