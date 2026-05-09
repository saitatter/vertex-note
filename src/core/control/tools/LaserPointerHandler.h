/*
 * VertexNote
 *
 * Handles a laser pointer that draws temporary strokes
 *
 * @author VertexNote Team
 * https://github.com/saitatter/vertex-note
 *
 * @license GNU GPLv2 or later
 */

#pragma once

#include "view/ViewNamespaceAliases.h"

#include <cstdint>
#include <memory>  // for unique_ptr

#include "model/OverlayBase.h"
#include "model/PageRef.h"  // for PageRef
#include "util/raii/GSourceURef.h"

class Control;
class PositionInputData;
class TemporaryStrokeHandler;
class PageView;

namespace xoj::util {
template <class T>
class DispatchPool;
};


namespace vn::view {
class OverlayView;
class Repaintable;
class LaserPointerView;
};  // namespace vn::view

class LaserPointerHandler: public OverlayBase {
public:
    LaserPointerHandler(PageView* pageView, Control* control, const PageRef& page);
    ~LaserPointerHandler() override;

    void onSequenceCancelEvent();
    bool onMotionNotifyEvent(const PositionInputData& pos, double zoom);
    void onButtonReleaseEvent(const PositionInputData& pos, double zoom);
    void onButtonPressEvent(const PositionInputData& pos, double zoom);

    auto createView(vn::view::Repaintable* parent) const -> std::unique_ptr<vn::view::OverlayView>;

    inline auto getViewPool() const -> std::shared_ptr<xoj::util::DispatchPool<vn::view::LaserPointerView>> {
        return viewPool;
    }

private:
    static void triggerFadeoutCallback(LaserPointerHandler* self);
    static gboolean fadeoutCallback(LaserPointerHandler* self);

private:
    std::unique_ptr<TemporaryStrokeHandler> strokehandler;
    std::shared_ptr<xoj::util::DispatchPool<vn::view::LaserPointerView>> viewPool;

    // Used only to pass on to (Temporary)StrokeHandler
    Control* ctrl;
    PageRef page;
    PageView* pageView;

    xoj::util::GSourceURef fadeoutTimer;
    uint8_t fadeoutAlpha = 255;
    const unsigned int fadeoutStartDelay;
    bool hasFinishedStrokes;
};
