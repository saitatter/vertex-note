/*
 * VertexNote
 *
 * Qt document controller backed by the shared core model.
 */

#include "QtDocumentController.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <limits>
#include <memory>
#include <numeric>
#include <unordered_set>
#include <utility>

#include "control/shaperecognizer/ShapeRecognizer.h"
#include "control/xojfile/LoadHandler.h"
#include "control/xojfile/SaveHandler.h"
#include "model/Element.h"
#include "model/Document.h"
#include "model/Image.h"
#include "model/Layer.h"
#include "model/NotePage.h"
#include "model/Text.h"
#include "model/Stroke.h"
#include "model/StrokeStyle.h"
#include "model/SplineSegment.h"
#include "vertexnote/constraints/GeometryConstraintSolver.h"
#include "vertexnote/geometry/GeometryElement.h"
#include "vertexnote/geometry/GeometryIdGenerator.h"
#include "vertexnote/snapping/GeometrySnapProvider.h"
#include "vertexnote/snapping/GridSnapProvider.h"
#include "vertexnote/snapping/ISnapProvider.h"
#include "vertexnote/snapping/PageGeometryCollector.h"
#include "vertexnote/snapping/SnapEngine.h"
#include "view/render/PageRasterPreviewFactory.h"

QtDocumentController::QtDocumentController() { newBlankDocument(); }

void QtDocumentController::newBlankDocument() {
    this->document = std::make_unique<Document>(&this->documentHandler);
    this->document->lock();
    this->document->addPage(std::make_shared<NotePage>(595.0, 842.0));
    this->document->unlock();
    this->loadedPath.reset();
    clearPdfRasterCache();
    clearGeometryHistory();
    clearInteractiveGeometryState();
    this->activePdfTextSelection.reset();
    rebuildPageSnapshots();
}

auto QtDocumentController::loadFrom(const std::filesystem::path& path, std::string* errorMessage) -> bool {
    try {
        if (isPdfPath(path)) {
            return loadPdfAsDocument(path, false, errorMessage);
        }

        LoadHandler loader;
        auto loaded = loader.loadDocument(path);
        this->document = std::move(loaded);
        this->loadedPath = path;
        clearPdfRasterCache();
        clearGeometryHistory();
        clearInteractiveGeometryState();
        this->activePdfTextSelection.reset();
        rebuildPageSnapshots();
        return true;
    } catch (const std::exception& e) {
        if (errorMessage) {
            *errorMessage = e.what();
        }
        return false;
    }
}

