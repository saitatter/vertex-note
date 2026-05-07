/*
 * VertexNote
 *
 * Overlay for click-based polyline creation.
 */

#include "PolylineByClicksView.h"

#include <iterator>

#include <cairo.h>

#include "control/tools/PolylineByClicksHandler.h"
#include "util/Color.h"
#include "util/raii/CairoWrappers.h"
#include "view/Repaintable.h"

using namespace xoj::view;

PolylineByClicksView::PolylineByClicksView(const PolylineByClicksHandler* handler, Repaintable* parent):
        ToolView(parent), handler(handler) {
    this->registerToPool(handler->getViewPool());
}

PolylineByClicksView::~PolylineByClicksView() noexcept { this->unregisterFromPool(); }

void PolylineByClicksView::draw(cairo_t* cr) const {
    if (!this->handler->hasPreview()) {
        return;
    }

    const auto& points = this->handler->getPoints();
    if (points.empty()) {
        return;
    }

    xoj::util::CairoSaveGuard saveGuard(cr);
    cairo_set_line_cap(cr, CAIRO_LINE_CAP_ROUND);
    cairo_set_line_join(cr, CAIRO_LINE_JOIN_ROUND);
    cairo_set_line_width(cr, this->handler->getStrokeWidth());
    Util::cairo_set_source_rgbi(cr, this->handler->getStrokeColor());

    cairo_move_to(cr, points.front().x, points.front().y);
    for (auto it = std::next(points.begin()); it != points.end(); ++it) {
        cairo_line_to(cr, it->x, it->y);
    }

    const Point current = this->handler->getCurrentPoint();
    cairo_line_to(cr, current.x, current.y);
    cairo_stroke(cr);
}

bool PolylineByClicksView::isViewOf(const OverlayBase* overlay) const { return overlay == this->handler; }

void PolylineByClicksView::on(PolylineByClicksView::FlagDirtyRegionRequest, const Range& range) {
    this->parent->flagDirtyRegion(range);
}

void PolylineByClicksView::deleteOn(PolylineByClicksView::FinalizationRequest, const Range& range) {
    this->parent->deleteOverlayView(this, range);
}

void PolylineByClicksView::deleteOn(PolylineByClicksView::CancellationRequest, const Range& range) {
    this->parent->deleteOverlayView(this, range);
}
