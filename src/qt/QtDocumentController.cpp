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
