/*
 * VertexNote
 *
 * Qt document controller backed by the shared core model.
 */

#include "QtDocumentController.h"

#include <algorithm>
#include <cctype>
#include <memory>

#include "control/xojfile/LoadHandler.h"
#include "model/Element.h"
#include "model/Document.h"
#include "model/Image.h"
#include "model/Layer.h"
#include "model/NotePage.h"
#include "model/Text.h"
#include "model/Stroke.h"
#include "model/PathParameter.h"
#include "model/eraser/PaddedBox.h"
#include "util/SmallVector.h"
#include "vertexnote/constraints/GeometryConstraintSolver.h"
#include "vertexnote/geometry/GeometryElement.h"
#include "vertexnote/snapping/GeometrySnapProvider.h"
#include "vertexnote/snapping/GridSnapProvider.h"
#include "vertexnote/snapping/ISnapProvider.h"
#include "vertexnote/snapping/PageGeometryCollector.h"
#include "vertexnote/snapping/SnapEngine.h"

QtDocumentController::QtDocumentController() { newBlankDocument(); }

void QtDocumentController::newBlankDocument() {
    this->document = std::make_unique<Document>(&this->documentHandler);
    this->document->lock();
    this->document->addPage(std::make_shared<NotePage>(595.0, 842.0));
    this->document->unlock();
    this->loadedPath.reset();
    clearGeometryHistory();
    clearInteractiveGeometryState();
    rebuildPageSnapshots();
}

auto QtDocumentController::loadFrom(const std::filesystem::path& path, std::string* errorMessage) -> bool {
    try {
        if (isPdfPath(path)) {
            auto loaded = std::make_unique<Document>(&this->documentHandler);
            if (!loaded->readPdf(path, true, false)) {
                if (errorMessage) {
                    *errorMessage = loaded->getLastErrorMsg();
                }
                return false;
            }
            loaded->setFilepath(path);
            this->document = std::move(loaded);
            this->loadedPath = path;
            clearGeometryHistory();
            clearInteractiveGeometryState();
            rebuildPageSnapshots();
            return true;
        }

        LoadHandler loader;
        auto loaded = loader.loadDocument(path);
        this->document = std::move(loaded);
        this->loadedPath = path;
        clearGeometryHistory();
        clearInteractiveGeometryState();
        rebuildPageSnapshots();
        return true;
    } catch (const std::exception& e) {
        if (errorMessage) {
            *errorMessage = e.what();
        }
        return false;
    }
}

auto QtDocumentController::hasDocument() const -> bool { return static_cast<bool>(this->document); }

auto QtDocumentController::pageCount() const -> std::size_t {
    if (!this->document) {
        return 0U;
    }
    this->document->lock_shared();
    const auto count = this->document->getPageCount();
    this->document->unlock_shared();
    return count;
}

auto QtDocumentController::snapshotPages() const -> const std::vector<vn::view::render::PageRenderSnapshot>& {
    return this->pageSnapshots;
}

auto QtDocumentController::sourcePath() const -> const std::optional<std::filesystem::path>& {
    return this->loadedPath;
}

auto QtDocumentController::titleText() const -> std::string {
    if (this->loadedPath) {
        return this->loadedPath->filename().string();
    }
    return "Untitled Document";
}

auto QtDocumentController::hitTestGeometry(std::size_t pageIndex, double pageX, double pageY, double zoom,
                                                       double maxScreenDistance) const
        -> std::optional<QtGeometryHit> {
    if (pageIndex >= this->pageSnapshots.size()) {
        return std::nullopt;
    }

    std::optional<QtGeometryHit> bestHit;
    for (const auto& drawable: this->pageSnapshots[pageIndex].drawables) {
        const auto* geometry = std::get_if<vn::view::render::GeometryRenderModel>(&drawable);
        if (!geometry) {
            continue;
        }

        auto hit = vn::view::render::hitTestGeometry(*geometry, pageX, pageY, zoom, maxScreenDistance);
        if (!hit) {
            continue;
        }

        if (!bestHit || hit->screenDistance < bestHit->hit.screenDistance) {
            bestHit = QtGeometryHit{.pageIndex = pageIndex, .hit = *hit};
        }
    }

    return bestHit;
}

void QtDocumentController::setHoveredGeometry(std::optional<QtGeometryHit> hit) {
    this->hoveredGeometryHit = std::move(hit);
}

void QtDocumentController::setSelectedGeometry(std::optional<QtGeometryHit> hit, bool additive) {
    if (!additive || !hit || hit->hit.type != vn::view::render::GeometryHitType::Vertex || !this->selectedGeometryHit ||
        this->selectedGeometryHit->pageIndex != hit->pageIndex ||
        this->selectedGeometryHit->hit.objectId != hit->hit.objectId) {
        this->selectedGeometryHit = std::move(hit);
        this->selectedGeometryVertexIds.clear();
        if (this->selectedGeometryHit && this->selectedGeometryHit->hit.type == vn::view::render::GeometryHitType::Vertex &&
            this->selectedGeometryHit->hit.vertexId != vn::geom::InvalidVertexId) {
            this->selectedGeometryVertexIds.push_back(this->selectedGeometryHit->hit.vertexId);
        }
        return;
    }

    this->selectedGeometryHit = std::move(hit);
    if (this->selectedGeometryHit && this->selectedGeometryHit->hit.vertexId != vn::geom::InvalidVertexId &&
        std::find(this->selectedGeometryVertexIds.begin(), this->selectedGeometryVertexIds.end(),
                  this->selectedGeometryHit->hit.vertexId) == this->selectedGeometryVertexIds.end()) {
        this->selectedGeometryVertexIds.push_back(this->selectedGeometryHit->hit.vertexId);
    }
}

void QtDocumentController::clearInteractiveGeometryState() {
    this->hoveredGeometryHit.reset();
    this->selectedGeometryHit.reset();
    this->selectedGeometryVertexIds.clear();
    this->geometryDragState.reset();
}

auto QtDocumentController::hoveredGeometry() const -> const std::optional<QtGeometryHit>& {
    return this->hoveredGeometryHit;
}

auto QtDocumentController::selectedGeometry() const -> const std::optional<QtGeometryHit>& {
    return this->selectedGeometryHit;
}

auto QtDocumentController::selectedVertexIds() const -> const std::vector<vn::geom::VertexId>& {
    return this->selectedGeometryVertexIds;
}

auto QtDocumentController::beginGeometryVertexDrag(const QtGeometryHit& hit) -> bool {
    if (hit.hit.type != vn::view::render::GeometryHitType::Vertex) {
        return false;
    }

    const bool preserveSelection = this->selectedGeometryHit && this->selectedGeometryHit->pageIndex == hit.pageIndex &&
                                   this->selectedGeometryHit->hit.objectId == hit.hit.objectId &&
                                   std::find(this->selectedGeometryVertexIds.begin(), this->selectedGeometryVertexIds.end(),
                                             hit.hit.vertexId) != this->selectedGeometryVertexIds.end();
    if (!preserveSelection) {
        setSelectedGeometry(hit);
    }
    this->geometryDragState = QtGeometryDragState{
            .pageIndex = hit.pageIndex,
            .objectId = hit.hit.objectId,
            .vertexId = hit.hit.vertexId,
            .vertexIds = this->selectedGeometryVertexIds.empty() ? std::vector<vn::geom::VertexId>{hit.hit.vertexId}
                                                                 : this->selectedGeometryVertexIds,
            .originalPosition = {hit.hit.point.x, hit.hit.point.y},
            .currentPosition = {hit.hit.point.x, hit.hit.point.y},
            .beforeGeometry = vn::geom::GeometryObject{},
            .snapPoint = {hit.hit.point.x, hit.hit.point.y},
    };

    this->document->lock();
    if (auto* geometry = findMutableGeometryElement(hit.pageIndex, hit.hit.objectId)) {
        this->geometryDragState->beforeGeometry = geometry->geometry();
        this->geometryDragState->originalPositions.reserve(this->geometryDragState->vertexIds.size());
        this->geometryDragState->currentPositions.reserve(this->geometryDragState->vertexIds.size());
        for (auto vertexId: this->geometryDragState->vertexIds) {
            if (const auto* vertex = geometry->geometry().vertex(vertexId)) {
                this->geometryDragState->originalPositions.push_back(vertex->position);
                this->geometryDragState->currentPositions.push_back(vertex->position);
            }
        }
    }
    this->document->unlock();
    return true;
}

