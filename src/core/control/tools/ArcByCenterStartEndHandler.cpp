/*
 * VertexNote
 *
 * Click-based arc creation from center, start, and end.
 */

#include "ArcByCenterStartEndHandler.h"

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
#include "view/overlays/ArcByCenterStartEndView.h"
#include "vertexnote/geometry/GeometryElement.h"
#include "vertexnote/geometry/GeometryIdGenerator.h"
#include "vertexnote/snapping/GeometrySnapProvider.h"
#include "vertexnote/snapping/PageGeometryCollector.h"

namespace {
constexpr double GEOMETRY_SNAP_RADIUS_PIXELS = 8.0;
constexpr double SNAP_INDICATOR_PADDING = 8.0;
}

ArcByCenterStartEndHandler::ArcByCenterStartEndHandler(Control* control, const PageRef& page):
        InputHandler(control, page),
        snappingHandler(control->getSettings()),
        viewPool(std::make_shared<vn::util::DispatchPool<vn::view::ArcByCenterStartEndView>>()) {
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

ArcByCenterStartEndHandler::~ArcByCenterStartEndHandler() = default;

auto ArcByCenterStartEndHandler::onMotionNotifyEvent(const PositionInputData& pos, double zoom) -> bool {
    if (!this->centerPoint || this->done) {
        return false;
    }

    Range repaintRange = previewRange();
    updateCurrentPoint(pos, zoom);
    repaintRange = repaintRange.unite(previewRange());
    this->viewPool->dispatch(vn::view::ArcByCenterStartEndView::FLAG_DIRTY_REGION, repaintRange);
    return true;
}

auto ArcByCenterStartEndHandler::onKeyPressEvent(const KeyEvent&) -> bool { return false; }

auto ArcByCenterStartEndHandler::onKeyReleaseEvent(const KeyEvent& event) -> bool {
    if (event.keyval != GDK_KEY_Escape || this->done) {
        return false;
    }

    cancel();
    return true;
}

void ArcByCenterStartEndHandler::onButtonReleaseEvent(const PositionInputData&, double) {}

void ArcByCenterStartEndHandler::onButtonPressEvent(const PositionInputData& pos, double zoom) {
    if (this->done) {
        return;
    }

    updateCurrentPoint(pos, zoom);
    if (!this->centerPoint) {
        this->centerPoint = this->currentPoint;
        this->viewPool->dispatch(vn::view::ArcByCenterStartEndView::FLAG_DIRTY_REGION, previewRange());
        return;
    }

    if (!this->startPoint) {
        if (!validMotion(*this->centerPoint, this->currentPoint)) {
            return;
        }
        this->startPoint = this->currentPoint;
        this->viewPool->dispatch(vn::view::ArcByCenterStartEndView::FLAG_DIRTY_REGION, previewRange());
        return;
    }

    finalizeArc();
}

void ArcByCenterStartEndHandler::onButtonDoublePressEvent(const PositionInputData&, double) {}

void ArcByCenterStartEndHandler::onSequenceCancelEvent() { cancel(); }

auto ArcByCenterStartEndHandler::createView(vn::view::Repaintable* parent) const
        -> std::unique_ptr<vn::view::OverlayView> {
    return std::make_unique<vn::view::ArcByCenterStartEndView>(this, parent);
}

auto ArcByCenterStartEndHandler::isDone() const -> bool { return this->done; }

auto ArcByCenterStartEndHandler::acceptsAdditionalPress() const -> bool { return true; }

auto ArcByCenterStartEndHandler::hasPreview() const -> bool { return this->centerPoint.has_value() && !this->done; }

auto ArcByCenterStartEndHandler::hasStartPoint() const -> bool { return this->startPoint.has_value(); }

auto ArcByCenterStartEndHandler::getCenterPoint() const -> Point {
    return this->centerPoint.value_or(this->currentPoint);
}

auto ArcByCenterStartEndHandler::getStartPoint() const -> Point {
    return this->startPoint.value_or(this->currentPoint);
}

auto ArcByCenterStartEndHandler::getCurrentPoint() const -> Point { return this->currentPoint; }

auto ArcByCenterStartEndHandler::getCurrentSnapKind() const -> std::optional<vn::snap::SnapKind> {
    return this->currentSnapKind;
}

auto ArcByCenterStartEndHandler::getStrokeWidth() const -> double { return this->strokeWidth; }

auto ArcByCenterStartEndHandler::getStrokeColor() const -> Color { return this->strokeColor; }

auto ArcByCenterStartEndHandler::getViewPool() const
        -> const std::shared_ptr<vn::util::DispatchPool<vn::view::ArcByCenterStartEndView>>& {
    return this->viewPool;
}

auto ArcByCenterStartEndHandler::previewRange() const -> Range {
    if (!this->centerPoint) {
        return Range();
    }

    const Point radiusReference = this->startPoint.value_or(this->currentPoint);
    const double radius = std::hypot(radiusReference.x - this->centerPoint->x, radiusReference.y - this->centerPoint->y);
    Range range(this->centerPoint->x - radius, this->centerPoint->y - radius);
    range.addPoint(this->centerPoint->x + radius, this->centerPoint->y + radius);
    range.addPadding(this->strokeWidth + SNAP_INDICATOR_PADDING);
    return range;
}

void ArcByCenterStartEndHandler::updateCurrentPoint(const PositionInputData& pos, double zoom) {
    Point pagePoint(pos.x / zoom, pos.y / zoom);
    Point snapped = snapPoint(pagePoint, pos.isAltDown(), zoom);
    this->currentPoint = this->startPoint ? projectToStartRadius(snapped) : snapped;
}

auto ArcByCenterStartEndHandler::snapPoint(const Point& pagePoint, bool alt, double zoom) -> Point {
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

auto ArcByCenterStartEndHandler::projectToStartRadius(const Point& pagePoint) const -> Point {
    if (!this->centerPoint || !this->startPoint) {
        return pagePoint;
    }

    const double radius =
            std::hypot(this->startPoint->x - this->centerPoint->x, this->startPoint->y - this->centerPoint->y);
    const double dx = pagePoint.x - this->centerPoint->x;
    const double dy = pagePoint.y - this->centerPoint->y;
    if (dx == 0.0 && dy == 0.0) {
        return *this->startPoint;
    }

    const double angle = std::atan2(dy, dx);
    return Point(this->centerPoint->x + std::cos(angle) * radius, this->centerPoint->y + std::sin(angle) * radius,
                 pagePoint.z);
}

void ArcByCenterStartEndHandler::finalizeArc() {
    if (!this->centerPoint || !this->startPoint || !validMotion(*this->startPoint, this->currentPoint)) {
        cancel();
        return;
    }

    vn::geom::GeometryObject object(vn::geom::GeometryIdGenerator::nextObjectId());
    const auto center = object.addVertex({this->centerPoint->x, this->centerPoint->y});
    const auto start = object.addVertex({this->startPoint->x, this->startPoint->y});
    const auto end = object.addVertex({this->currentPoint.x, this->currentPoint.y});
    object.addEdge(vn::geom::EdgeKind::Arc, start, end, {center});

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
    this->startPoint.reset();
    this->viewPool->dispatchAndClear(vn::view::ArcByCenterStartEndView::FINALIZATION_REQUEST, range);
    this->page->fireElementChanged(ptr);
}

void ArcByCenterStartEndHandler::cancel() {
    const Range range = previewRange();
    this->done = true;
    this->centerPoint.reset();
    this->startPoint.reset();
    this->viewPool->dispatchAndClear(vn::view::ArcByCenterStartEndView::CANCELLATION_REQUEST, range);
}
