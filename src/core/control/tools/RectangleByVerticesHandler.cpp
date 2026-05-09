/*
 * VertexNote
 *
 * Click-based vertex rectangle creation.
 */

#include "RectangleByVerticesHandler.h"

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
#include "view/overlays/RectangleByVerticesView.h"
#include "vertexnote/geometry/GeometryElement.h"
#include "vertexnote/geometry/GeometryIdGenerator.h"
#include "vertexnote/snapping/GeometrySnapProvider.h"
#include "vertexnote/snapping/PageGeometryCollector.h"

namespace {
constexpr double GEOMETRY_SNAP_RADIUS_PIXELS = 8.0;
constexpr double SNAP_INDICATOR_PADDING = 8.0;
}

RectangleByVerticesHandler::RectangleByVerticesHandler(Control* control, const PageRef& page):
        InputHandler(control, page),
        snappingHandler(control->getSettings()),
        viewPool(std::make_shared<xoj::util::DispatchPool<xoj::view::RectangleByVerticesView>>()) {
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

RectangleByVerticesHandler::~RectangleByVerticesHandler() = default;

auto RectangleByVerticesHandler::onMotionNotifyEvent(const PositionInputData& pos, double zoom) -> bool {
    if (!this->startPoint || this->done) {
        return false;
    }

    Range repaintRange = previewRange();
    updateCurrentPoint(pos, zoom);
    repaintRange = repaintRange.unite(previewRange());
    this->viewPool->dispatch(xoj::view::RectangleByVerticesView::FLAG_DIRTY_REGION, repaintRange);
    return true;
}

auto RectangleByVerticesHandler::onKeyPressEvent(const KeyEvent&) -> bool { return false; }

auto RectangleByVerticesHandler::onKeyReleaseEvent(const KeyEvent& event) -> bool {
    if (event.keyval != GDK_KEY_Escape || this->done) {
        return false;
    }

    cancel();
    return true;
}

void RectangleByVerticesHandler::onButtonReleaseEvent(const PositionInputData&, double) {}

void RectangleByVerticesHandler::onButtonPressEvent(const PositionInputData& pos, double zoom) {
    if (this->done) {
        return;
    }

    updateCurrentPoint(pos, zoom);
    if (!this->startPoint) {
        this->startPoint = this->currentPoint;
        this->viewPool->dispatch(xoj::view::RectangleByVerticesView::FLAG_DIRTY_REGION, previewRange());
        return;
    }

    finalizeRectangle();
}

void RectangleByVerticesHandler::onButtonDoublePressEvent(const PositionInputData&, double) {}

void RectangleByVerticesHandler::onSequenceCancelEvent() { cancel(); }

auto RectangleByVerticesHandler::createView(xoj::view::Repaintable* parent) const
        -> std::unique_ptr<xoj::view::OverlayView> {
    return std::make_unique<xoj::view::RectangleByVerticesView>(this, parent);
}

auto RectangleByVerticesHandler::isDone() const -> bool { return this->done; }

auto RectangleByVerticesHandler::acceptsAdditionalPress() const -> bool { return true; }

auto RectangleByVerticesHandler::hasPreview() const -> bool { return this->startPoint.has_value() && !this->done; }

auto RectangleByVerticesHandler::getStartPoint() const -> Point { return this->startPoint.value_or(this->currentPoint); }

auto RectangleByVerticesHandler::getCurrentPoint() const -> Point { return this->currentPoint; }

auto RectangleByVerticesHandler::getCurrentSnapKind() const -> std::optional<vn::snap::SnapKind> {
    return this->currentSnapKind;
}

auto RectangleByVerticesHandler::getStrokeWidth() const -> double { return this->strokeWidth; }

auto RectangleByVerticesHandler::getStrokeColor() const -> Color { return this->strokeColor; }

auto RectangleByVerticesHandler::getViewPool() const
        -> const std::shared_ptr<xoj::util::DispatchPool<xoj::view::RectangleByVerticesView>>& {
    return this->viewPool;
}

auto RectangleByVerticesHandler::previewRange() const -> Range {
    if (!this->startPoint) {
        return Range();
    }

    Range range(this->startPoint->x, this->startPoint->y);
    range.addPoint(this->currentPoint.x, this->currentPoint.y);
    range.addPadding(this->strokeWidth + SNAP_INDICATOR_PADDING);
    return range;
}

void RectangleByVerticesHandler::updateCurrentPoint(const PositionInputData& pos, double zoom) {
    Point pagePoint(pos.x / zoom, pos.y / zoom);
    this->currentPoint = snapPoint(pagePoint, pos.isAltDown(), zoom);
}

auto RectangleByVerticesHandler::snapPoint(const Point& pagePoint, bool alt, double zoom) -> Point {
    this->currentSnapKind.reset();
    if (this->geometrySnapEnabled) {
        const auto geometrySnap = this->snapEngine.snap(
                vn::snap::SnapQuery{{pagePoint.x, pagePoint.y}, zoom, GEOMETRY_SNAP_RADIUS_PIXELS});
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

void RectangleByVerticesHandler::finalizeRectangle() {
    if (!this->startPoint || !validMotion(*this->startPoint, this->currentPoint)) {
        cancel();
        return;
    }

    vn::geom::GeometryObject object(vn::geom::GeometryIdGenerator::nextObjectId());
    auto topLeft = object.addVertex({this->startPoint->x, this->startPoint->y});
    auto topRight = object.addVertex({this->currentPoint.x, this->startPoint->y});
    auto bottomRight = object.addVertex({this->currentPoint.x, this->currentPoint.y});
    auto bottomLeft = object.addVertex({this->startPoint->x, this->currentPoint.y});
    object.addLine(topLeft, topRight);
    object.addLine(topRight, bottomRight);
    object.addLine(bottomRight, bottomLeft);
    object.addLine(bottomLeft, topLeft);

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
    this->viewPool->dispatchAndClear(xoj::view::RectangleByVerticesView::FINALIZATION_REQUEST, range);
    this->page->fireElementChanged(ptr);
}

void RectangleByVerticesHandler::cancel() {
    const Range range = previewRange();
    this->done = true;
    this->startPoint.reset();
    this->viewPool->dispatchAndClear(xoj::view::RectangleByVerticesView::CANCELLATION_REQUEST, range);
}