auto QtDocumentController::updateGeometryVertexDrag(double pageX, double pageY, double zoom,
                                                               const QtSnapOptions& options) -> bool {
    if (!this->geometryDragState || !this->document) {
        return false;
    }

    bool changed = false;
    this->document->lock();
    auto* geometry = findMutableGeometryElement(this->geometryDragState->pageIndex, this->geometryDragState->objectId);
    auto page = this->document->getPage(this->geometryDragState->pageIndex);
    if (!geometry || !page) {
        this->document->unlock();
        return false;
    }

    vn::geom::Vec2 target{pageX, pageY};
    this->geometryDragState->snapKind.reset();
    this->geometryDragState->snapPoint = target;

    vn::snap::SnapEngine engine;
    bool hasSnapProviders = false;
    if (options.geometryEnabled) {
        auto objects = vn::snap::collectGeometryObjects(page);
        objects.erase(std::remove_if(objects.begin(), objects.end(),
                                     [&](const vn::geom::GeometryObject* object) {
                                         return !object || object->objectId() == this->geometryDragState->objectId;
                                     }),
                      objects.end());

        if (!objects.empty()) {
            engine.addProvider(std::make_shared<vn::snap::GeometrySnapProvider>(std::move(objects)));
            hasSnapProviders = true;
        }
    }

    if (options.gridEnabled) {
        if (auto provider = gridSnapProviderFor(page->getBackgroundType().format)) {
            engine.addProvider(std::move(provider));
            hasSnapProviders = true;
        }
    }

    if (hasSnapProviders) {
        const auto snapResult = engine.snap(vn::snap::SnapQuery{target, zoom, 8.0});
        if (snapResult.snapped()) {
            target = snapResult.pagePoint;
            this->geometryDragState->snapKind =
                    snapResult.candidate ? std::optional<vn::snap::SnapKind>(snapResult.candidate->kind) : std::nullopt;
            this->geometryDragState->snapPoint = snapResult.pagePoint;
        }
    }

    if (this->geometryDragState->vertexIds.size() <= 1U) {
        changed = geometry->setVertexPosition(this->geometryDragState->vertexId, target);
    } else {
        const vn::geom::Vec2 delta{target.x - this->geometryDragState->originalPosition.x,
                                   target.y - this->geometryDragState->originalPosition.y};
        for (std::size_t index = 0; index < this->geometryDragState->vertexIds.size() &&
                                     index < this->geometryDragState->originalPositions.size();
             ++index) {
            const auto position = vn::geom::Vec2{this->geometryDragState->originalPositions[index].x + delta.x,
                                                 this->geometryDragState->originalPositions[index].y + delta.y};
            changed = geometry->setVertexPosition(this->geometryDragState->vertexIds[index], position) || changed;
        }
    }
    if (!geometry->geometry().constraints().empty()) {
        const vn::constraints::GeometryConstraintSolver solver;
        changed = solver.apply(geometry->geometry()).changed || changed;
    }

    if (const auto* vertex = geometry->geometry().vertex(this->geometryDragState->vertexId)) {
        this->geometryDragState->currentPosition = vertex->position;
        this->geometryDragState->changed = this->geometryDragState->changed ||
                                           vertex->position.x != this->geometryDragState->originalPosition.x ||
                                           vertex->position.y != this->geometryDragState->originalPosition.y;

        if (this->selectedGeometryHit) {
            this->selectedGeometryHit->hit.point = Point(vertex->position.x, vertex->position.y);
        }
        if (this->hoveredGeometryHit && this->hoveredGeometryHit->hit.objectId == this->geometryDragState->objectId &&
            this->hoveredGeometryHit->hit.vertexId == this->geometryDragState->vertexId) {
            this->hoveredGeometryHit->hit.point = Point(vertex->position.x, vertex->position.y);
        }
    }
    this->geometryDragState->currentPositions.clear();
    for (auto vertexId: this->geometryDragState->vertexIds) {
        if (const auto* vertex = geometry->geometry().vertex(vertexId)) {
            this->geometryDragState->currentPositions.push_back(vertex->position);
        }
    }
    this->document->unlock();

    if (changed) {
        rebuildPageSnapshots();
    }
    return changed;
}

auto QtDocumentController::endGeometryVertexDrag() -> bool {
    const bool changed = this->geometryDragState && this->geometryDragState->changed;
    if (changed && this->document && this->geometryDragState) {
        this->document->lock();
        if (auto* geometry =
                    findMutableGeometryElement(this->geometryDragState->pageIndex, this->geometryDragState->objectId)) {
            pushGeometryHistory({.pageIndex = this->geometryDragState->pageIndex,
                                 .objectId = this->geometryDragState->objectId,
                                 .before = this->geometryDragState->beforeGeometry,
                                 .after = geometry->geometry(),
                                 .text = this->geometryDragState->vertexIds.size() > 1U ? "Move geometry vertices"
                                                                                         : "Move geometry vertex"});
        }
        this->document->unlock();
    }
    this->geometryDragState.reset();
    return changed;
}

auto QtDocumentController::activeGeometryDrag() const -> const std::optional<QtGeometryDragState>& {
    return this->geometryDragState;
}

auto QtDocumentController::deleteSelectedGeometry() -> bool {
    if (!this->selectedGeometryHit || !this->document) {
        return false;
    }

    bool changed = false;
    std::optional<vn::geom::GeometryObject> beforeGeometry;
    std::optional<vn::geom::GeometryObject> afterGeometry;
    std::string actionText = "Edit geometry topology";
    this->document->lock();
    auto* geometry = findMutableGeometryElement(this->selectedGeometryHit->pageIndex, this->selectedGeometryHit->hit.objectId);
    if (!geometry) {
        this->document->unlock();
        return false;
    }
    beforeGeometry = geometry->geometry();

    if (this->selectedGeometryHit->hit.type == vn::view::render::GeometryHitType::Vertex &&
        this->selectedGeometryHit->hit.vertexId != vn::geom::InvalidVertexId) {
        if (this->selectedGeometryVertexIds.size() > 1U) {
            for (auto vertexId: this->selectedGeometryVertexIds) {
                changed = geometry->removeVertex(vertexId) || changed;
            }
            actionText = "Delete geometry vertices";
        } else {
            changed = geometry->removeVertex(this->selectedGeometryHit->hit.vertexId);
            actionText = "Delete geometry vertex";
        }
    } else if (this->selectedGeometryHit->hit.type == vn::view::render::GeometryHitType::Edge &&
               this->selectedGeometryHit->hit.edgeId != vn::geom::InvalidEdgeId) {
        changed = geometry->removeEdge(this->selectedGeometryHit->hit.edgeId);
        actionText = "Delete geometry edge";
    }
    if (changed) {
        afterGeometry = geometry->geometry();
    }
    this->document->unlock();

    if (changed) {
        pushGeometryHistory({.pageIndex = this->selectedGeometryHit->pageIndex,
                             .objectId = this->selectedGeometryHit->hit.objectId,
                             .before = std::move(*beforeGeometry),
                             .after = std::move(*afterGeometry),
                             .text = std::move(actionText)});
        clearInteractiveGeometryState();
        rebuildPageSnapshots();
    }

    return changed;
}

