/*
 * VertexNote
 *
 * Overlay for click-based arc creation.
 */

#include "ArcByCenterStartEndView.h"

#include <cmath>

#include <cairo.h>

#include "control/tools/ArcByCenterStartEndHandler.h"
#include "util/Color.h"
#include "util/raii/CairoWrappers.h"
#include "view/Repaintable.h"
#include "view/overlays/SnapIndicatorViewHelper.h"

using namespace vn::view;

ArcByCenterStartEndView::ArcByCenterStartEndView(const ArcByCenterStartEndHandler* handler, Repaintable* parent):
        ToolView(parent), handler(handler) {
    this->registerToPool(handler->getViewPool());
}

ArcByCenterStartEndView::~ArcByCenterStartEndView() noexcept { this->unregisterFromPool(); }

void ArcByCenterStartEndView::draw(cairo_t* cr) const {
    if (!this->handler->hasPreview()) {
        return;
    }

    const Point center = this->handler->getCenterPoint();
    const Point current = this->handler->getCurrentPoint();

    vn::util::CairoSaveGuard saveGuard(cr);
    cairo_set_line_cap(cr, CAIRO_LINE_CAP_ROUND);
    cairo_set_line_join(cr, CAIRO_LINE_JOIN_ROUND);
    cairo_set_line_width(cr, this->handler->getStrokeWidth());
    Util::cairo_set_source_rgbi(cr, this->handler->getStrokeColor());

    if (!this->handler->hasStartPoint()) {
        cairo_move_to(cr, center.x, center.y);
        cairo_line_to(cr, current.x, current.y);
        cairo_stroke(cr);
        drawSnapIndicator(cr, current, this->handler->getCurrentSnapKind());
        return;
    }

    const Point start = this->handler->getStartPoint();
    const double radius = std::hypot(start.x - center.x, start.y - center.y);
    const double startAngle = std::atan2(start.y - center.y, start.x - center.x);
    double endAngle = std::atan2(current.y - center.y, current.x - center.x);
    if (endAngle <= startAngle) {
        endAngle += 2.0 * M_PI;
    }

    cairo_arc(cr, center.x, center.y, radius, startAngle, endAngle);
    cairo_stroke(cr);
    drawSnapIndicator(cr, current, this->handler->getCurrentSnapKind());
}

bool ArcByCenterStartEndView::isViewOf(const OverlayBase* overlay) const { return overlay == this->handler; }

void ArcByCenterStartEndView::on(ArcByCenterStartEndView::FlagDirtyRegionRequest, const Range& range) {
    this->parent->flagDirtyRegion(range);
}

void ArcByCenterStartEndView::deleteOn(ArcByCenterStartEndView::FinalizationRequest, const Range& range) {
    this->parent->deleteOverlayView(this, range);
}

void ArcByCenterStartEndView::deleteOn(ArcByCenterStartEndView::CancellationRequest, const Range& range) {
    this->parent->deleteOverlayView(this, range);
}
