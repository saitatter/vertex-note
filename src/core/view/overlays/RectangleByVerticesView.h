/*
 * VertexNote
 *
 * Overlay for click-based vertex rectangle creation.
 */

#pragma once

#include "util/DispatchPool.h"
#include "view/overlays/OverlayView.h"

class OverlayBase;
class Range;
class RectangleByVerticesHandler;

namespace xoj::view {
class Repaintable;

class RectangleByVerticesView final: public ToolView, public xoj::util::Listener<RectangleByVerticesView> {
public:
    RectangleByVerticesView(const RectangleByVerticesHandler* handler, Repaintable* parent);
    ~RectangleByVerticesView() noexcept override;

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
    const RectangleByVerticesHandler* handler;
};

}  // namespace xoj::view