auto QtDocumentController::insertVertexOnSelectedEdge() -> bool {
    if (!this->selectedGeometryHit || !this->document ||
        this->selectedGeometryHit->hit.type != vn::view::render::GeometryHitType::Edge ||
        this->selectedGeometryHit->hit.edgeId == vn::geom::InvalidEdgeId) {
        return false;
    }

    bool changed = false;
    std::optional<vn::geom::GeometryObject> beforeGeometry;
    std::optional<vn::geom::GeometryObject> afterGeometry;
    std::optional<vn::geom::VertexId> insertedVertexId;
    vn::geom::Vec2 insertedPoint{this->selectedGeometryHit->hit.point.x, this->selectedGeometryHit->hit.point.y};
    this->document->lock();
    auto* geometry = findMutableGeometryElement(this->selectedGeometryHit->pageIndex, this->selectedGeometryHit->hit.objectId);
    if (!geometry) {
        this->document->unlock();
        return false;
    }

    beforeGeometry = geometry->geometry();
    insertedVertexId = geometry->insertVertexOnEdge(this->selectedGeometryHit->hit.edgeId, insertedPoint);
    changed = insertedVertexId.has_value();
    if (changed) {
        afterGeometry = geometry->geometry();
        if (const auto* vertex = geometry->geometry().vertex(*insertedVertexId)) {
            insertedPoint = vertex->position;
        }
    }
    this->document->unlock();

    if (changed) {
        pushGeometryHistory({.pageIndex = this->selectedGeometryHit->pageIndex,
                             .objectId = this->selectedGeometryHit->hit.objectId,
                             .before = std::move(*beforeGeometry),
                             .after = std::move(*afterGeometry),
                             .text = "Insert geometry vertex"});
        vn::view::render::GeometryHitResult insertedHit;
        insertedHit.type = vn::view::render::GeometryHitType::Vertex;
        insertedHit.objectId = this->selectedGeometryHit->hit.objectId;
        insertedHit.vertexId = *insertedVertexId;
        insertedHit.edgeId = vn::geom::InvalidEdgeId;
        insertedHit.point = Point(insertedPoint.x, insertedPoint.y);
        insertedHit.snapKind.reset();
        insertedHit.screenDistance = 0.0;
        this->selectedGeometryHit = QtGeometryHit{.pageIndex = this->selectedGeometryHit->pageIndex,
                                                              .hit = std::move(insertedHit)};
        this->hoveredGeometryHit = this->selectedGeometryHit;
        rebuildPageSnapshots();
    }

    return changed;
}

auto QtDocumentController::canUndoGeometryEdit() const -> bool { return !this->geometryUndoHistory.empty(); }

auto QtDocumentController::canRedoGeometryEdit() const -> bool { return !this->geometryRedoHistory.empty(); }

auto QtDocumentController::undoGeometryEditText() const -> std::string {
    return this->geometryUndoHistory.empty() ? std::string{} : this->geometryUndoHistory.back().text;
}

auto QtDocumentController::redoGeometryEditText() const -> std::string {
    return this->geometryRedoHistory.empty() ? std::string{} : this->geometryRedoHistory.back().text;
}

auto QtDocumentController::undoGeometryEdit() -> bool {
    if (this->geometryUndoHistory.empty()) {
        return false;
    }

    auto entry = std::move(this->geometryUndoHistory.back());
    this->geometryUndoHistory.pop_back();
    const bool changed = applyGeometryHistoryEntry(entry, false);
    if (changed) {
        this->geometryRedoHistory.push_back(std::move(entry));
    } else {
        this->geometryUndoHistory.push_back(std::move(entry));
    }
    return changed;
}

auto QtDocumentController::redoGeometryEdit() -> bool {
    if (this->geometryRedoHistory.empty()) {
        return false;
    }

    auto entry = std::move(this->geometryRedoHistory.back());
    this->geometryRedoHistory.pop_back();
    const bool changed = applyGeometryHistoryEntry(entry, true);
    if (changed) {
        this->geometryUndoHistory.push_back(std::move(entry));
    } else {
        this->geometryRedoHistory.push_back(std::move(entry));
    }
    return changed;
}

auto QtDocumentController::isPdfPath(const std::filesystem::path& path) -> bool {
    return normalizeExtension(path) == ".pdf";
}

