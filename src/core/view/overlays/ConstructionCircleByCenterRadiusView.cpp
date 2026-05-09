/*
 * VertexNote
 *
 * Overlay for click-based construction circle creation.
 */

#include "ConstructionCircleByCenterRadiusView.h"

#include <array>
#include <cmath>

#include <cairo.h>

#include "control/tools/ConstructionCircleByCenterRadiusHandler.h"
#include "util/Color.h"
#include "util/raii/CairoWrappers.h"
#include "view/Repaintable.h"
#include "view/overlays/SnapIndicatorViewHelper.h"

using namespace vn::view;

namespace {
constexpr std::array<double, 2> ConstructionDash{8.0, 6.0};
constexpr double ConstructionCenterlineInsetRatio = 0.6;

auto drawConstructionCircleHelper(cairo_t* cr, Point center, double radius) -> void {
    const double helperExtent = radius * ConstructionCenterlineInsetRatio;
    cairo_move_to(cr, center.x - helperExtent, center.y);
    cairo_line_to(cr, center.x + helperExtent, center.y);
    cairo_move_to(cr, center.x, center.y - helperExtent);
    cairo_line_to(cr, center.x, center.y + helperExtent);
    cairo_stroke(cr);
}
}  // namespace

ConstructionCircleByCenterRadiusView::ConstructionCircleByCenterRadiusView(
        const ConstructionCircleByCenterRadiusHandler* handler, Repaintable* parent):
        ToolView(parent), handler(handler) {
    this->registerToPool(handler->getViewPool());
}

ConstructionCircleByCenterRadiusView::~ConstructionCircleByCenterRadiusView() noexcept { this->unregisterFromPool(); }

void ConstructionCircleByCenterRadiusView::draw(cairo_t* cr) const {
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
    cairo_set_dash(cr, ConstructionDash.data(), static_cast<int>(ConstructionDash.size()), 0.0);
    Util::cairo_set_source_rgbi(cr, this->handler->getStrokeColor());
    cairo_arc(cr, center.x, center.y, radius, 0.0, 2.0 * M_PI);
    cairo_stroke(cr);
    drawConstructionCircleHelper(cr, center, radius);
    drawSnapIndicator(cr, current, this->handler->getCurrentSnapKind());
}

bool ConstructionCircleByCenterRadiusView::isViewOf(const OverlayBase* overlay) const { return overlay == this->handler; }

void ConstructionCircleByCenterRadiusView::on(FlagDirtyRegionRequest, const Range& range) {
    this->parent->flagDirtyRegion(range);
}

void ConstructionCircleByCenterRadiusView::deleteOn(FinalizationRequest, const Range& range) {
    this->parent->deleteOverlayView(this, range);
}

void ConstructionCircleByCenterRadiusView::deleteOn(CancellationRequest, const Range& range) {
    this->parent->deleteOverlayView(this, range);
}
