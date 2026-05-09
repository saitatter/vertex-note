/*
 * VertexNote
 *
 * Click-based construction circle creation from center and radius.
 */

#include "ConstructionCircleByCenterRadiusHandler.h"

#include <cmath>
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
#include "view/overlays/ConstructionCircleByCenterRadiusView.h"
#include "vertexnote/geometry/GeometryElement.h"
#include "vertexnote/geometry/GeometryIdGenerator.h"
#include "vertexnote/snapping/GeometrySnapProvider.h"
#include "vertexnote/snapping/PageGeometryCollector.h"

namespace {
constexpr double GEOMETRY_SNAP_RADIUS_PIXELS = 8.0;
constexpr double SNAP_INDICATOR_PADDING = 8.0;
}

ConstructionCircleByCenterRadiusHandler::ConstructionCircleByCenterRadiusHandler(Control* control, const PageRef& page):
        InputHandler(control, page),
        snappingHandler(control->getSettings()),
        viewPool(std::make_shared<vn::util::DispatchPool<vn::view::ConstructionCircleByCenterRadiusView>>()) {
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

ConstructionCircleByCenterRadiusHandler::~ConstructionCircleByCenterRadiusHandler() = default;

auto ConstructionCircleByCenterRadiusHandler::onMotionNotifyEvent(const PositionInputData& pos, double zoom) -> bool {
    if (!this->centerPoint || this->done) {
        return false;
    }

    Range repaintRange = previewRange();
    updateCurrentPoint(pos, zoom);
    repaintRange = repaintRange.unite(previewRange());
    this->viewPool->dispatch(vn::view::ConstructionCircleByCenterRadiusView::FLAG_DIRTY_REGION, repaintRange);
    return true;
}

auto ConstructionCircleByCenterRadiusHandler::onKeyPressEvent(const KeyEvent&) -> bool { return false; }

auto ConstructionCircleByCenterRadiusHandler::onKeyReleaseEvent(const KeyEvent& event) -> bool {
    if (event.keyval != GDK_KEY_Escape || this->done) {
        return false;
    }

    cancel();
    return true;
}

void ConstructionCircleByCenterRadiusHandler::onButtonReleaseEvent(const PositionInputData&, double) {}

void ConstructionCircleByCenterRadiusHandler::onButtonPressEvent(const PositionInputData& pos, double zoom) {
    if (this->done) {
        return;
    }

    updateCurrentPoint(pos, zoom);
    if (!this->centerPoint) {
        this->centerPoint = this->currentPoint;
        this->viewPool->dispatch(vn::view::ConstructionCircleByCenterRadiusView::FLAG_DIRTY_REGION, previewRange());
        return;
    }

    finalizeCircle();
}

void ConstructionCircleByCenterRadiusHandler::onButtonDoublePressEvent(const PositionInputData&, double) {}

void ConstructionCircleByCenterRadiusHandler::onSequenceCancelEvent() { cancel(); }

auto ConstructionCircleByCenterRadiusHandler::createView(vn::view::Repaintable* parent) const
        -> std::unique_ptr<vn::view::OverlayView> {
    return std::make_unique<vn::view::ConstructionCircleByCenterRadiusView>(this, parent);
}

auto ConstructionCircleByCenterRadiusHandler::isDone() const -> bool { return this->done; }

auto ConstructionCircleByCenterRadiusHandler::acceptsAdditionalPress() const -> bool { return true; }

auto ConstructionCircleByCenterRadiusHandler::hasPreview() const -> bool {
    return this->centerPoint.has_value() && !this->done;
}

auto ConstructionCircleByCenterRadiusHandler::getCenterPoint() const -> Point {
    return this->centerPoint.value_or(this->currentPoint);
}

auto ConstructionCircleByCenterRadiusHandler::getCurrentPoint() const -> Point { return this->currentPoint; }

auto ConstructionCircleByCenterRadiusHandler::getCurrentSnapKind() const -> std::optional<vn::snap::SnapKind> {
    return this->currentSnapKind;
}

auto ConstructionCircleByCenterRadiusHandler::getStrokeWidth() const -> double { return this->strokeWidth; }

auto ConstructionCircleByCenterRadiusHandler::getStrokeColor() const -> Color { return this->strokeColor; }

auto ConstructionCircleByCenterRadiusHandler::getViewPool() const
        -> const std::shared_ptr<vn::util::DispatchPool<vn::view::ConstructionCircleByCenterRadiusView>>& {
    return this->viewPool;
}

auto ConstructionCircleByCenterRadiusHandler::previewRange() const -> Range {
    if (!this->centerPoint) {
        return Range();
    }

    const double radius =
            std::hypot(this->currentPoint.x - this->centerPoint->x, this->currentPoint.y - this->centerPoint->y);
    Range range(this->centerPoint->x - radius, this->centerPoint->y - radius);
    range.addPoint(this->centerPoint->x + radius, this->centerPoint->y + radius);
    range.addPadding(this->strokeWidth + SNAP_INDICATOR_PADDING);
    return range;
}

void ConstructionCircleByCenterRadiusHandler::updateCurrentPoint(const PositionInputData& pos, double zoom) {
    Point pagePoint(pos.x / zoom, pos.y / zoom);
    this->currentPoint = snapPoint(pagePoint, pos.isAltDown(), zoom);
}

auto ConstructionCircleByCenterRadiusHandler::snapPoint(const Point& pagePoint, bool alt, double zoom) -> Point {
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

void ConstructionCircleByCenterRadiusHandler::finalizeCircle() {
    if (!this->centerPoint || !validMotion(*this->centerPoint, this->currentPoint)) {
        cancel();
        return;
    }

    vn::geom::GeometryObject object(vn::geom::GeometryIdGenerator::nextObjectId());
    const auto center = object.addVertex({this->centerPoint->x, this->centerPoint->y});
    const auto radiusPoint = object.addVertex({this->currentPoint.x, this->currentPoint.y});
    object.addEdge(vn::geom::EdgeKind::ConstructionCircle, radiusPoint, radiusPoint, {center});

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
    this->centerPoint.reset();
    this->viewPool->dispatchAndClear(vn::view::ConstructionCircleByCenterRadiusView::FINALIZATION_REQUEST, range);
    this->page->fireElementChanged(ptr);
}

void ConstructionCircleByCenterRadiusHandler::cancel() {
    const Range range = previewRange();
    this->done = true;
    this->centerPoint.reset();
    this->viewPool->dispatchAndClear(vn::view::ConstructionCircleByCenterRadiusView::CANCELLATION_REQUEST, range);
}
