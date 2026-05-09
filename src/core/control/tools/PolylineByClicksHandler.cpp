/*
 * VertexNote
 *
 * Click-based polyline creation.
 */

#include "PolylineByClicksHandler.h"

#include <iterator>
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
#include "view/overlays/PolylineByClicksView.h"
#include "vertexnote/geometry/GeometryElement.h"
#include "vertexnote/geometry/GeometryIdGenerator.h"
#include "vertexnote/snapping/GeometrySnapProvider.h"
#include "vertexnote/snapping/PageGeometryCollector.h"

namespace {
constexpr double GEOMETRY_SNAP_RADIUS_PIXELS = 8.0;
constexpr double SNAP_INDICATOR_PADDING = 8.0;
}

PolylineByClicksHandler::PolylineByClicksHandler(Control* control, const PageRef& page):
        InputHandler(control, page),
        snappingHandler(control->getSettings()),
        viewPool(std::make_shared<vn::util::DispatchPool<xoj::view::PolylineByClicksView>>()) {
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

PolylineByClicksHandler::~PolylineByClicksHandler() = default;

auto PolylineByClicksHandler::onMotionNotifyEvent(const PositionInputData& pos, double zoom) -> bool {
    if (this->points.empty() || this->done) {
        return false;
    }

    Range repaintRange = previewRange();
    updateCurrentPoint(pos, zoom);
    repaintRange = repaintRange.unite(previewRange());
    this->viewPool->dispatch(xoj::view::PolylineByClicksView::FLAG_DIRTY_REGION, repaintRange);
    return true;
}

auto PolylineByClicksHandler::onKeyPressEvent(const KeyEvent&) -> bool { return false; }

auto PolylineByClicksHandler::onKeyReleaseEvent(const KeyEvent& event) -> bool {
    if (this->done) {
        return false;
    }

    if (event.keyval == GDK_KEY_Escape) {
        cancel();
        return true;
    }

    if (event.keyval == GDK_KEY_Return || event.keyval == GDK_KEY_KP_Enter) {
        finalizePolyline();
        return true;
    }

    return false;
}

void PolylineByClicksHandler::onButtonReleaseEvent(const PositionInputData&, double) {}

void PolylineByClicksHandler::onButtonPressEvent(const PositionInputData& pos, double zoom) {
    if (this->done) {
        return;
    }

    updateCurrentPoint(pos, zoom);
    addCurrentPoint();
}

void PolylineByClicksHandler::onButtonDoublePressEvent(const PositionInputData&, double) { finalizePolyline(); }

void PolylineByClicksHandler::onSequenceCancelEvent() { cancel(); }

auto PolylineByClicksHandler::createView(xoj::view::Repaintable* parent) const
        -> std::unique_ptr<xoj::view::OverlayView> {
    return std::make_unique<xoj::view::PolylineByClicksView>(this, parent);
}

auto PolylineByClicksHandler::isDone() const -> bool { return this->done; }

auto PolylineByClicksHandler::acceptsAdditionalPress() const -> bool { return true; }

auto PolylineByClicksHandler::hasPreview() const -> bool { return !this->points.empty() && !this->done; }

auto PolylineByClicksHandler::getPoints() const -> const std::vector<Point>& { return this->points; }

auto PolylineByClicksHandler::getCurrentPoint() const -> Point { return this->currentPoint; }

auto PolylineByClicksHandler::getCurrentSnapKind() const -> std::optional<vn::snap::SnapKind> {
    return this->currentSnapKind;
}

auto PolylineByClicksHandler::getStrokeWidth() const -> double { return this->strokeWidth; }

auto PolylineByClicksHandler::getStrokeColor() const -> Color { return this->strokeColor; }

auto PolylineByClicksHandler::getViewPool() const
        -> const std::shared_ptr<vn::util::DispatchPool<xoj::view::PolylineByClicksView>>& {
    return this->viewPool;
}

auto PolylineByClicksHandler::previewRange() const -> Range {
    if (this->points.empty()) {
        return Range();
    }

    Range range(this->points.front().x, this->points.front().y);
    for (const auto& point: this->points) {
        range.addPoint(point.x, point.y);
    }
    range.addPoint(this->currentPoint.x, this->currentPoint.y);
    range.addPadding(this->strokeWidth + SNAP_INDICATOR_PADDING);
    return range;
}

void PolylineByClicksHandler::updateCurrentPoint(const PositionInputData& pos, double zoom) {
    Point pagePoint(pos.x / zoom, pos.y / zoom);
    this->currentPoint = snapPoint(pagePoint, pos.isAltDown(), zoom);
}

auto PolylineByClicksHandler::snapPoint(const Point& pagePoint, bool alt, double zoom) -> Point {
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

void PolylineByClicksHandler::addCurrentPoint() {
    if (!this->points.empty() && !validMotion(this->points.back(), this->currentPoint)) {
        return;
    }

    Range repaintRange = previewRange();
    this->points.push_back(this->currentPoint);
    repaintRange = repaintRange.unite(previewRange());
    this->viewPool->dispatch(xoj::view::PolylineByClicksView::FLAG_DIRTY_REGION, repaintRange);
}

void PolylineByClicksHandler::finalizePolyline() {
    if (this->points.size() < 2U) {
        cancel();
        return;
    }

    vn::geom::GeometryObject object(vn::geom::GeometryIdGenerator::nextObjectId());
    std::vector<vn::geom::VertexId> vertices;
    vertices.reserve(this->points.size());
    for (const auto& point: this->points) {
        vertices.push_back(object.addVertex({point.x, point.y}));
    }

    for (auto it = std::next(vertices.begin()); it != vertices.end(); ++it) {
        object.addLine(*std::prev(it), *it);
    }

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
    this->points.clear();
    this->viewPool->dispatchAndClear(xoj::view::PolylineByClicksView::FINALIZATION_REQUEST, range);
    this->page->fireElementChanged(ptr);
}

void PolylineByClicksHandler::cancel() {
    const Range range = previewRange();
    this->done = true;
    this->points.clear();
    this->viewPool->dispatchAndClear(xoj::view::PolylineByClicksView::CANCELLATION_REQUEST, range);
}
