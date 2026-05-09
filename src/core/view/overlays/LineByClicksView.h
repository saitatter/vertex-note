/*
 * VertexNote
 *
 * Overlay for click-based line creation.
 */

#pragma once

#include "util/DispatchPool.h"
#include "view/overlays/OverlayView.h"

class LineByClicksHandler;
class OverlayBase;
class Range;

namespace vn::view {
class Repaintable;

class LineByClicksView final: public ToolView, public xoj::util::Listener<LineByClicksView> {
public:
    LineByClicksView(const LineByClicksHandler* handler, Repaintable* parent);
    ~LineByClicksView() noexcept override;

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
    const LineByClicksHandler* handler;
};

}  // namespace vn::view
