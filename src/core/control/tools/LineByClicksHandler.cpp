/*
 * VertexNote
 *
 * Click-based line creation.
 */

#include "LineByClicksHandler.h"

#include <memory>
#include <utility>

#include <gdk/gdkkeysyms.h>

#include "control/Control.h"
#include "control/ToolHandler.h"
#include "gui/inputdevices/InputEvents.h"
#include "gui/inputdevices/PositionInputData.h"
#include "model/Document.h"
#include "model/Layer.h"
#include "model/XojPage.h"
#include "undo/InsertUndoAction.h"
#include "undo/UndoRedoHandler.h"
#include "util/DispatchPool.h"
#include "view/overlays/LineByClicksView.h"
#include "vertexnote/geometry/GeometryElement.h"

LineByClicksHandler::LineByClicksHandler(Control* control, const PageRef& page):
        InputHandler(control, page),
        snappingHandler(control->getSettings()),
        viewPool(std::make_shared<xoj::util::DispatchPool<xoj::view::LineByClicksView>>()) {
    this->snappingHandler.setPageRef(page);
    const auto* toolHandler = control->getToolHandler();
    this->strokeWidth = toolHandler->getThickness();
    this->strokeColor = toolHandler->getColor();
}

LineByClicksHandler::~LineByClicksHandler() = default;

auto LineByClicksHandler::onMotionNotifyEvent(const PositionInputData& pos, double zoom) -> bool {
    if (!this->startPoint || this->done) {
        return false;
    }

    Range repaintRange = previewRange();
    updateCurrentPoint(pos, zoom);
    repaintRange = repaintRange.unite(previewRange());
    this->viewPool->dispatch(xoj::view::LineByClicksView::FLAG_DIRTY_REGION, repaintRange);
    return true;
}

auto LineByClicksHandler::onKeyPressEvent(const KeyEvent&) -> bool { return false; }

auto LineByClicksHandler::onKeyReleaseEvent(const KeyEvent& event) -> bool {
    if (event.keyval != GDK_KEY_Escape || this->done) {
        return false;
    }

    cancel();
    return true;
}

void LineByClicksHandler::onButtonReleaseEvent(const PositionInputData&, double) {}

void LineByClicksHandler::onButtonPressEvent(const PositionInputData& pos, double zoom) {
    if (this->done) {
        return;
    }

    updateCurrentPoint(pos, zoom);
    if (!this->startPoint) {
        this->startPoint = this->currentPoint;
        this->viewPool->dispatch(xoj::view::LineByClicksView::FLAG_DIRTY_REGION, previewRange());
        return;
    }

    finalizeLine();
}

void LineByClicksHandler::onButtonDoublePressEvent(const PositionInputData&, double) {}

void LineByClicksHandler::onSequenceCancelEvent() { cancel(); }

auto LineByClicksHandler::createView(xoj::view::Repaintable* parent) const
        -> std::unique_ptr<xoj::view::OverlayView> {
    return std::make_unique<xoj::view::LineByClicksView>(this, parent);
}

auto LineByClicksHandler::isDone() const -> bool { return this->done; }

auto LineByClicksHandler::acceptsAdditionalPress() const -> bool { return true; }

auto LineByClicksHandler::hasPreview() const -> bool { return this->startPoint.has_value() && !this->done; }

auto LineByClicksHandler::getStartPoint() const -> Point { return this->startPoint.value_or(this->currentPoint); }

auto LineByClicksHandler::getCurrentPoint() const -> Point { return this->currentPoint; }

auto LineByClicksHandler::getStrokeWidth() const -> double { return this->strokeWidth; }

auto LineByClicksHandler::getStrokeColor() const -> Color { return this->strokeColor; }

auto LineByClicksHandler::getViewPool() const
        -> const std::shared_ptr<xoj::util::DispatchPool<xoj::view::LineByClicksView>>& {
    return this->viewPool;
}

auto LineByClicksHandler::previewRange() const -> Range {
    if (!this->startPoint) {
        return Range();
    }

    Range range(this->startPoint->x, this->startPoint->y);
    range.addPoint(this->currentPoint.x, this->currentPoint.y);
    range.addPadding(this->strokeWidth);
    return range;
}

void LineByClicksHandler::updateCurrentPoint(const PositionInputData& pos, double zoom) {
    Point pagePoint(pos.x / zoom, pos.y / zoom);
    this->currentPoint = this->snappingHandler.snapToGrid(pagePoint, pos.isAltDown());
}

void LineByClicksHandler::finalizeLine() {
    if (!this->startPoint || !validMotion(*this->startPoint, this->currentPoint)) {
        cancel();
        return;
    }

    vn::geom::GeometryObject object;
    auto start = object.addVertex({this->startPoint->x, this->startPoint->y});
    auto end = object.addVertex({this->currentPoint.x, this->currentPoint.y});
    object.addLine(start, end);

    auto geometry = std::make_unique<vn::geom::GeometryElement>(std::move(object));
    geometry->setColor(this->strokeColor);
    geometry->setStrokeWidth(this->strokeWidth);

    Layer* layer = this->page->getSelectedLayer();
    auto* undo = this->control->getUndoRedoHandler();
    undo->addUndoAction(std::make_unique<InsertUndoAction>(this->page, layer, geometry.get()));

    auto* ptr = geometry.get();
    Document* doc = this->control->getDocument();
    doc->lock();
    layer->addElement(std::move(geometry));
    doc->unlock();

    const Range range = previewRange();
    this->done = true;
    this->startPoint.reset();
    this->viewPool->dispatchAndClear(xoj::view::LineByClicksView::FINALIZATION_REQUEST, range);
    this->page->fireElementChanged(ptr);
}

void LineByClicksHandler::cancel() {
    const Range range = previewRange();
    this->done = true;
    this->startPoint.reset();
    this->viewPool->dispatchAndClear(xoj::view::LineByClicksView::CANCELLATION_REQUEST, range);
}
