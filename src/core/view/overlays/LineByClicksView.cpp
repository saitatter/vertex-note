/*
 * VertexNote
 *
 * Overlay for click-based line creation.
 */

#include "LineByClicksView.h"

#include <cairo.h>

#include "control/tools/LineByClicksHandler.h"
#include "util/Color.h"
#include "util/raii/CairoWrappers.h"
#include "view/Repaintable.h"
#include "view/overlays/SnapIndicatorViewHelper.h"

using namespace vn::view;

LineByClicksView::LineByClicksView(const LineByClicksHandler* handler, Repaintable* parent):
        ToolView(parent), handler(handler) {
    this->registerToPool(handler->getViewPool());
}

LineByClicksView::~LineByClicksView() noexcept { this->unregisterFromPool(); }

void LineByClicksView::draw(cairo_t* cr) const {
    if (!this->handler->hasPreview()) {
        return;
    }

    const Point start = this->handler->getStartPoint();
    const Point current = this->handler->getCurrentPoint();

    vn::util::CairoSaveGuard saveGuard(cr);
    cairo_set_line_cap(cr, CAIRO_LINE_CAP_ROUND);
    cairo_set_line_join(cr, CAIRO_LINE_JOIN_ROUND);
    cairo_set_line_width(cr, this->handler->getStrokeWidth());
    Util::cairo_set_source_rgbi(cr, this->handler->getStrokeColor());
    cairo_move_to(cr, start.x, start.y);
    cairo_line_to(cr, current.x, current.y);
    cairo_stroke(cr);
    drawSnapIndicator(cr, current, this->handler->getCurrentSnapKind());
}

bool LineByClicksView::isViewOf(const OverlayBase* overlay) const { return overlay == this->handler; }

void LineByClicksView::on(LineByClicksView::FlagDirtyRegionRequest, const Range& range) {
    this->parent->flagDirtyRegion(range);
}

void LineByClicksView::deleteOn(LineByClicksView::FinalizationRequest, const Range& range) {
    this->parent->deleteOverlayView(this, range);
}

void LineByClicksView::deleteOn(LineByClicksView::CancellationRequest, const Range& range) {
    this->parent->deleteOverlayView(this, range);
}
