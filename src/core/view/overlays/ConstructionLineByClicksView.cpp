/*
 * VertexNote
 *
 * Overlay for click-based construction line creation.
 */

#include "ConstructionLineByClicksView.h"

#include <array>
#include <cmath>

#include <cairo.h>

#include "control/tools/ConstructionLineByClicksHandler.h"
#include "util/Color.h"
#include "util/raii/CairoWrappers.h"
#include "view/Repaintable.h"
#include "view/overlays/SnapIndicatorViewHelper.h"

using namespace vn::view;

namespace {
constexpr std::array<double, 2> ConstructionDash{8.0, 6.0};

auto drawExtendedPreviewLine(cairo_t* cr, Point start, Point end) -> void {
    double clipMinX = 0.0;
    double clipMinY = 0.0;
    double clipMaxX = 0.0;
    double clipMaxY = 0.0;
    cairo_clip_extents(cr, &clipMinX, &clipMinY, &clipMaxX, &clipMaxY);

    const double dx = end.x - start.x;
    const double dy = end.y - start.y;
    const double length = std::hypot(dx, dy);
    if (length == 0.0) {
        cairo_move_to(cr, start.x, start.y);
        cairo_line_to(cr, end.x, end.y);
        cairo_stroke(cr);
        return;
    }

    const double extent = std::hypot(clipMaxX - clipMinX, clipMaxY - clipMinY) + length;
    const double unitX = dx / length;
    const double unitY = dy / length;
    cairo_move_to(cr, start.x - unitX * extent, start.y - unitY * extent);
    cairo_line_to(cr, start.x + unitX * extent, start.y + unitY * extent);
    cairo_stroke(cr);
}
}

ConstructionLineByClicksView::ConstructionLineByClicksView(const ConstructionLineByClicksHandler* handler,
                                                           Repaintable* parent):
        ToolView(parent), handler(handler) {
    this->registerToPool(handler->getViewPool());
}

ConstructionLineByClicksView::~ConstructionLineByClicksView() noexcept { this->unregisterFromPool(); }

void ConstructionLineByClicksView::draw(cairo_t* cr) const {
    if (!this->handler->hasPreview()) {
        return;
    }

    const Point start = this->handler->getStartPoint();
    const Point current = this->handler->getCurrentPoint();

    vn::util::CairoSaveGuard saveGuard(cr);
    cairo_set_line_cap(cr, CAIRO_LINE_CAP_ROUND);
    cairo_set_line_join(cr, CAIRO_LINE_JOIN_ROUND);
    cairo_set_line_width(cr, this->handler->getStrokeWidth());
    cairo_set_dash(cr, ConstructionDash.data(), static_cast<int>(ConstructionDash.size()), 0.0);
    Util::cairo_set_source_rgbi(cr, this->handler->getStrokeColor());
    drawExtendedPreviewLine(cr, start, current);
    drawSnapIndicator(cr, current, this->handler->getCurrentSnapKind());
}

bool ConstructionLineByClicksView::isViewOf(const OverlayBase* overlay) const { return overlay == this->handler; }

void ConstructionLineByClicksView::on(ConstructionLineByClicksView::FlagDirtyRegionRequest, const Range& range) {
    this->parent->flagDirtyRegion(range);
}

void ConstructionLineByClicksView::deleteOn(ConstructionLineByClicksView::FinalizationRequest, const Range& range) {
    this->parent->deleteOverlayView(this, range);
}

void ConstructionLineByClicksView::deleteOn(ConstructionLineByClicksView::CancellationRequest, const Range& range) {
    this->parent->deleteOverlayView(this, range);
}
