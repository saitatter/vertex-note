/*
 * VertexNote
 *
 * Overlay for click-based circle creation.
 */

#include "CircleByCenterRadiusView.h"

#include <cmath>

#include <cairo.h>

#include "control/tools/CircleByCenterRadiusHandler.h"
#include "util/Color.h"
#include "util/raii/CairoWrappers.h"
#include "view/Repaintable.h"
#include "view/overlays/SnapIndicatorViewHelper.h"

using namespace vn::view;

CircleByCenterRadiusView::CircleByCenterRadiusView(const CircleByCenterRadiusHandler* handler, Repaintable* parent):
        ToolView(parent), handler(handler) {
    this->registerToPool(handler->getViewPool());
}

CircleByCenterRadiusView::~CircleByCenterRadiusView() noexcept { this->unregisterFromPool(); }

void CircleByCenterRadiusView::draw(cairo_t* cr) const {
    if (!this->handler->hasPreview()) {
        return;
    }

    const Point center = this->handler->getCenterPoint();
    const Point current = this->handler->getCurrentPoint();
    const double radius = std::hypot(current.x - center.x, current.y - center.y);

    vn::util::CairoSaveGuard saveGuard(cr);
    cairo_set_line_cap(cr, CAIRO_LINE_CAP_ROUND);
    cairo_set_line_join(cr, CAIRO_LINE_JOIN_ROUND);
    cairo_set_line_width(cr, this->handler->getStrokeWidth());
    Util::cairo_set_source_rgbi(cr, this->handler->getStrokeColor());
    cairo_arc(cr, center.x, center.y, radius, 0.0, 2.0 * M_PI);
    cairo_stroke(cr);
    drawSnapIndicator(cr, current, this->handler->getCurrentSnapKind());
}

bool CircleByCenterRadiusView::isViewOf(const OverlayBase* overlay) const { return overlay == this->handler; }

void CircleByCenterRadiusView::on(CircleByCenterRadiusView::FlagDirtyRegionRequest, const Range& range) {
    this->parent->flagDirtyRegion(range);
}

void CircleByCenterRadiusView::deleteOn(CircleByCenterRadiusView::FinalizationRequest, const Range& range) {
    this->parent->deleteOverlayView(this, range);
}

void CircleByCenterRadiusView::deleteOn(CircleByCenterRadiusView::CancellationRequest, const Range& range) {
    this->parent->deleteOverlayView(this, range);
}