auto QtDocumentController::normalizeExtension(const std::filesystem::path& path) -> std::string {
    auto extension = path.extension().string();
    std::transform(extension.begin(), extension.end(), extension.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return extension;
}

void QtDocumentController::rebuildPageSnapshots() {
    this->pageSnapshots.clear();
    if (!this->document) {
        return;
    }
    this->pageSnapshots = vn::view::render::buildPageRenderSnapshots(*this->document);
}

void QtDocumentController::clearGeometryHistory() {
    this->geometryUndoHistory.clear();
    this->geometryRedoHistory.clear();
}

void QtDocumentController::pushGeometryHistory(QtGeometryHistoryEntry entry) {
    this->geometryRedoHistory.clear();
    this->geometryUndoHistory.push_back(std::move(entry));
}

auto QtDocumentController::applyGeometryHistoryEntry(const QtGeometryHistoryEntry& entry, bool useAfterState)
        -> bool {
    if (!this->document) {
        return false;
    }

    this->document->lock();
    auto* geometry = findMutableGeometryElement(entry.pageIndex, entry.objectId);
    if (!geometry) {
        this->document->unlock();
        return false;
    }

    geometry->replaceGeometry(useAfterState ? entry.after : entry.before);
    this->document->unlock();

    clearInteractiveGeometryState();
    rebuildPageSnapshots();
    return true;
}

auto QtDocumentController::findMutableGeometryElement(std::size_t pageIndex, vn::geom::ObjectId objectId)
        -> vn::geom::GeometryElement* {
    if (!this->document || pageIndex >= this->document->getPageCount()) {
        return nullptr;
    }

    auto page = this->document->getPage(pageIndex);
    if (!page) {
        return nullptr;
    }

    for (Layer* layer: page->getLayers()) {
        if (!layer || !layer->isVisible()) {
            continue;
        }

        for (auto& element: layer->getElements()) {
            auto* geometry = dynamic_cast<vn::geom::GeometryElement*>(element.get());
            if (geometry && geometry->geometry().objectId() == objectId) {
                return geometry;
            }
        }
    }

    return nullptr;
}

auto QtDocumentController::gridSnapProviderFor(PageTypeFormat format)
        -> std::shared_ptr<const vn::snap::ISnapProvider> {
    switch (format) {
        case PageTypeFormat::Graph:
        case PageTypeFormat::Dotted:
        case PageTypeFormat::IsoDotted:
        case PageTypeFormat::IsoGraph:
            return std::make_shared<vn::snap::GridSnapProvider>(28.0, 28.0, 0.35);
        default:
            return nullptr;
    }
}

// ---------------------------------------------------------------------------
// Stroke input
// ---------------------------------------------------------------------------

auto QtDocumentController::beginStroke(std::size_t pageIndex, double x, double y, double pressure, Color color,
                                       double width, StrokeTool::Value toolType, bool pressureSensitive) -> bool {
    if (!this->document || pageIndex >= this->document->getPageCount()) {
        return false;
    }

    auto stroke = std::make_unique<Stroke>();
    stroke->setToolType(StrokeTool(toolType));
    stroke->setColor(color);
    stroke->setWidth(width);

    if (toolType == StrokeTool::HIGHLIGHTER) {
        stroke->setFill(128);
    }

    const bool hasPressure = pressureSensitive && pressure > 0.0 && toolType == StrokeTool::PEN;
    if (hasPressure) {
        stroke->addPoint(Point(x, y, pressure * width));
    } else {
        stroke->addPoint(Point(x, y));
    }

    this->currentStroke = QtActiveStroke{.pageIndex = pageIndex, .stroke = std::move(stroke), .hasPressure = hasPressure};
    return true;
}

auto QtDocumentController::updateStroke(double x, double y, double pressure) -> bool {
    if (!this->currentStroke) {
        return false;
    }

    auto& stroke = this->currentStroke->stroke;
    const auto pointCount = stroke->getPointCount();
    if (pointCount == 0) {
        return false;
    }

    if (pressure == 0.0) {
        // Some devices emit zero-pressure moves when lifting — ignore them
        return true;
    }

    const auto lastPoint = stroke->getPoint(pointCount - 1);
    Point newPoint;
    if (this->currentStroke->hasPressure) {
        newPoint = Point(x, y, pressure * stroke->getWidth());
    } else {
        newPoint = Point(x, y);
    }

    constexpr double MIN_DISTANCE = 0.3;
    if (newPoint.lineLengthTo(lastPoint) < MIN_DISTANCE) {
        return true;
    }

    stroke->addPoint(newPoint);
    return true;
}

auto QtDocumentController::finalizeStroke() -> bool {
    if (!this->currentStroke) {
        return false;
    }

    auto& stroke = this->currentStroke->stroke;

    // A stroke with only one point needs a duplicate to be visible
    if (stroke->getPointCount() == 1) {
        const Point pt = stroke->getPoint(0);
        stroke->addPoint(pt);
    }

    if (stroke->getPointCount() < 2) {
        this->currentStroke.reset();
        return false;
    }

    stroke->freeUnusedPointItems();
    const std::size_t pageIndex = this->currentStroke->pageIndex;

    this->document->lock();
    if (pageIndex >= this->document->getPageCount()) {
        this->document->unlock();
        this->currentStroke.reset();
        return false;
    }

    auto page = this->document->getPage(pageIndex);
    auto* layer = page ? page->getSelectedLayer() : nullptr;
    if (!layer) {
        this->document->unlock();
        this->currentStroke.reset();
        return false;
    }

    const Element* elementPtr = stroke.get();
    layer->addElement(std::move(stroke));
    this->document->unlock();

    pushHistory(QtHistoryEntry{QtStrokeHistoryEntry{.pageIndex = pageIndex, .element = elementPtr, .text = "Draw stroke"}});
    rebuildPageSnapshots();
    this->currentStroke.reset();
    return true;
}

auto QtDocumentController::cancelStroke() -> void { this->currentStroke.reset(); }

auto QtDocumentController::activeStroke() const -> const QtActiveStroke* {
    return this->currentStroke ? &*this->currentStroke : nullptr;
}

// ---------------------------------------------------------------------------
// Eraser
// ---------------------------------------------------------------------------

auto QtDocumentController::beginErase(std::size_t pageIndex) -> void {
    this->pendingErase = QtEraseHistoryEntry{.pageIndex = pageIndex, .removedElements = {}, .text = "Erase"};
}

auto QtDocumentController::eraseAt(std::size_t pageIndex, double x, double y, double halfSize) -> int {
    if (!this->document || pageIndex >= this->document->getPageCount()) {
        return 0;
    }

    this->document->lock();
    auto page = this->document->getPage(pageIndex);
    auto* layer = page ? page->getSelectedLayer() : nullptr;
    if (!layer) {
        this->document->unlock();
        return 0;
    }

    // Collect strokes that intersect the eraser point
    std::vector<const Element*> toRemove;
    for (auto& ep: layer->getElements()) {
        if (!ep || ep->getType() != ELEMENT_STROKE) {
            continue;
        }
        auto* s = dynamic_cast<Stroke*>(ep.get());
        if (s && s->intersects(x, y, halfSize)) {
            toRemove.push_back(s);
        }
    }

    int erased = 0;
    for (const auto* elem: toRemove) {
        auto removed = layer->removeElement(elem);
        if (removed.e) {
            if (this->pendingErase) {
                this->pendingErase->removedElements.push_back(std::move(removed));
            }
            ++erased;
        }
    }

    this->document->unlock();
    if (erased > 0) {
        rebuildPageSnapshots();
    }
    return erased;
}

auto QtDocumentController::finalizeErase() -> bool {
    // Finalize segment erase if active
    if (this->pendingSegmentErase) {
        if (this->pendingSegmentErase->removedOriginals.empty()) {
            this->pendingSegmentErase.reset();
            return false;
        }
        auto entry = std::move(*this->pendingSegmentErase);
        this->pendingSegmentErase.reset();

        const auto count = entry.removedOriginals.size();
        entry.text = count == 1 ? "Segment erase stroke"
                                : "Segment erase " + std::to_string(count) + " strokes";

        pushHistory(QtHistoryEntry{std::move(entry)});
        return true;
    }

    // Finalize whole-stroke erase
    if (!this->pendingErase || this->pendingErase->removedElements.empty()) {
        this->pendingErase.reset();
        return false;
    }

    auto entry = std::move(*this->pendingErase);
    this->pendingErase.reset();

    const auto count = entry.removedElements.size();
    entry.text = count == 1 ? "Erase stroke" : "Erase " + std::to_string(count) + " strokes";

    pushHistory(QtHistoryEntry{std::move(entry)});
    return true;
}

auto QtDocumentController::cancelErase() -> void {
    // Cancel segment erase: remove fragments from layer, let owned originals drop
    if (this->pendingSegmentErase) {
        if (this->document) {
            this->document->lock();
            for (auto pageIdx = this->pendingSegmentErase->pageIndex;
                 pageIdx < this->document->getPageCount() && !this->pendingSegmentErase->fragmentPtrs.empty();) {
                auto page = this->document->getPage(this->pendingSegmentErase->pageIndex);
                auto* layer = page ? page->getSelectedLayer() : nullptr;
                if (layer) {
                    for (const auto* frag: this->pendingSegmentErase->fragmentPtrs) {
                        layer->removeElement(frag);
                    }
                }
                break;
            }
            this->document->unlock();
        }
        this->pendingSegmentErase.reset();
        rebuildPageSnapshots();
        return;
    }
    this->pendingErase.reset();
}

auto QtDocumentController::isErasing() const -> bool {
    return this->pendingErase.has_value() || this->pendingSegmentErase.has_value();
}

auto QtDocumentController::eraseSegmentAt(std::size_t pageIndex, double x, double y, double halfSize) -> int {
    if (!this->document || pageIndex >= this->document->getPageCount()) {
        return 0;
    }

    this->document->lock();
    auto page = this->document->getPage(pageIndex);
    auto* layer = page ? page->getSelectedLayer() : nullptr;
    if (!layer) {
        this->document->unlock();
        return 0;
    }

    // Initialize segment erase state if needed
    if (!this->pendingSegmentErase) {
        this->pendingSegmentErase =
                QtSegmentEraseHistoryEntry{.pageIndex = pageIndex, .text = "Segment erase"};
    }

    const PaddedBox box{Point(x, y), halfSize, halfSize * 1.2};

    // Collect strokes that might intersect
    std::vector<Stroke*> candidates;
    for (auto& ep: layer->getElements()) {
        if (!ep || ep->getType() != ELEMENT_STROKE) {
            continue;
        }
        auto* s = dynamic_cast<Stroke*>(ep.get());
        if (s && s->intersects(x, y, halfSize)) {
            candidates.push_back(s);
        }
    }

    int affected = 0;
    for (auto* stroke: candidates) {
        if (stroke->getPointCount() < 2) {
            continue;
        }

        // Get precise intersection parameters
        auto intersections = stroke->intersectWithPaddedBox(box);
        if (intersections.empty()) {
            continue;
        }

        // Compute remaining sections (parts NOT erased)
        const PathParameter strokeStart{0, 0.0};
        const PathParameter strokeEnd{stroke->getPointCount() - 2, 1.0};

        std::vector<std::pair<PathParameter, PathParameter>> remaining;
        PathParameter current = strokeStart;
        for (std::size_t i = 0; i + 1 < intersections.size(); i += 2) {
            if (current < intersections[i]) {
                remaining.emplace_back(current, intersections[i]);
            }
            current = intersections[i + 1];
        }
        if (current < strokeEnd) {
            remaining.emplace_back(current, strokeEnd);
        }

        // If no remaining sections, delete the whole stroke
        if (remaining.empty()) {
            auto removed = layer->removeElement(stroke);
            if (removed.e) {
                this->pendingSegmentErase->removedOriginals.push_back(std::move(removed));
                ++affected;
            }
            continue;
        }

        // If remaining sections cover the whole stroke, skip
        if (remaining.size() == 1 && !(strokeStart < remaining[0].first) && !(remaining[0].second < strokeEnd)) {
            continue;
        }

        // Create fragment strokes from remaining sections
        std::vector<std::unique_ptr<Stroke>> fragments;
        for (const auto& [lo, hi]: remaining) {
            auto frag = stroke->cloneSection(lo, hi);
            if (frag && frag->getPointCount() >= 2) {
                fragments.push_back(std::move(frag));
            }
        }

        // Remove original stroke
        auto removed = layer->removeElement(stroke);
        if (!removed.e) {
            continue;
        }
        this->pendingSegmentErase->removedOriginals.push_back(std::move(removed));

        // Insert fragments
        for (auto& frag: fragments) {
            const auto* ptr = frag.get();
            layer->addElement(std::move(frag));
            this->pendingSegmentErase->fragmentPtrs.push_back(ptr);
        }
        ++affected;
    }

    this->document->unlock();
    if (affected > 0) {
        rebuildPageSnapshots();
    }
    return affected;
}

// ---------------------------------------------------------------------------
// Element selection
// ---------------------------------------------------------------------------

auto QtDocumentController::hitTestElement(std::size_t pageIndex, double pageX, double pageY,
                                          double maxDistance) const -> const Element* {
    if (!this->document || pageIndex >= this->document->getPageCount()) {
        return nullptr;
    }

    const Element* best = nullptr;
    double bestDist = maxDistance;

    auto page = this->document->getPage(pageIndex);
    if (!page) {
        return nullptr;
    }

    for (const Layer* layer: page->getLayersView()) {
        if (!layer || !layer->isVisible()) {
            continue;
        }
        for (const Element* element: layer->getElementsView()) {
            if (!element) {
                continue;
            }
            const double dist = element->distanceTo(pageX, pageY);
            if (dist < bestDist) {
                bestDist = dist;
                best = element;
            }
        }
    }
    return best;
}

void QtDocumentController::selectElementAt(std::size_t pageIndex, double pageX, double pageY, double maxDistance,
                                           bool additive) {
    const Element* hit = hitTestElement(pageIndex, pageX, pageY, maxDistance);
    if (!hit) {
        if (!additive) {
            clearElementSelection();
        }
        return;
    }

    if (additive && this->currentSelection && this->currentSelection->pageIndex == pageIndex) {
        // Toggle: if already selected, deselect; otherwise add
        auto& elems = this->currentSelection->elements;
        auto it = std::find(elems.begin(), elems.end(), hit);
        if (it != elems.end()) {
            elems.erase(it);
            if (elems.empty()) {
                this->currentSelection.reset();
            }
        } else {
            elems.push_back(hit);
        }
    } else {
        this->currentSelection = QtElementSelection{.pageIndex = pageIndex, .elements = {hit}};
    }
}

void QtDocumentController::selectElementsInRect(std::size_t pageIndex, double x, double y, double w, double h) {
    if (!this->document || pageIndex >= this->document->getPageCount()) {
        return;
    }

    auto page = this->document->getPage(pageIndex);
    if (!page) {
        return;
    }

    std::vector<const Element*> hits;
    for (const Layer* layer: page->getLayersView()) {
        if (!layer || !layer->isVisible()) {
            continue;
        }
        for (const Element* element: layer->getElementsView()) {
            if (!element) {
                continue;
            }
            if (element->intersectsArea(x, y, w, h)) {
                hits.push_back(element);
            }
        }
    }

    if (hits.empty()) {
        this->currentSelection.reset();
    } else {
        this->currentSelection = QtElementSelection{.pageIndex = pageIndex, .elements = std::move(hits)};
    }
}

void QtDocumentController::clearElementSelection() { this->currentSelection.reset(); }

auto QtDocumentController::elementSelection() const -> const std::optional<QtElementSelection>& {
    return this->currentSelection;
}

auto QtDocumentController::isElementSelected(const Element* e) const -> bool {
    if (!this->currentSelection || !e) {
        return false;
    }
    const auto& elems = this->currentSelection->elements;
    return std::find(elems.begin(), elems.end(), e) != elems.end();
}

// ---------------------------------------------------------------------------
// Element move
// ---------------------------------------------------------------------------

auto QtDocumentController::beginMoveSelection(double pageX, double pageY) -> bool {
    if (!this->currentSelection || this->currentSelection->elements.empty()) {
        return false;
    }

    this->moveState = QtMoveState{.startX = pageX,
                                  .startY = pageY,
                                  .currentDx = 0.0,
                                  .currentDy = 0.0,
                                  .elements = this->currentSelection->elements,
                                  .pageIndex = this->currentSelection->pageIndex};
    return true;
}

auto QtDocumentController::updateMoveSelection(double pageX, double pageY) -> bool {
    if (!this->moveState || !this->document) {
        return false;
    }

    const double newDx = pageX - this->moveState->startX;
    const double newDy = pageY - this->moveState->startY;
    const double deltaDx = newDx - this->moveState->currentDx;
    const double deltaDy = newDy - this->moveState->currentDy;

    if (std::abs(deltaDx) < 1e-6 && std::abs(deltaDy) < 1e-6) {
        return false;
    }

    this->document->lock();
    for (const auto* elem: this->moveState->elements) {
        // const_cast is needed because the selection stores const pointers
        // but Element::move is a non-const operation on the same objects we own
        auto* mutableElem = const_cast<Element*>(elem);
        mutableElem->move(deltaDx, deltaDy);
    }
    this->document->unlock();

    this->moveState->currentDx = newDx;
    this->moveState->currentDy = newDy;
    rebuildPageSnapshots();
    return true;
}

auto QtDocumentController::endMoveSelection() -> bool {
    if (!this->moveState) {
        return false;
    }

    const double dx = this->moveState->currentDx;
    const double dy = this->moveState->currentDy;

    if (std::abs(dx) < 1e-6 && std::abs(dy) < 1e-6) {
        this->moveState.reset();
        return false;
    }

    pushHistory(QtHistoryEntry{QtMoveHistoryEntry{.pageIndex = this->moveState->pageIndex,
                                                   .elements = this->moveState->elements,
                                                   .dx = dx,
                                                   .dy = dy,
                                                   .text = "Move elements"}});
    this->moveState.reset();
    return true;
}

auto QtDocumentController::cancelMoveSelection() -> void {
    if (!this->moveState || !this->document) {
        this->moveState.reset();
        return;
    }

    // Undo the partial move
    const double dx = this->moveState->currentDx;
    const double dy = this->moveState->currentDy;
    if (std::abs(dx) > 1e-6 || std::abs(dy) > 1e-6) {
        this->document->lock();
        for (const auto* elem: this->moveState->elements) {
            auto* mutableElem = const_cast<Element*>(elem);
            mutableElem->move(-dx, -dy);
        }
        this->document->unlock();
        rebuildPageSnapshots();
    }
    this->moveState.reset();
}

auto QtDocumentController::isMovingSelection() const -> bool { return this->moveState.has_value(); }

// ---------------------------------------------------------------------------
// Layer management
// ---------------------------------------------------------------------------

auto QtDocumentController::layerCount(std::size_t pageIndex) const -> std::size_t {
    if (!this->document || pageIndex >= this->document->getPageCount()) {
        return 0;
    }
    auto page = this->document->getPage(pageIndex);
    return page ? static_cast<std::size_t>(page->getLayerCount()) : 0;
}

auto QtDocumentController::layerInfos(std::size_t pageIndex) const -> std::vector<QtLayerInfo> {
    std::vector<QtLayerInfo> infos;
    if (!this->document || pageIndex >= this->document->getPageCount()) {
        return infos;
    }
    auto page = this->document->getPage(pageIndex);
    if (!page) {
        return infos;
    }

    const auto selectedId = page->getSelectedLayerId();
    const auto& layers = page->getLayers();
    infos.reserve(layers.size());
    for (std::size_t i = 0; i < layers.size(); ++i) {
        const auto* layer = layers[i];
        if (!layer) {
            continue;
        }
        QtLayerInfo info;
        info.index = i;
        info.name = layer->hasName() ? layer->getName() : "Layer " + std::to_string(i + 1);
        info.visible = layer->isVisible();
        info.selected = (static_cast<std::size_t>(selectedId) == i);
        info.elementCount = layer->getElementsView().size();
        infos.push_back(std::move(info));
    }
    return infos;
}

auto QtDocumentController::selectedLayerIndex(std::size_t pageIndex) const -> std::size_t {
    if (!this->document || pageIndex >= this->document->getPageCount()) {
        return 0;
    }
    auto page = this->document->getPage(pageIndex);
    return page ? static_cast<std::size_t>(page->getSelectedLayerId()) : 0;
}

void QtDocumentController::selectLayer(std::size_t pageIndex, std::size_t layerIndex) {
    if (!this->document || pageIndex >= this->document->getPageCount()) {
        return;
    }
    this->document->lock();
    auto page = this->document->getPage(pageIndex);
    if (page) {
        page->setSelectedLayerId(static_cast<Layer::Index>(layerIndex));
    }
    this->document->unlock();
}

void QtDocumentController::setLayerVisible(std::size_t pageIndex, std::size_t layerIndex, bool visible) {
    if (!this->document || pageIndex >= this->document->getPageCount()) {
        return;
    }
    this->document->lock();
    auto page = this->document->getPage(pageIndex);
    if (page) {
        auto& layers = page->getLayers();
        if (layerIndex < layers.size() && layers[layerIndex]) {
            layers[layerIndex]->setVisible(visible);
        }
    }
    this->document->unlock();
    rebuildPageSnapshots();
}

void QtDocumentController::addLayer(std::size_t pageIndex) {
    if (!this->document || pageIndex >= this->document->getPageCount()) {
        return;
    }
    this->document->lock();
    auto page = this->document->getPage(pageIndex);
    if (page) {
        auto* newLayer = new Layer();
        auto& layers = page->getLayers();
        layers.push_back(newLayer);
        page->setSelectedLayerId(static_cast<Layer::Index>(layers.size() - 1));
    }
    this->document->unlock();
    rebuildPageSnapshots();
}

void QtDocumentController::removeLayer(std::size_t pageIndex, std::size_t layerIndex) {
    if (!this->document || pageIndex >= this->document->getPageCount()) {
        return;
    }
    this->document->lock();
    auto page = this->document->getPage(pageIndex);
    if (page) {
        auto& layers = page->getLayers();
        if (layers.size() > 1 && layerIndex < layers.size()) {
            delete layers[layerIndex];
            layers.erase(layers.begin() + static_cast<std::ptrdiff_t>(layerIndex));
            if (static_cast<std::size_t>(page->getSelectedLayerId()) >= layers.size()) {
                page->setSelectedLayerId(static_cast<Layer::Index>(layers.size() - 1));
            }
        }
    }
    this->document->unlock();
    rebuildPageSnapshots();
}

void QtDocumentController::renameLayer(std::size_t pageIndex, std::size_t layerIndex, const std::string& name) {
    if (!this->document || pageIndex >= this->document->getPageCount()) {
        return;
    }
    this->document->lock();
    auto page = this->document->getPage(pageIndex);
    if (page) {
        auto& layers = page->getLayers();
        if (layerIndex < layers.size() && layers[layerIndex]) {
            layers[layerIndex]->setName(name);
        }
    }
    this->document->unlock();
}

void QtDocumentController::moveLayerUp(std::size_t pageIndex, std::size_t layerIndex) {
    if (!this->document || pageIndex >= this->document->getPageCount()) {
        return;
    }
    this->document->lock();
    auto page = this->document->getPage(pageIndex);
    if (page) {
        auto& layers = page->getLayers();
        if (layerIndex + 1 < layers.size()) {
            std::swap(layers[layerIndex], layers[layerIndex + 1]);
            if (static_cast<std::size_t>(page->getSelectedLayerId()) == layerIndex) {
                page->setSelectedLayerId(static_cast<Layer::Index>(layerIndex + 1));
            } else if (static_cast<std::size_t>(page->getSelectedLayerId()) == layerIndex + 1) {
                page->setSelectedLayerId(static_cast<Layer::Index>(layerIndex));
            }
        }
    }
    this->document->unlock();
    rebuildPageSnapshots();
}

void QtDocumentController::moveLayerDown(std::size_t pageIndex, std::size_t layerIndex) {
    if (!this->document || pageIndex >= this->document->getPageCount()) {
        return;
    }
    this->document->lock();
    auto page = this->document->getPage(pageIndex);
    if (page) {
        auto& layers = page->getLayers();
        if (layerIndex > 0) {
            std::swap(layers[layerIndex], layers[layerIndex - 1]);
            if (static_cast<std::size_t>(page->getSelectedLayerId()) == layerIndex) {
                page->setSelectedLayerId(static_cast<Layer::Index>(layerIndex - 1));
            } else if (static_cast<std::size_t>(page->getSelectedLayerId()) == layerIndex - 1) {
                page->setSelectedLayerId(static_cast<Layer::Index>(layerIndex));
            }
        }
    }
    this->document->unlock();
    rebuildPageSnapshots();
}

// ---------------------------------------------------------------------------
// Page background
// ---------------------------------------------------------------------------

void QtDocumentController::setPageBackgroundColor(std::size_t pageIndex, Color color) {
    if (!this->document || pageIndex >= this->document->getPageCount()) {
        return;
    }
    this->document->lock();
    auto page = this->document->getPage(pageIndex);
    if (page) {
        page->setBackgroundColor(color);
    }
    this->document->unlock();
    rebuildPageSnapshots();
}

void QtDocumentController::setPageBackgroundType(std::size_t pageIndex, PageTypeFormat format) {
    if (!this->document || pageIndex >= this->document->getPageCount()) {
        return;
    }
    this->document->lock();
    auto page = this->document->getPage(pageIndex);
    if (page) {
        PageType type = page->getBackgroundType();
        type.format = format;
        page->setBackgroundType(type);
    }
    this->document->unlock();
    rebuildPageSnapshots();
}

// ---------------------------------------------------------------------------
// Text editing
// ---------------------------------------------------------------------------

auto QtDocumentController::insertTextElement(std::size_t pageIndex, std::unique_ptr<Text> text) -> const Element* {
    if (!this->document || !text || pageIndex >= this->document->getPageCount()) {
        return nullptr;
    }
    this->document->lock();
    auto page = this->document->getPage(pageIndex);
    auto* layer = page ? page->getSelectedLayer() : nullptr;
    if (!layer) {
        this->document->unlock();
        return nullptr;
    }

    const auto* ptr = text.get();
    layer->addElement(std::move(text));

    // Push to undo history
    QtTextHistoryEntry entry;
    entry.pageIndex = pageIndex;
    entry.element = ptr;
    entry.isNew = true;
    entry.text = "Insert text";
    pushHistory(QtHistoryEntry{std::move(entry)});

    this->document->unlock();
    rebuildPageSnapshots();
    return ptr;
}

auto QtDocumentController::hitTestTextElement(std::size_t pageIndex, double pageX, double pageY,
                                              double maxDistance) const -> Text* {
    if (!this->document || pageIndex >= this->document->getPageCount()) {
        return nullptr;
    }
    auto page = this->document->getPage(pageIndex);
    auto* layer = page ? page->getSelectedLayer() : nullptr;
    if (!layer) {
        return nullptr;
    }

    Text* best = nullptr;
    double bestDist = maxDistance;

    for (auto& ep: layer->getElements()) {
        if (!ep || ep->getType() != ELEMENT_TEXT) {
            continue;
        }
        auto* t = dynamic_cast<Text*>(ep.get());
        if (!t) {
            continue;
        }
        const double dist = t->distanceTo(pageX, pageY);
        if (dist < bestDist) {
            bestDist = dist;
            best = t;
        }
    }
    return best;
}

// ---------------------------------------------------------------------------
// Unified undo/redo
// ---------------------------------------------------------------------------

auto QtHistoryEntry::text() const -> std::string {
    return std::visit([](auto& entry) { return entry.text; }, this->data);
}

void QtDocumentController::clearHistory() {
    this->undoHistory.clear();
    this->redoHistory.clear();
}

void QtDocumentController::pushHistory(QtHistoryEntry entry) {
    this->redoHistory.clear();
    this->undoHistory.push_back(std::move(entry));
}

auto QtDocumentController::applyHistoryUndo(QtHistoryEntry& entry) -> bool {
    return std::visit(
            [this](auto& e) -> bool {
                using T = std::decay_t<decltype(e)>;
                if constexpr (std::is_same_v<T, QtGeometryHistoryEntry>) {
                    return applyGeometryHistoryEntry(e, false);
                } else if constexpr (std::is_same_v<T, QtStrokeHistoryEntry>) {
                    // Stroke undo: remove the element from the layer and take ownership
                    if (!this->document || e.pageIndex >= this->document->getPageCount()) {
                        return false;
                    }
                    this->document->lock();
                    auto page = this->document->getPage(e.pageIndex);
                    if (!page) {
                        this->document->unlock();
                        return false;
                    }
                    for (auto* layer: page->getLayers()) {
                        if (!layer) {
                            continue;
                        }
                        auto removed = layer->removeElement(e.element);
                        if (removed.e) {
                            e.ownedElement = std::move(removed.e);
                            e.insertionPos = removed.pos;
                            this->document->unlock();
                            rebuildPageSnapshots();
                            return true;
                        }
                    }
                    this->document->unlock();
                    return false;
                } else if constexpr (std::is_same_v<T, QtEraseHistoryEntry>) {
                    // Erase undo: re-insert all removed elements at their original positions
                    if (e.removedElements.empty() || !this->document ||
                        e.pageIndex >= this->document->getPageCount()) {
                        return false;
                    }
                    this->document->lock();
                    auto page = this->document->getPage(e.pageIndex);
                    auto* layer = page ? page->getSelectedLayer() : nullptr;
                    if (!layer) {
                        this->document->unlock();
                        return false;
                    }
                    // Re-insert in ascending position order to restore original z-order
                    std::sort(e.removedElements.begin(), e.removedElements.end());
                    e.elementPtrs.clear();
                    for (auto& ip: e.removedElements) {
                        e.elementPtrs.push_back(ip.e.get());
                        layer->insertElement(std::move(ip.e), ip.pos);
                    }
                    this->document->unlock();
                    rebuildPageSnapshots();
                    return true;
                } else if constexpr (std::is_same_v<T, QtSegmentEraseHistoryEntry>) {
                    // Segment erase undo: remove fragments, re-insert originals
                    if (e.removedOriginals.empty() || !this->document ||
                        e.pageIndex >= this->document->getPageCount()) {
                        return false;
                    }
                    this->document->lock();
                    auto page = this->document->getPage(e.pageIndex);
                    auto* layer = page ? page->getSelectedLayer() : nullptr;
                    if (!layer) {
                        this->document->unlock();
                        return false;
                    }
                    // Remove fragments from layer
                    e.ownedFragments.clear();
                    for (const auto* frag: e.fragmentPtrs) {
                        auto removed = layer->removeElement(frag);
                        if (removed.e) {
                            e.ownedFragments.push_back(std::move(removed));
                        }
                    }
                    e.fragmentPtrs.clear();
                    // Re-insert originals at their original positions
                    std::sort(e.removedOriginals.begin(), e.removedOriginals.end());
                    e.removedOriginalPtrs.clear();
                    for (auto& ip: e.removedOriginals) {
                        e.removedOriginalPtrs.push_back(ip.e.get());
                        layer->insertElement(std::move(ip.e), ip.pos);
                    }
                    this->document->unlock();
                    rebuildPageSnapshots();
                    return true;
                } else if constexpr (std::is_same_v<T, QtMoveHistoryEntry>) {
                    // Move undo: move elements back by -dx, -dy
                    if (e.elements.empty() || !this->document ||
                        e.pageIndex >= this->document->getPageCount()) {
                        return false;
                    }
                    this->document->lock();
                    for (const auto* elem: e.elements) {
                        auto* mutableElem = const_cast<Element*>(elem);
                        mutableElem->move(-e.dx, -e.dy);
                    }
                    this->document->unlock();
                    rebuildPageSnapshots();
                    return true;
                } else if constexpr (std::is_same_v<T, QtTextHistoryEntry>) {
                    // Text undo: remove the text element from the layer
                    if (!this->document || e.pageIndex >= this->document->getPageCount()) {
                        return false;
                    }
                    this->document->lock();
                    auto page = this->document->getPage(e.pageIndex);
                    if (!page) {
                        this->document->unlock();
                        return false;
                    }
                    for (auto* layer: page->getLayers()) {
                        if (!layer) {
                            continue;
                        }
                        auto removed = layer->removeElement(e.element);
                        if (removed.e) {
                            e.ownedElement = std::move(removed.e);
                            e.insertionPos = removed.pos;
                            this->document->unlock();
                            rebuildPageSnapshots();
                            return true;
                        }
                    }
                    this->document->unlock();
                    return false;
                }
            },
            entry.data);
}

auto QtDocumentController::applyHistoryRedo(QtHistoryEntry& entry) -> bool {
    return std::visit(
            [this](auto& e) -> bool {
                using T = std::decay_t<decltype(e)>;
                if constexpr (std::is_same_v<T, QtGeometryHistoryEntry>) {
                    return applyGeometryHistoryEntry(e, true);
                } else if constexpr (std::is_same_v<T, QtStrokeHistoryEntry>) {
                    // Stroke redo: re-insert the owned element at its original position
                    if (!e.ownedElement || !this->document ||
                        e.pageIndex >= this->document->getPageCount()) {
                        return false;
                    }
                    this->document->lock();
                    auto page = this->document->getPage(e.pageIndex);
                    auto* layer = page ? page->getSelectedLayer() : nullptr;
                    if (!layer) {
                        this->document->unlock();
                        return false;
                    }
                    e.element = e.ownedElement.get();
                    layer->insertElement(std::move(e.ownedElement), e.insertionPos);
                    this->document->unlock();
                    rebuildPageSnapshots();
                    return true;
                } else if constexpr (std::is_same_v<T, QtEraseHistoryEntry>) {
                    // Erase redo: remove elements again using saved raw pointers
                    if (e.elementPtrs.empty() || !this->document ||
                        e.pageIndex >= this->document->getPageCount()) {
                        return false;
                    }
                    this->document->lock();
                    auto page = this->document->getPage(e.pageIndex);
                    auto* layer = page ? page->getSelectedLayer() : nullptr;
                    if (!layer) {
                        this->document->unlock();
                        return false;
                    }
                    e.removedElements.clear();
                    for (const auto* ptr: e.elementPtrs) {
                        auto removed = layer->removeElement(ptr);
                        if (removed.e) {
                            e.removedElements.push_back(std::move(removed));
                        }
                    }
                    e.elementPtrs.clear();
                    this->document->unlock();
                    if (!e.removedElements.empty()) {
                        rebuildPageSnapshots();
                        return true;
                    }
                    return false;
                } else if constexpr (std::is_same_v<T, QtSegmentEraseHistoryEntry>) {
                    // Segment erase redo: remove originals, re-insert fragments
                    if (e.removedOriginalPtrs.empty() || !this->document ||
                        e.pageIndex >= this->document->getPageCount()) {
                        return false;
                    }
                    this->document->lock();
                    auto page = this->document->getPage(e.pageIndex);
                    auto* layer = page ? page->getSelectedLayer() : nullptr;
                    if (!layer) {
                        this->document->unlock();
                        return false;
                    }
                    // Remove originals from layer
                    e.removedOriginals.clear();
                    for (const auto* ptr: e.removedOriginalPtrs) {
                        auto removed = layer->removeElement(ptr);
                        if (removed.e) {
                            e.removedOriginals.push_back(std::move(removed));
                        }
                    }
                    e.removedOriginalPtrs.clear();
                    // Re-insert fragments
                    e.fragmentPtrs.clear();
                    for (auto& ip: e.ownedFragments) {
                        e.fragmentPtrs.push_back(ip.e.get());
                        layer->addElement(std::move(ip.e));
                    }
                    e.ownedFragments.clear();
                    this->document->unlock();
                    rebuildPageSnapshots();
                    return true;
                } else if constexpr (std::is_same_v<T, QtMoveHistoryEntry>) {
                    // Move redo: move elements by dx, dy again
                    if (e.elements.empty() || !this->document ||
                        e.pageIndex >= this->document->getPageCount()) {
                        return false;
                    }
                    this->document->lock();
                    for (const auto* elem: e.elements) {
                        auto* mutableElem = const_cast<Element*>(elem);
                        mutableElem->move(e.dx, e.dy);
                    }
                    this->document->unlock();
                    rebuildPageSnapshots();
                    return true;
                } else if constexpr (std::is_same_v<T, QtTextHistoryEntry>) {
                    // Text redo: re-insert the text element at its original position
                    if (!e.ownedElement || !this->document ||
                        e.pageIndex >= this->document->getPageCount()) {
                        return false;
                    }
                    this->document->lock();
                    auto page = this->document->getPage(e.pageIndex);
                    auto* layer = page ? page->getSelectedLayer() : nullptr;
                    if (!layer) {
                        this->document->unlock();
                        return false;
                    }
                    e.element = e.ownedElement.get();
                    layer->insertElement(std::move(e.ownedElement), e.insertionPos);
                    this->document->unlock();
                    rebuildPageSnapshots();
                    return true;
                }
            },
            entry.data);
}

auto QtDocumentController::canUndo() const -> bool {
    return !this->undoHistory.empty() || !this->geometryUndoHistory.empty();
}

auto QtDocumentController::canRedo() const -> bool {
    return !this->redoHistory.empty() || !this->geometryRedoHistory.empty();
}

auto QtDocumentController::undoText() const -> std::string {
    // Prefer the newest entry from either stack
    if (!this->undoHistory.empty() && !this->geometryUndoHistory.empty()) {
        return this->undoHistory.back().text();
    }
    if (!this->undoHistory.empty()) {
        return this->undoHistory.back().text();
    }
    if (!this->geometryUndoHistory.empty()) {
        return this->geometryUndoHistory.back().text;
    }
    return {};
}

auto QtDocumentController::redoText() const -> std::string {
    if (!this->redoHistory.empty()) {
        return this->redoHistory.back().text();
    }
    if (!this->geometryRedoHistory.empty()) {
        return this->geometryRedoHistory.back().text;
    }
    return {};
}

auto QtDocumentController::undo() -> bool {
    // Try unified history first, fall back to geometry-only
    if (!this->undoHistory.empty()) {
        auto entry = std::move(this->undoHistory.back());
        this->undoHistory.pop_back();
        if (applyHistoryUndo(entry)) {
            this->redoHistory.push_back(std::move(entry));
            return true;
        }
        this->undoHistory.push_back(std::move(entry));
        return false;
    }
    return undoGeometryEdit();
}

auto QtDocumentController::redo() -> bool {
    if (!this->redoHistory.empty()) {
        auto entry = std::move(this->redoHistory.back());
        this->redoHistory.pop_back();
        if (applyHistoryRedo(entry)) {
            this->undoHistory.push_back(std::move(entry));
            return true;
        }
        this->redoHistory.push_back(std::move(entry));
        return false;
    }
    return redoGeometryEdit();
}
