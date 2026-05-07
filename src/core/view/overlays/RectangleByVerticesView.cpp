/*
 * VertexNote
 *
 * Overlay for click-based vertex rectangle creation.
 */

#include "RectangleByVerticesView.h"

#include <cairo.h>

#include "control/tools/RectangleByVerticesHandler.h"
#include "util/Color.h"
#include "util/raii/CairoWrappers.h"
#include "view/Repaintable.h"

using namespace xoj::view;

RectangleByVerticesView::RectangleByVerticesView(const RectangleByVerticesHandler* handler, Repaintable* parent):
        ToolView(parent), handler(handler) {
    this->registerToPool(handler->getViewPool());
}

RectangleByVerticesView::~RectangleByVerticesView() noexcept { this->unregisterFromPool(); }

void RectangleByVerticesView::draw(cairo_t* cr) const {
    if (!this->handler->hasPreview()) {
        return;
    }

    const Point start = this->handler->getStartPoint();
    const Point current = this->handler->getCurrentPoint();

    xoj::util::CairoSaveGuard saveGuard(cr);
    cairo_set_line_cap(cr, CAIRO_LINE_CAP_ROUND);
    cairo_set_line_join(cr, CAIRO_LINE_JOIN_ROUND);
    cairo_set_line_width(cr, this->handler->getStrokeWidth());
    Util::cairo_set_source_rgbi(cr, this->handler->getStrokeColor());
    cairo_rectangle(cr, start.x, start.y, current.x - start.x, current.y - start.y);
    cairo_stroke(cr);
}

bool RectangleByVerticesView::isViewOf(const OverlayBase* overlay) const { return overlay == this->handler; }

void RectangleByVerticesView::on(RectangleByVerticesView::FlagDirtyRegionRequest, const Range& range) {
    this->parent->flagDirtyRegion(range);
}

void RectangleByVerticesView::deleteOn(RectangleByVerticesView::FinalizationRequest, const Range& range) {
    this->parent->deleteOverlayView(this, range);
}

void RectangleByVerticesView::deleteOn(RectangleByVerticesView::CancellationRequest, const Range& range) {
    this->parent->deleteOverlayView(this, range);
}
