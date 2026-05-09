/*
 * VertexNote
 *
 * Click-based construction line creation.
 */

#include "ConstructionLineByClicksHandler.h"

#include <memory>
#include <utility>

#include <gdk/gdkkeysyms.h>

#include "control/Control.h"
#include "control/ToolHandler.h"
#include "control/settings/Settings.h"
#include "gui/inputdevices/InputEvents.h"
#include "gui/inputdevices/PositionInputData.h"
#include "model/Document.h"
#include "model/Layer.h"
#include "model/NotePage.h"
#include "undo/InsertUndoAction.h"
#include "undo/UndoRedoHandler.h"
#include "util/DispatchPool.h"
#include "view/overlays/ConstructionLineByClicksView.h"
#include "vertexnote/geometry/GeometryElement.h"
#include "vertexnote/geometry/GeometryIdGenerator.h"
#include "vertexnote/snapping/GeometrySnapProvider.h"
#include "vertexnote/snapping/PageGeometryCollector.h"

namespace {
constexpr double GEOMETRY_SNAP_RADIUS_PIXELS = 8.0;
constexpr double SNAP_INDICATOR_PADDING = 8.0;
}

ConstructionLineByClicksHandler::ConstructionLineByClicksHandler(Control* control, const PageRef& page):
        InputHandler(control, page),
        snappingHandler(control->getSettings()),
        viewPool(std::make_shared<vn::util::DispatchPool<vn::view::ConstructionLineByClicksView>>()) {
    this->snappingHandler.setPageRef(page);
    const auto* settings = control->getSettings();
    this->geometrySnapEnabled = settings->isVertexNoteGeometrySnapEnabled();
    this->gridSnapEnabled = settings->isVertexNoteGridSnapEnabled();

    if (this->geometrySnapEnabled) {
        this->snapEngine.addProvider(
                std::make_shared<vn::snap::GeometrySnapProvider>(vn::snap::collectGeometryObjects(page)));
    }

    const auto* toolHandler = control->getToolHandler();
    this->strokeWidth = toolHandler->getThickness();
    this->strokeColor = toolHandler->getColor();
}

ConstructionLineByClicksHandler::~ConstructionLineByClicksHandler() = default;

auto ConstructionLineByClicksHandler::onMotionNotifyEvent(const PositionInputData& pos, double zoom) -> bool {
    if (!this->startPoint || this->done) {
        return false;
    }

    Range repaintRange = previewRange();
    updateCurrentPoint(pos, zoom);
    repaintRange = repaintRange.unite(previewRange());
    this->viewPool->dispatch(vn::view::ConstructionLineByClicksView::FLAG_DIRTY_REGION, repaintRange);
    return true;
}

auto ConstructionLineByClicksHandler::onKeyPressEvent(const KeyEvent&) -> bool { return false; }

auto ConstructionLineByClicksHandler::onKeyReleaseEvent(const KeyEvent& event) -> bool {
    if (event.keyval != GDK_KEY_Escape || this->done) {
        return false;
    }

    cancel();
    return true;
}

void ConstructionLineByClicksHandler::onButtonReleaseEvent(const PositionInputData&, double) {}

void ConstructionLineByClicksHandler::onButtonPressEvent(const PositionInputData& pos, double zoom) {
    if (this->done) {
        return;
    }

    updateCurrentPoint(pos, zoom);
    if (!this->startPoint) {
        this->startPoint = this->currentPoint;
        this->viewPool->dispatch(vn::view::ConstructionLineByClicksView::FLAG_DIRTY_REGION, previewRange());
        return;
    }

    finalizeConstructionLine();
}

void ConstructionLineByClicksHandler::onButtonDoublePressEvent(const PositionInputData&, double) {}

void ConstructionLineByClicksHandler::onSequenceCancelEvent() { cancel(); }

auto ConstructionLineByClicksHandler::createView(vn::view::Repaintable* parent) const
        -> std::unique_ptr<vn::view::OverlayView> {
    return std::make_unique<vn::view::ConstructionLineByClicksView>(this, parent);
}