auto QtDocumentController::loadPdfAsDocument(const std::filesystem::path& path, bool attachToDocument,
                                             std::string* errorMessage) -> bool {
    try {
        auto loaded = std::make_unique<Document>(&this->documentHandler);
        if (!loaded->readPdf(path, true, attachToDocument)) {
            if (errorMessage) {
                *errorMessage = loaded->getLastErrorMsg();
            }
            return false;
        }
        loaded->setFilepath(path);
        this->document = std::move(loaded);
        this->loadedPath = path;
        clearPdfRasterCache();
        clearHistory();
        clearGeometryHistory();
        clearInteractiveGeometryState();
        this->activePdfTextSelection.reset();
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

auto QtDocumentController::hasPdfBackgroundDocument() const -> bool {
    if (!this->document) {
        return false;
    }
    this->document->lock_shared();
    const bool hasPdf = this->document->getPdfPageCount() > 0U;
    this->document->unlock_shared();
    return hasPdf;
}

auto QtDocumentController::snapshotPages() const -> const std::vector<vn::view::render::PageRenderSnapshot>& {
    return this->pageSnapshots;
}

void QtDocumentController::preparePdfRasterCache(const std::vector<std::size_t>& visiblePageIndices) {
    if (!this->document || visiblePageIndices.empty() || this->pageSnapshots.empty()) {
        return;
    }

    std::unordered_set<std::size_t> wantedPageIndices;
    for (const auto pageIndex: visiblePageIndices) {
        const auto first = pageIndex >= static_cast<std::size_t>(this->pdfPreloadPagesBefore)
                                   ? pageIndex - static_cast<std::size_t>(this->pdfPreloadPagesBefore)
                                   : 0U;
        const auto last = std::min(this->pageSnapshots.size() - 1U,
                                   pageIndex + static_cast<std::size_t>(std::max(0, this->pdfPreloadPagesAfter)));
        for (std::size_t index = first; index <= last; ++index) {
            wantedPageIndices.insert(index);
        }
    }

    for (const auto pageIndex: wantedPageIndices) {
        if (pageIndex >= this->pageSnapshots.size()) {
            continue;
        }
        auto& snapshot = this->pageSnapshots[pageIndex];
        auto& background = snapshot.background;
        if (background.backgroundFormat != PageTypeFormat::Pdf) {
            continue;
        }
        background.rasterContent =
                cachedPdfRaster(background.pdfPageNumber, background.pageWidth, background.pageHeight);
    }

    if (this->pdfEagerPageCleanup) {
        std::unordered_set<std::size_t> wantedPdfPages;
        for (const auto pageIndex: wantedPageIndices) {
            if (pageIndex < this->pageSnapshots.size() &&
                this->pageSnapshots[pageIndex].background.backgroundFormat == PageTypeFormat::Pdf) {
                wantedPdfPages.insert(this->pageSnapshots[pageIndex].background.pdfPageNumber);
            }
        }
        this->pdfRasterCache.erase(
                std::remove_if(this->pdfRasterCache.begin(), this->pdfRasterCache.end(),
                               [&wantedPdfPages](const QtPdfRasterCacheEntry& entry) {
                                   return !wantedPdfPages.contains(entry.pdfPageNumber);
                               }),
                this->pdfRasterCache.end());
        for (auto& snapshot: this->pageSnapshots) {
            if (snapshot.background.backgroundFormat == PageTypeFormat::Pdf &&
                !wantedPdfPages.contains(snapshot.background.pdfPageNumber)) {
                snapshot.background.rasterContent = {};
            }
        }
    }

    prunePdfRasterCache();
}

void QtDocumentController::setPdfCacheOptions(int pageCacheSize, int preloadPagesBefore, int preloadPagesAfter,
                                              bool eagerCleanup, double pageRerenderThreshold) {
    this->pdfPageCacheSize = std::clamp(pageCacheSize, 1, 500);
    this->pdfPreloadPagesBefore = std::clamp(preloadPagesBefore, 0, 50);
    this->pdfPreloadPagesAfter = std::clamp(preloadPagesAfter, 0, 50);
    this->pdfEagerPageCleanup = eagerCleanup;
    this->pdfPageRerenderThreshold = std::clamp(pageRerenderThreshold, 0.0, 100.0);
    prunePdfRasterCache();
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
    if (!additive || !hit || !this->selectedGeometryHit ||
        this->selectedGeometryHit->pageIndex != hit->pageIndex ||
        this->selectedGeometryHit->hit.objectId != hit->hit.objectId) {
        this->selectedGeometryHit = std::move(hit);
        this->selectedGeometryVertexIds.clear();
        this->selectedGeometryEdgeIds.clear();
        if (this->selectedGeometryHit) {
            if (this->selectedGeometryHit->hit.type == vn::view::render::GeometryHitType::Vertex &&
                this->selectedGeometryHit->hit.vertexId != vn::geom::InvalidVertexId) {
                this->selectedGeometryVertexIds.push_back(this->selectedGeometryHit->hit.vertexId);
            }
            if (this->selectedGeometryHit->hit.type == vn::view::render::GeometryHitType::Edge &&
                this->selectedGeometryHit->hit.edgeId != vn::geom::InvalidEdgeId) {
                this->selectedGeometryEdgeIds.push_back(this->selectedGeometryHit->hit.edgeId);
            }
        }
        return;
    }

    this->selectedGeometryHit = std::move(hit);
    if (this->selectedGeometryHit) {
        if (this->selectedGeometryHit->hit.type == vn::view::render::GeometryHitType::Vertex &&
            this->selectedGeometryHit->hit.vertexId != vn::geom::InvalidVertexId &&
            std::find(this->selectedGeometryVertexIds.begin(), this->selectedGeometryVertexIds.end(),
                      this->selectedGeometryHit->hit.vertexId) == this->selectedGeometryVertexIds.end()) {
            this->selectedGeometryVertexIds.push_back(this->selectedGeometryHit->hit.vertexId);
        }
        if (this->selectedGeometryHit->hit.type == vn::view::render::GeometryHitType::Edge &&
            this->selectedGeometryHit->hit.edgeId != vn::geom::InvalidEdgeId &&
            std::find(this->selectedGeometryEdgeIds.begin(), this->selectedGeometryEdgeIds.end(),
                      this->selectedGeometryHit->hit.edgeId) == this->selectedGeometryEdgeIds.end()) {
            this->selectedGeometryEdgeIds.push_back(this->selectedGeometryHit->hit.edgeId);
        }
    }
}

void QtDocumentController::clearInteractiveGeometryState() {
    this->hoveredGeometryHit.reset();
    this->selectedGeometryHit.reset();
    this->selectedGeometryVertexIds.clear();
    this->selectedGeometryEdgeIds.clear();
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

auto QtDocumentController::selectedEdgeIds() const -> const std::vector<vn::geom::EdgeId>& {
    return this->selectedGeometryEdgeIds;
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
        if (auto provider = gridSnapProviderFor(page->getBackgroundType().format, options.gridSize, options.gridTolerance)) {
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
    this->pageSnapshots = vn::view::render::buildPageRenderSnapshots(
            *this->document, {.renderPdfBackgrounds = false});
}

auto QtDocumentController::cachedPdfRaster(std::size_t pdfPageNumber, double pageWidth, double pageHeight)
        -> vn::util::RasterImageData {
    const auto percentChange = [](double oldValue, double newValue) {
        const double average = (std::abs(oldValue) + std::abs(newValue)) / 2.0;
        return average <= std::numeric_limits<double>::epsilon() ? 0.0 : std::abs(oldValue - newValue) * 100.0 / average;
    };
    const auto sameSize = [this, pageWidth, pageHeight, percentChange](const QtPdfRasterCacheEntry& entry) {
        if (std::abs(entry.pageWidth - pageWidth) < 0.5 && std::abs(entry.pageHeight - pageHeight) < 0.5) {
            return true;
        }
        return std::max(percentChange(entry.pageWidth, pageWidth), percentChange(entry.pageHeight, pageHeight)) <=
               this->pdfPageRerenderThreshold;
    };
    for (auto& entry: this->pdfRasterCache) {
        if (entry.pdfPageNumber == pdfPageNumber && sameSize(entry)) {
            entry.lastUsed = ++this->pdfRasterUseCounter;
            return entry.raster;
        }
    }

    auto raster = vn::view::render::createPdfPagePreviewRaster(*this->document, pdfPageNumber, pageWidth, pageHeight);
    if (raster.empty()) {
        return raster;
    }

    this->pdfRasterCache.push_back(QtPdfRasterCacheEntry{.pdfPageNumber = pdfPageNumber,
                                                         .pageWidth = pageWidth,
                                                         .pageHeight = pageHeight,
                                                         .lastUsed = ++this->pdfRasterUseCounter,
                                                         .raster = raster});
    prunePdfRasterCache();
    return raster;
}

void QtDocumentController::prunePdfRasterCache() {
    const auto maxSize = static_cast<std::size_t>(std::clamp(this->pdfPageCacheSize, 1, 500));
    if (this->pdfRasterCache.size() <= maxSize) {
        return;
    }
    std::ranges::sort(this->pdfRasterCache, [](const auto& lhs, const auto& rhs) {
        return lhs.lastUsed > rhs.lastUsed;
    });
    this->pdfRasterCache.resize(maxSize);
}

void QtDocumentController::clearPdfRasterCache() {
    this->pdfRasterCache.clear();
    this->pdfRasterUseCounter = 0U;
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

auto QtDocumentController::gridSnapProviderFor(PageTypeFormat format, double gridSize, double gridTolerance)
        -> std::shared_ptr<const vn::snap::ISnapProvider> {
    switch (format) {
        case PageTypeFormat::Graph:
        case PageTypeFormat::Dotted:
        case PageTypeFormat::IsoDotted:
        case PageTypeFormat::IsoGraph:
            return std::make_shared<vn::snap::GridSnapProvider>(gridSize, gridSize, gridTolerance);
        default:
            return nullptr;
    }
}

// ---------------------------------------------------------------------------
// Stroke input
// ---------------------------------------------------------------------------

auto QtDocumentController::beginStroke(std::size_t pageIndex, double x, double y, double pressure, Color color,
                                       double width, StrokeTool::Value toolType, bool pressureSensitive,
                                       const std::string& lineStyle, int fill) -> bool {
    if (!this->document || pageIndex >= this->document->getPageCount()) {
        return false;
    }

    auto stroke = std::make_unique<Stroke>();
    stroke->setToolType(StrokeTool(toolType));
    stroke->setColor(color);
    stroke->setWidth(width);

    if (toolType == StrokeTool::HIGHLIGHTER) {
        stroke->setFill(128);
    } else if (fill > 0) {
        stroke->setFill(fill);
    }

    if (lineStyle != "plain" && !lineStyle.empty() && toolType == StrokeTool::PEN) {
        stroke->setLineStyle(StrokeStyle::parseStyle(lineStyle));
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

auto QtDocumentController::finalizeStroke(bool recognizeShape, double recognizerMinSize, bool snapRecognizedToGrid)
        -> bool {
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

    if (recognizeShape) {
        ShapeRecognizer recognizer;
        if (auto recognized = recognizer.recognizePatterns(stroke.get(), recognizerMinSize)) {
            recognized->setColor(stroke->getColor());
            recognized->setWidth(stroke->hasPressure() ? stroke->getAvgPressure() : stroke->getWidth());

            if (snapRecognizedToGrid) {
                const auto oldSnappedBounds = recognized->getSnappedBounds();
                Point topLeft(oldSnappedBounds.x, oldSnappedBounds.y);
                Point topLeftSnapped = Point(std::round(topLeft.x / 28.0) * 28.0, std::round(topLeft.y / 28.0) * 28.0);

                recognized->move(topLeftSnapped.x - topLeft.x, topLeftSnapped.y - topLeft.y);
                const auto snappedBounds = recognized->getSnappedBounds();
                Point bottomRight(snappedBounds.x + snappedBounds.width, snappedBounds.y + snappedBounds.height);
                Point bottomRightSnapped = Point(std::round(bottomRight.x / 28.0) * 28.0,
                                                 std::round(bottomRight.y / 28.0) * 28.0);

                const double fx = std::abs(snappedBounds.width) > std::numeric_limits<double>::epsilon()
                                          ? (bottomRightSnapped.x - topLeftSnapped.x) / snappedBounds.width
                                          : 1.0;
                const double fy = std::abs(snappedBounds.height) > std::numeric_limits<double>::epsilon()
                                          ? (bottomRightSnapped.y - topLeftSnapped.y) / snappedBounds.height
                                          : 1.0;
                recognized->scale(topLeftSnapped.x, topLeftSnapped.y, fx, fy, 0.0, false);
            }

            stroke = std::move(recognized);
        }
    }

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
// Document save
// ---------------------------------------------------------------------------

auto QtDocumentController::saveDocument(const std::filesystem::path& path, std::string* errorMessage) -> bool {
    if (!this->document) {
        if (errorMessage) {
            *errorMessage = "No document to save.";
        }
        return false;
    }

    SaveHandler handler;
    this->document->lock();
    handler.prepareSave(this->document.get(), path);
    this->document->unlock();

    handler.saveTo(path);

    const auto& err = handler.getErrorMessage();
    if (!err.empty()) {
        if (errorMessage) {
            *errorMessage = err;
        }
        return false;
    }
    this->loadedPath = path;
    return true;
}

auto QtDocumentController::documentPtr() const -> const Document* { return this->document.get(); }

// ---------------------------------------------------------------------------
// Image insertion
// ---------------------------------------------------------------------------

auto QtDocumentController::insertImage(std::size_t pageIndex, double x, double y, const std::string& imageData,
                                       double width, double height) -> const Element* {
    if (!this->document || pageIndex >= this->document->getPageCount()) {
        return nullptr;
    }
    this->document->lock();
    auto page = this->document->getPage(pageIndex);
    auto* layer = page ? page->getSelectedLayer() : nullptr;
    if (!layer) {
        this->document->unlock();
        return nullptr;
    }

    auto img = std::make_unique<Image>();
    img->setX(x);
    img->setY(y);
    img->setWidth(width);
    img->setHeight(height);
    img->setImage(std::string(imageData));

    const auto* ptr = img.get();
    layer->addElement(std::move(img));

    // Push to undo history (reuse stroke-style undo: remove on undo, re-insert on redo)
    QtStrokeHistoryEntry entry;
    entry.pageIndex = pageIndex;
    entry.element = ptr;
    entry.text = "Insert image";
    pushHistory(QtHistoryEntry{std::move(entry)});

    this->document->unlock();
    rebuildPageSnapshots();
    return ptr;
}

auto QtDocumentController::insertElement(std::size_t pageIndex, ElementPtr element, std::string historyText)
        -> const Element* {
    if (!this->document || !element || pageIndex >= this->document->getPageCount()) {
        return nullptr;
    }

    this->document->lock();
    auto page = this->document->getPage(pageIndex);
    auto* layer = page ? page->getSelectedLayer() : nullptr;
    if (!layer) {
        this->document->unlock();
        return nullptr;
    }

    const auto* ptr = element.get();
    layer->addElement(std::move(element));
    pushHistory(QtHistoryEntry{
            QtStrokeHistoryEntry{.pageIndex = pageIndex, .element = ptr, .text = std::move(historyText)}});
    this->document->unlock();
    rebuildPageSnapshots();
    return ptr;
}

// ---------------------------------------------------------------------------
// Text search
// ---------------------------------------------------------------------------

auto QtDocumentController::findTextInDocument(const std::string& query) const
        -> std::vector<TextSearchResult> {
    std::vector<TextSearchResult> results;
    if (!this->document || query.empty()) {
        return results;
    }

    for (std::size_t pi = 0; pi < this->document->getPageCount(); ++pi) {
        auto page = this->document->getPage(pi);
        if (!page) {
            continue;
        }
        for (auto* layer: page->getLayers()) {
            if (!layer) {
                continue;
            }
            for (const auto& elem: layer->getElements()) {
                if (!elem || elem->getType() != ELEMENT_TEXT) {
                    continue;
                }
                auto* t = dynamic_cast<const Text*>(elem.get());
                if (!t) {
                    continue;
                }
                const auto& text = t->getText();
                // Case-insensitive substring search
                std::string lowerText = text;
                std::string lowerQuery = query;
                std::transform(lowerText.begin(), lowerText.end(), lowerText.begin(),
                               [](unsigned char c) { return std::tolower(c); });
                std::transform(lowerQuery.begin(), lowerQuery.end(), lowerQuery.begin(),
                               [](unsigned char c) { return std::tolower(c); });
                if (lowerText.find(lowerQuery) != std::string::npos) {
                    results.push_back({.pageIndex = pi, .textElement = t, .matchContext = text});
                }
            }
        }
    }
    return results;
}

// ---------------------------------------------------------------------------
// Geometry constraints
// ---------------------------------------------------------------------------

auto QtDocumentController::applyConstraint(vn::geom::ConstraintKind kind) -> bool {
    if (!this->selectedGeometryHit) {
        return false;
    }

    auto* geometry = findMutableGeometryElement(this->selectedGeometryHit->pageIndex,
                                                this->selectedGeometryHit->hit.objectId);
    if (!geometry) {
        return false;
    }

    auto before = geometry->geometry();
    auto after = before;

    try {
        switch (kind) {
            case vn::geom::ConstraintKind::Coincident:
                if (this->selectedGeometryVertexIds.size() < 2U) {
                    return false;
                }
                after.addConstraint(kind, this->selectedGeometryVertexIds);
                break;
            case vn::geom::ConstraintKind::Horizontal:
            case vn::geom::ConstraintKind::Vertical:
                if (this->selectedGeometryVertexIds.size() != 2U) {
                    return false;
                }
                after.addConstraint(kind, {this->selectedGeometryVertexIds[0], this->selectedGeometryVertexIds[1]});
                break;
            case vn::geom::ConstraintKind::FixedLength: {
                if (this->selectedGeometryVertexIds.size() != 2U) {
                    return false;
                }
                const auto* v0 = before.vertex(this->selectedGeometryVertexIds[0]);
                const auto* v1 = before.vertex(this->selectedGeometryVertexIds[1]);
                if (!v0 || !v1) {
                    return false;
                }
                const double length =
                        std::hypot(v1->position.x - v0->position.x, v1->position.y - v0->position.y);
                if (length <= 0.0) {
                    return false;
                }
                after.addConstraint(kind, {this->selectedGeometryVertexIds[0], this->selectedGeometryVertexIds[1]}, {},
                                    length);
                break;
            }
            case vn::geom::ConstraintKind::Parallel:
            case vn::geom::ConstraintKind::Perpendicular:
                if (this->selectedGeometryEdgeIds.size() != 2U) {
                    return false;
                }
                for (const auto edgeId: this->selectedGeometryEdgeIds) {
                    const auto* edge = before.edge(edgeId);
                    if (!edge || (edge->kind != vn::geom::EdgeKind::Line &&
                                  edge->kind != vn::geom::EdgeKind::ConstructionLine)) {
                        return false;
                    }
                }
                after.addConstraint(kind, {}, {this->selectedGeometryEdgeIds[0], this->selectedGeometryEdgeIds[1]});
                break;
            case vn::geom::ConstraintKind::Radius: {
                if (this->selectedGeometryEdgeIds.size() != 1U) {
                    return false;
                }
                const auto* edge = before.edge(this->selectedGeometryEdgeIds.front());
                if (!edge || (edge->kind != vn::geom::EdgeKind::Arc &&
                              edge->kind != vn::geom::EdgeKind::ConstructionCircle) ||
                    edge->controls.empty()) {
                    return false;
                }
                const auto* center = before.vertex(edge->controls.front());
                const auto* start = before.vertex(edge->start);
                if (!center || !start) {
                    return false;
                }
                const double radius =
                        std::hypot(start->position.x - center->position.x, start->position.y - center->position.y);
                if (radius <= 0.0) {
                    return false;
                }
                after.addConstraint(kind, {}, {this->selectedGeometryEdgeIds.front()}, radius);
                break;
            }
            case vn::geom::ConstraintKind::EqualLength:
            case vn::geom::ConstraintKind::FixedAngle:
            case vn::geom::ConstraintKind::OnEdge:
                return false;
        }
    } catch (const std::invalid_argument&) {
        return false;
    }

    // Run the constraint solver
    vn::constraints::GeometryConstraintSolver solver;
    (void)solver.apply(after);

    geometry->replaceGeometry(after);
    pushGeometryHistory({.pageIndex = this->selectedGeometryHit->pageIndex,
                         .objectId = this->selectedGeometryHit->hit.objectId,
                         .before = std::move(before),
                         .after = geometry->geometry(),
                         .text = "Apply constraint"});
    rebuildPageSnapshots();
    return true;
}

auto QtDocumentController::deleteSelectedConstraints() -> bool {
    if (!this->selectedGeometryHit) {
        return false;
    }

    auto* geometry = findMutableGeometryElement(this->selectedGeometryHit->pageIndex,
                                                this->selectedGeometryHit->hit.objectId);
    if (!geometry) {
        return false;
    }

    if (this->selectedGeometryVertexIds.empty() && this->selectedGeometryEdgeIds.empty()) {
        return false;
    }

    const auto before = geometry->geometry();
    auto after = before;
    std::vector<vn::geom::ConstraintId> removedIds;

    for (const auto& constraint: before.constraints()) {
        bool intersects = false;
        for (const auto& vid: constraint.vertices) {
            if (std::find(this->selectedGeometryVertexIds.begin(), this->selectedGeometryVertexIds.end(), vid) !=
                this->selectedGeometryVertexIds.end()) {
                intersects = true;
                break;
            }
        }
        if (!intersects) {
            for (const auto& eid: constraint.edges) {
                if (std::find(this->selectedGeometryEdgeIds.begin(), this->selectedGeometryEdgeIds.end(), eid) !=
                    this->selectedGeometryEdgeIds.end()) {
                    intersects = true;
                    break;
                }
            }
        }
        if (intersects) {
            removedIds.push_back(constraint.id);
        }
    }

    if (removedIds.empty()) {
        return false;
    }

    for (const auto id: removedIds) {
        (void)after.removeConstraint(id);
    }

    geometry->replaceGeometry(after);
    pushGeometryHistory({.pageIndex = this->selectedGeometryHit->pageIndex,
                         .objectId = this->selectedGeometryHit->hit.objectId,
                         .before = std::move(before),
                         .after = geometry->geometry(),
                         .text = "Delete constraint"});
    rebuildPageSnapshots();
    return true;
}

auto QtDocumentController::selectedFixedLengthConstraint() -> std::optional<vn::geom::Constraint> {
    if (!this->selectedGeometryHit) {
        return std::nullopt;
    }
    const auto* geometry = findMutableGeometryElement(this->selectedGeometryHit->pageIndex,
                                                     this->selectedGeometryHit->hit.objectId);
    if (!geometry) {
        return std::nullopt;
    }

    for (const auto& constraint: geometry->geometry().constraints()) {
        if (constraint.kind != vn::geom::ConstraintKind::FixedLength &&
            constraint.kind != vn::geom::ConstraintKind::Radius) {
            continue;
        }
        // Check if any selected vertex/edge is part of this constraint
        for (const auto& vid: constraint.vertices) {
            if (std::find(this->selectedGeometryVertexIds.begin(), this->selectedGeometryVertexIds.end(), vid) !=
                this->selectedGeometryVertexIds.end()) {
                return constraint;
            }
        }
        for (const auto& eid: constraint.edges) {
            if (std::find(this->selectedGeometryEdgeIds.begin(), this->selectedGeometryEdgeIds.end(), eid) !=
                this->selectedGeometryEdgeIds.end()) {
                return constraint;
            }
        }
    }
    return std::nullopt;
}

auto QtDocumentController::updateFixedLengthConstraint(double value) -> bool {
    if (!this->selectedGeometryHit || value <= 0.0) {
        return false;
    }

    auto* geometry = findMutableGeometryElement(this->selectedGeometryHit->pageIndex,
                                                this->selectedGeometryHit->hit.objectId);
    if (!geometry) {
        return false;
    }

    auto constraint = selectedFixedLengthConstraint();
    if (!constraint) {
        return false;
    }

    auto before = geometry->geometry();
    auto after = before;

    auto updatedConstraint = *constraint;
    updatedConstraint.value = value;
    (void)after.replaceConstraint(updatedConstraint);

    vn::constraints::GeometryConstraintSolver solver;
    (void)solver.apply(after);

    geometry->replaceGeometry(after);
    pushGeometryHistory({.pageIndex = this->selectedGeometryHit->pageIndex,
                         .objectId = this->selectedGeometryHit->hit.objectId,
                         .before = std::move(before),
                         .after = geometry->geometry(),
                         .text = "Edit constraint value"});
    rebuildPageSnapshots();
    return true;
}

// ---------------------------------------------------------------------------
// Shape creation (geometry-based)
// ---------------------------------------------------------------------------

namespace {

auto insertGeometryOnPage(Document* doc, std::size_t pageIndex,
                          std::unique_ptr<vn::geom::GeometryElement> geometry) -> const Element* {
    if (!doc || pageIndex >= doc->getPageCount()) {
        return nullptr;
    }
    doc->lock();
    auto page = doc->getPage(pageIndex);
    auto* layer = page ? page->getSelectedLayer() : nullptr;
    if (!layer) {
        doc->unlock();
        return nullptr;
    }
    const auto* ptr = geometry.get();
    layer->addElement(std::move(geometry));
    doc->unlock();
    return ptr;
}

auto insertStrokeOnPage(Document* doc, std::size_t pageIndex, std::unique_ptr<Stroke> stroke) -> const Element* {
    if (!doc || pageIndex >= doc->getPageCount()) {
        return nullptr;
    }
    doc->lock();
    auto page = doc->getPage(pageIndex);
    auto* layer = page ? page->getSelectedLayer() : nullptr;
    if (!layer) {
        doc->unlock();
        return nullptr;
    }
    const auto* ptr = stroke.get();
    layer->addElement(std::move(stroke));
    doc->unlock();
    return ptr;
}

void configureStrokeStyle(Stroke& stroke, Color color, double width, const std::string& lineStyle, int fill) {
    stroke.setToolType(StrokeTool::PEN);
    stroke.setColor(color);
    stroke.setWidth(width);
    if (!lineStyle.empty() && lineStyle != "plain") {
        stroke.setLineStyle(StrokeStyle::parseStyle(lineStyle));
    }
    if (fill >= 0) {
        stroke.setFill(fill);
    }
}

auto buildEllipsePoints(double x1, double y1, double x2, double y2) -> std::vector<Point> {
    const double width = x2 - x1;
    const double height = y2 - y1;
    const double radiusX = width * 0.5;
    const double radiusY = height * 0.5;
    const double centerX = x1 + radiusX;
    const double centerY = y1 + radiusY;
    const auto nbPtsPerQuadrant =
            static_cast<unsigned int>(std::ceil(5.0 + 0.3 * (std::abs(radiusX) + std::abs(radiusY))));
    const double stepAngle = M_PI_2 / std::max(1U, nbPtsPerQuadrant);

    std::vector<Point> shape;
    shape.reserve(4 * nbPtsPerQuadrant + 1);
    shape.emplace_back(centerX + radiusX, centerY);
    for (unsigned int j = 1U; j < nbPtsPerQuadrant; ++j) {
        const double tgtAngle = stepAngle * j;
        const double centerAngle = 0.25 * std::atan2(std::abs(radiusY) * std::sin(tgtAngle),
                                                     std::abs(radiusX) * std::cos(tgtAngle)) +
                                   0.75 * tgtAngle;
        shape.emplace_back(centerX + radiusX * std::cos(centerAngle), centerY + radiusY * std::sin(centerAngle));
    }
    shape.emplace_back(centerX, centerY + radiusY);

    std::vector<Point> firstHalf = shape;
    for (auto it = std::next(firstHalf.rbegin()); it != firstHalf.rend(); ++it) {
        shape.emplace_back(2 * centerX - it->x, it->y);
    }
    std::vector<Point> upperHalf = shape;
    for (auto it = std::next(upperHalf.rbegin()); it != upperHalf.rend(); ++it) {
        shape.emplace_back(it->x, 2 * centerY - it->y);
    }
    if (!shape.empty()) {
        shape.emplace_back(shape.front());
    }
    return shape;
}

auto buildArrowPoints(double x1, double y1, double x2, double y2, double thickness, bool doubleEnded)
        -> std::vector<Point> {
    const Point start(x1, y1);
    const Point end(x2, y2);
    const double lineLength = std::hypot(end.x - start.x, end.y - start.y);
    if (lineLength <= 0.0001) {
        return {start, end};
    }

    const double safeThickness = std::max(0.5, thickness);
    const double slimness = lineLength / safeThickness;
    double delta = M_PI / 6.0;
    constexpr double THICK1 = 7.0;
    constexpr double THICK3 = 1.6;
    constexpr double LENGTH2 = 0.4;
    constexpr double LENGTH4 = 0.8;
    constexpr double LENGTH4_DOUBLE = 0.5;
    double arrowDist = safeThickness * THICK1;
    if (slimness >= THICK1 / LENGTH2) {
        // keep default
    } else if (slimness >= THICK3 / LENGTH2) {
        arrowDist = lineLength * LENGTH2;
    } else if (slimness >= THICK3 / (doubleEnded ? LENGTH4_DOUBLE : LENGTH4)) {
        arrowDist = safeThickness * THICK3;
        delta = (1 + (slimness - THICK3 / LENGTH2) /
                            (THICK3 / (doubleEnded ? LENGTH4_DOUBLE : LENGTH4) - THICK3 / LENGTH2)) *
                M_PI / 6.0;
        arrowDist *= std::sin(M_PI / 6.0) / std::sin(delta);
    } else {
        arrowDist = lineLength * (doubleEnded ? LENGTH4_DOUBLE : LENGTH4);
        delta = M_PI / 3.0;
        arrowDist *= std::sin(M_PI / 6.0) / std::sin(M_PI / 3.0);
    }

    const double angle = std::atan2(end.y - start.y, end.x - start.x);
    std::vector<Point> shape;
    shape.reserve(doubleEnded ? 10 : 6);
    shape.emplace_back(start);
    if (doubleEnded) {
        shape.emplace_back(start.x + arrowDist * std::cos(angle + delta),
                           start.y + arrowDist * std::sin(angle + delta));
        shape.emplace_back(start);
        shape.emplace_back(start.x + arrowDist * std::cos(angle - delta),
                           start.y + arrowDist * std::sin(angle - delta));
        shape.emplace_back(start);
    }
    shape.emplace_back(end);
    shape.emplace_back(end.x - arrowDist * std::cos(angle + delta), end.y - arrowDist * std::sin(angle + delta));
    shape.emplace_back(end);
    shape.emplace_back(end.x - arrowDist * std::cos(angle - delta), end.y - arrowDist * std::sin(angle - delta));
    shape.emplace_back(end);
    return shape;
}

auto buildCoordinateSystemPoints(double x1, double y1, double x2, double y2) -> std::vector<Point> {
    return {Point(x1, y1), Point(x1, y2), Point(x2, y2)};
}

auto buildSplinePoints(const std::vector<std::pair<double, double>>& points) -> std::vector<Point> {
    std::vector<Point> knots;
    knots.reserve(points.size());
    for (const auto& [x, y]: points) {
        knots.emplace_back(x, y);
    }
    if (knots.size() <= 2U) {
        return knots;
    }

    std::vector<Point> result;
    result.reserve(knots.size() * 8U);
    for (std::size_t index = 0; index + 1 < knots.size(); ++index) {
        const Point& p0 = index == 0 ? knots[index] : knots[index - 1];
        const Point& p1 = knots[index];
        const Point& p2 = knots[index + 1];
        const Point& p3 = (index + 2 < knots.size()) ? knots[index + 2] : knots[index + 1];

        const Point c1(p1.x + (p2.x - p0.x) / 6.0, p1.y + (p2.y - p0.y) / 6.0);
        const Point c2(p2.x - (p3.x - p1.x) / 6.0, p2.y - (p3.y - p1.y) / 6.0);
        SplineSegment segment(p1, c1, c2, p2);
        auto segmentPoints = segment.toPointSequence();
        result.insert(result.end(), segmentPoints.begin(), segmentPoints.end());
    }
    result.emplace_back(knots.back());
    return result;
}

auto insertLegacyStroke(Document* doc, std::size_t pageIndex, const std::vector<std::pair<double, double>>& points,
                        Color color, double width, const std::string& lineStyle, std::string_view actionText)
        -> std::pair<const Element*, QtStrokeHistoryEntry> {
    if (points.size() < 2U) {
        return {nullptr, QtStrokeHistoryEntry{}};
    }

    auto stroke = std::make_unique<Stroke>();
    configureStrokeStyle(*stroke, color, width, lineStyle, -1);
    std::vector<Point> strokePoints;
    strokePoints.reserve(points.size());
    for (const auto& [x, y]: points) {
        strokePoints.emplace_back(x, y);
    }
    stroke->setPointVector(std::move(strokePoints));
    stroke->freeUnusedPointItems();

    const auto* ptr = insertStrokeOnPage(doc, pageIndex, std::move(stroke));
    QtStrokeHistoryEntry entry;
    if (ptr) {
        entry.pageIndex = pageIndex;
        entry.element = ptr;
        entry.text = std::string(actionText);
    }
    return {ptr, std::move(entry)};
}

}  // namespace

auto QtDocumentController::createLine(std::size_t pageIndex, double x1, double y1, double x2, double y2, Color color,
                                      double width) -> const Element* {
    vn::geom::GeometryObject object(vn::geom::GeometryIdGenerator::nextObjectId());
    auto start = object.addVertex({x1, y1});
    auto end = object.addVertex({x2, y2});
    object.addLine(start, end);

    auto geometry = std::make_unique<vn::geom::GeometryElement>(std::move(object));
    geometry->setColor(color);
    geometry->setStrokeWidth(width);

    const auto* ptr = insertGeometryOnPage(this->document.get(), pageIndex, std::move(geometry));
    if (ptr) {
        QtStrokeHistoryEntry entry;
        entry.pageIndex = pageIndex;
        entry.element = ptr;
        entry.text = "Draw line";
        pushHistory(QtHistoryEntry{std::move(entry)});
        rebuildPageSnapshots();
    }
    return ptr;
}

auto QtDocumentController::createRectangle(std::size_t pageIndex, double x1, double y1, double x2, double y2,
                                           Color color, double width) -> const Element* {
    vn::geom::GeometryObject object(vn::geom::GeometryIdGenerator::nextObjectId());
    auto topLeft = object.addVertex({x1, y1});
    auto topRight = object.addVertex({x2, y1});
    auto bottomRight = object.addVertex({x2, y2});
    auto bottomLeft = object.addVertex({x1, y2});
    object.addLine(topLeft, topRight);
    object.addLine(topRight, bottomRight);
    object.addLine(bottomRight, bottomLeft);
    object.addLine(bottomLeft, topLeft);

    auto geometry = std::make_unique<vn::geom::GeometryElement>(std::move(object));
    geometry->setColor(color);
    geometry->setStrokeWidth(width);

    const auto* ptr = insertGeometryOnPage(this->document.get(), pageIndex, std::move(geometry));
    if (ptr) {
        QtStrokeHistoryEntry entry;
        entry.pageIndex = pageIndex;
        entry.element = ptr;
        entry.text = "Draw rectangle";
        pushHistory(QtHistoryEntry{std::move(entry)});
        rebuildPageSnapshots();
    }
    return ptr;
}

auto QtDocumentController::createCircle(std::size_t pageIndex, double cx, double cy, double rx, double ry, Color color,
                                        double width) -> const Element* {
    vn::geom::GeometryObject object(vn::geom::GeometryIdGenerator::nextObjectId());
    auto center = object.addVertex({cx, cy});
    auto radiusPoint = object.addVertex({rx, ry});
    object.addEdge(vn::geom::EdgeKind::Arc, radiusPoint, radiusPoint, {center});

    auto geometry = std::make_unique<vn::geom::GeometryElement>(std::move(object));
    geometry->setColor(color);
    geometry->setStrokeWidth(width);

    const auto* ptr = insertGeometryOnPage(this->document.get(), pageIndex, std::move(geometry));
    if (ptr) {
        QtStrokeHistoryEntry entry;
        entry.pageIndex = pageIndex;
        entry.element = ptr;
        entry.text = "Draw circle";
        pushHistory(QtHistoryEntry{std::move(entry)});
        rebuildPageSnapshots();
    }
    return ptr;
}

auto QtDocumentController::createEllipse(std::size_t pageIndex, double x1, double y1, double x2, double y2,
                                         Color color, double width, const std::string& lineStyle, int fill)
        -> const Element* {
    auto stroke = std::make_unique<Stroke>();
    configureStrokeStyle(*stroke, color, width, lineStyle, fill);
    stroke->setPointVector(buildEllipsePoints(x1, y1, x2, y2));
    stroke->freeUnusedPointItems();

    const auto* ptr = insertStrokeOnPage(this->document.get(), pageIndex, std::move(stroke));
    if (ptr) {
        QtStrokeHistoryEntry entry;
        entry.pageIndex = pageIndex;
        entry.element = ptr;
        entry.text = "Draw ellipse";
        pushHistory(QtHistoryEntry{std::move(entry)});
        rebuildPageSnapshots();
    }
    return ptr;
}

auto QtDocumentController::createArc(std::size_t pageIndex, double cx, double cy, double sx, double sy, double ex,
                                     double ey, Color color, double width) -> const Element* {
    vn::geom::GeometryObject object(vn::geom::GeometryIdGenerator::nextObjectId());
    auto center = object.addVertex({cx, cy});
    auto start = object.addVertex({sx, sy});
    auto end = object.addVertex({ex, ey});
    object.addEdge(vn::geom::EdgeKind::Arc, start, end, {center});

    auto geometry = std::make_unique<vn::geom::GeometryElement>(std::move(object));
    geometry->setColor(color);
    geometry->setStrokeWidth(width);

    const auto* ptr = insertGeometryOnPage(this->document.get(), pageIndex, std::move(geometry));
    if (ptr) {
        QtStrokeHistoryEntry entry;
        entry.pageIndex = pageIndex;
        entry.element = ptr;
        entry.text = "Draw arc";
        pushHistory(QtHistoryEntry{std::move(entry)});
        rebuildPageSnapshots();
    }
    return ptr;
}

auto QtDocumentController::createArrow(std::size_t pageIndex, double x1, double y1, double x2, double y2, Color color,
                                       double width, const std::string& lineStyle, bool doubleEnded)
        -> const Element* {
    auto stroke = std::make_unique<Stroke>();
    configureStrokeStyle(*stroke, color, width, lineStyle, -1);
    stroke->setPointVector(buildArrowPoints(x1, y1, x2, y2, width, doubleEnded));
    stroke->freeUnusedPointItems();

    const auto* ptr = insertStrokeOnPage(this->document.get(), pageIndex, std::move(stroke));
    if (ptr) {
        QtStrokeHistoryEntry entry;
        entry.pageIndex = pageIndex;
        entry.element = ptr;
        entry.text = doubleEnded ? "Draw double arrow" : "Draw arrow";
        pushHistory(QtHistoryEntry{std::move(entry)});
        rebuildPageSnapshots();
    }
    return ptr;
}

auto QtDocumentController::createCoordinateSystem(std::size_t pageIndex, double x1, double y1, double x2, double y2,
                                                  Color color, double width, const std::string& lineStyle)
        -> const Element* {
    auto stroke = std::make_unique<Stroke>();
    configureStrokeStyle(*stroke, color, width, lineStyle, -1);
    stroke->setPointVector(buildCoordinateSystemPoints(x1, y1, x2, y2));
    stroke->freeUnusedPointItems();

    const auto* ptr = insertStrokeOnPage(this->document.get(), pageIndex, std::move(stroke));
    if (ptr) {
        QtStrokeHistoryEntry entry;
        entry.pageIndex = pageIndex;
        entry.element = ptr;
        entry.text = "Draw coordinate system";
        pushHistory(QtHistoryEntry{std::move(entry)});
        rebuildPageSnapshots();
    }
    return ptr;
}

auto QtDocumentController::createSpline(std::size_t pageIndex, const std::vector<std::pair<double, double>>& points,
                                        Color color, double width, const std::string& lineStyle) -> const Element* {
    auto linearizedPoints = buildSplinePoints(points);
    if (linearizedPoints.size() < 2U) {
        return nullptr;
    }

    auto stroke = std::make_unique<Stroke>();
    configureStrokeStyle(*stroke, color, width, lineStyle, -1);
    stroke->setPointVector(std::move(linearizedPoints));
    stroke->freeUnusedPointItems();

    const auto* ptr = insertStrokeOnPage(this->document.get(), pageIndex, std::move(stroke));
    if (ptr) {
        QtStrokeHistoryEntry entry;
        entry.pageIndex = pageIndex;
        entry.element = ptr;
        entry.text = "Draw spline";
        pushHistory(QtHistoryEntry{std::move(entry)});
        rebuildPageSnapshots();
    }
    return ptr;
}

auto QtDocumentController::createSetsquareStroke(std::size_t pageIndex,
                                                 const std::vector<std::pair<double, double>>& points, Color color,
                                                 double width, const std::string& lineStyle) -> const Element* {
    auto [ptr, entry] =
            insertLegacyStroke(this->document.get(), pageIndex, points, color, width, lineStyle, "Draw setsquare stroke");
    if (ptr) {
        pushHistory(QtHistoryEntry{std::move(entry)});
        rebuildPageSnapshots();
    }
    return ptr;
}

auto QtDocumentController::createCompassStroke(std::size_t pageIndex,
                                               const std::vector<std::pair<double, double>>& points, Color color,
                                               double width, const std::string& lineStyle) -> const Element* {
    auto [ptr, entry] =
            insertLegacyStroke(this->document.get(), pageIndex, points, color, width, lineStyle, "Draw compass stroke");
    if (ptr) {
        pushHistory(QtHistoryEntry{std::move(entry)});
        rebuildPageSnapshots();
    }
    return ptr;
}

auto QtDocumentController::createPolyline(std::size_t pageIndex,
                                          const std::vector<std::pair<double, double>>& points, Color color,
                                          double width) -> const Element* {
    if (points.size() < 2U) {
        return nullptr;
    }

    vn::geom::GeometryObject object(vn::geom::GeometryIdGenerator::nextObjectId());
    std::vector<vn::geom::VertexId> vertices;
    vertices.reserve(points.size());
    for (const auto& [x, y]: points) {
        vertices.push_back(object.addVertex({x, y}));
    }
    for (auto it = std::next(vertices.begin()); it != vertices.end(); ++it) {
        object.addLine(*std::prev(it), *it);
    }

    auto geometry = std::make_unique<vn::geom::GeometryElement>(std::move(object));
    geometry->setColor(color);
    geometry->setStrokeWidth(width);

    const auto* ptr = insertGeometryOnPage(this->document.get(), pageIndex, std::move(geometry));
    if (ptr) {
        QtStrokeHistoryEntry entry;
        entry.pageIndex = pageIndex;
        entry.element = ptr;
        entry.text = "Draw polyline";
        pushHistory(QtHistoryEntry{std::move(entry)});
        rebuildPageSnapshots();
    }
    return ptr;
}

auto QtDocumentController::createConstructionLine(std::size_t pageIndex, double x1, double y1, double x2, double y2,
                                                  Color color, double width) -> const Element* {
    vn::geom::GeometryObject object(vn::geom::GeometryIdGenerator::nextObjectId());
    auto start = object.addVertex({x1, y1});
    auto end = object.addVertex({x2, y2});
    object.addEdge(vn::geom::EdgeKind::ConstructionLine, start, end);

    auto geometry = std::make_unique<vn::geom::GeometryElement>(std::move(object));
    geometry->setColor(color);
    geometry->setStrokeWidth(width);

    const auto* ptr = insertGeometryOnPage(this->document.get(), pageIndex, std::move(geometry));
    if (ptr) {
        QtStrokeHistoryEntry entry;
        entry.pageIndex = pageIndex;
        entry.element = ptr;
        entry.text = "Draw construction line";
        pushHistory(QtHistoryEntry{std::move(entry)});
        rebuildPageSnapshots();
    }
    return ptr;
}

auto QtDocumentController::createConstructionCircle(std::size_t pageIndex, double cx, double cy, double rx, double ry,
                                                    Color color, double width) -> const Element* {
    vn::geom::GeometryObject object(vn::geom::GeometryIdGenerator::nextObjectId());
    auto center = object.addVertex({cx, cy});
    auto radiusPoint = object.addVertex({rx, ry});
    object.addEdge(vn::geom::EdgeKind::ConstructionCircle, radiusPoint, radiusPoint, {center});

    auto geometry = std::make_unique<vn::geom::GeometryElement>(std::move(object));
    geometry->setColor(color);
    geometry->setStrokeWidth(width);

    const auto* ptr = insertGeometryOnPage(this->document.get(), pageIndex, std::move(geometry));
    if (ptr) {
        QtStrokeHistoryEntry entry;
        entry.pageIndex = pageIndex;
        entry.element = ptr;
        entry.text = "Draw construction circle";
        pushHistory(QtHistoryEntry{std::move(entry)});
        rebuildPageSnapshots();
    }
    return ptr;
}