auto ConstructionLineByClicksHandler::isDone() const -> bool { return this->done; }

auto ConstructionLineByClicksHandler::acceptsAdditionalPress() const -> bool { return true; }

auto ConstructionLineByClicksHandler::hasPreview() const -> bool { return this->startPoint.has_value() && !this->done; }

auto ConstructionLineByClicksHandler::getStartPoint() const -> Point {
    return this->startPoint.value_or(this->currentPoint);
}

auto ConstructionLineByClicksHandler::getCurrentPoint() const -> Point { return this->currentPoint; }

auto ConstructionLineByClicksHandler::getCurrentSnapKind() const -> std::optional<vn::snap::SnapKind> {
    return this->currentSnapKind;
}

auto ConstructionLineByClicksHandler::getStrokeWidth() const -> double { return this->strokeWidth; }

auto ConstructionLineByClicksHandler::getStrokeColor() const -> Color { return this->strokeColor; }

auto ConstructionLineByClicksHandler::getViewPool() const
        -> const std::shared_ptr<vn::util::DispatchPool<vn::view::ConstructionLineByClicksView>>& {
    return this->viewPool;
}

auto ConstructionLineByClicksHandler::previewRange() const -> Range {
    if (!this->startPoint) {
        return Range();
    }

    Range range(this->startPoint->x, this->startPoint->y);
    range.addPoint(this->currentPoint.x, this->currentPoint.y);
    range.addPadding(this->strokeWidth + SNAP_INDICATOR_PADDING);
    return range;
}

void ConstructionLineByClicksHandler::updateCurrentPoint(const PositionInputData& pos, double zoom) {
    Point pagePoint(pos.x / zoom, pos.y / zoom);
    this->currentPoint = snapPoint(pagePoint, pos.isAltDown(), zoom);
}

auto ConstructionLineByClicksHandler::snapPoint(const Point& pagePoint, bool alt, double zoom) -> Point {
    this->currentSnapKind.reset();
    if (this->geometrySnapEnabled) {
        const auto geometrySnap =
                this->snapEngine.snap(vn::snap::SnapQuery{{pagePoint.x, pagePoint.y}, zoom, GEOMETRY_SNAP_RADIUS_PIXELS});
        if (geometrySnap.snapped()) {
            this->currentSnapKind = geometrySnap.candidate->kind;
            return Point(geometrySnap.pagePoint.x, geometrySnap.pagePoint.y, pagePoint.z);
        }
    }

    if (!this->gridSnapEnabled) {
        return pagePoint;
    }

    Point gridPoint = this->snappingHandler.snapToGrid(pagePoint, alt);
    if (gridPoint.x != pagePoint.x || gridPoint.y != pagePoint.y) {
        this->currentSnapKind = vn::snap::SnapKind::Grid;
    }
    return gridPoint;
}

void ConstructionLineByClicksHandler::finalizeConstructionLine() {
    if (!this->startPoint || !validMotion(*this->startPoint, this->currentPoint)) {
        cancel();
        return;
    }

    vn::geom::GeometryObject object(vn::geom::GeometryIdGenerator::nextObjectId());
    auto start = object.addVertex({this->startPoint->x, this->startPoint->y});
    auto end = object.addVertex({this->currentPoint.x, this->currentPoint.y});
    object.addEdge(vn::geom::EdgeKind::ConstructionLine, start, end);

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
    this->viewPool->dispatchAndClear(vn::view::ConstructionLineByClicksView::FINALIZATION_REQUEST, range);
    this->page->fireElementChanged(ptr);
}

void ConstructionLineByClicksHandler::cancel() {
    const Range range = previewRange();
    this->done = true;
    this->startPoint.reset();
    this->viewPool->dispatchAndClear(vn::view::ConstructionLineByClicksView::CANCELLATION_REQUEST, range);
}
