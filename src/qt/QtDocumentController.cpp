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
#include "model/PathParameter.h"
#include "model/eraser/PaddedBox.h"
#include "util/SmallVector.h"
#include "vertexnote/constraints/GeometryConstraintSolver.h"
#include "vertexnote/geometry/GeometryElement.h"
#include "vertexnote/geometry/GeometryIdGenerator.h"
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

auto QtDocumentController::beginPdfTextSelection(std::size_t pageIndex, double x, double y, PdfPageSelectionStyle style)
        -> bool {
    if (!this->document || pageIndex >= this->document->getPageCount()) {
        return false;
    }

    this->document->lock_shared();
    auto page = this->document->getPage(pageIndex);
    const bool hasPdf = page && page->getPdfPageNr() != npos && this->document->getPdfPage(page->getPdfPageNr());
    this->document->unlock_shared();
    if (!hasPdf) {
        return false;
    }

    this->activePdfTextSelection = QtPdfTextSelectionState{
            .pageIndex = pageIndex, .style = style, .bounds = PdfRectangle(x, y, x, y), .previewRects = {}, .selectedText = {}, .finalized = false};
    return true;
}

auto QtDocumentController::updatePdfTextSelection(double x, double y) -> bool {
    if (!this->document || !this->activePdfTextSelection) {
        return false;
    }

    auto& selection = *this->activePdfTextSelection;
    this->document->lock_shared();
    auto page = selection.pageIndex < this->document->getPageCount() ? this->document->getPage(selection.pageIndex) : nullptr;
    auto pdfPage = page && page->getPdfPageNr() != npos ? this->document->getPdfPage(page->getPdfPageNr()) : nullptr;
    selection.bounds.x2 = x;
    selection.bounds.y2 = y;
    selection.previewRects.clear();
    if (pdfPage) {
        auto preview = pdfPage->selectTextLines(selection.bounds, selection.style);
        selection.previewRects = std::move(preview.rects);
    }
    this->document->unlock_shared();
    return true;
}

auto QtDocumentController::finalizePdfTextSelection() -> std::string {
    if (!this->document || !this->activePdfTextSelection) {
        return {};
    }

    auto selection = *this->activePdfTextSelection;
    this->document->lock_shared();
    auto page = selection.pageIndex < this->document->getPageCount() ? this->document->getPage(selection.pageIndex) : nullptr;
    auto pdfPage = page && page->getPdfPageNr() != npos ? this->document->getPdfPage(page->getPdfPageNr()) : nullptr;
    if (pdfPage) {
        auto finalized = pdfPage->selectTextLines(selection.bounds, selection.style);
        selection.previewRects = std::move(finalized.rects);
        selection.selectedText = pdfPage->selectText(selection.bounds, selection.style);
        selection.finalized = true;
    }
    this->document->unlock_shared();
    this->activePdfTextSelection = std::move(selection);
    return this->activePdfTextSelection->selectedText;
}

void QtDocumentController::cancelPdfTextSelection() { this->activePdfTextSelection.reset(); }

auto QtDocumentController::pdfTextSelection() const -> const std::optional<QtPdfTextSelectionState>& {
    return this->activePdfTextSelection;
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

auto QtDocumentController::elementsForPluginScope(std::string_view scope, ElementType type,
                                                  std::size_t currentPageIndex) const
        -> std::vector<QtPluginElementRef> {
    std::vector<QtPluginElementRef> result;
    if (!this->document) {
        return result;
    }

    const auto appendLayer = [&](std::size_t pageIndex, std::size_t layerIndex, const Layer* layer) {
        if (!layer) {
            return;
        }
        for (const auto* element: layer->getElementsView()) {
            if (element && element->getType() == type) {
                result.push_back(QtPluginElementRef{.element = element, .pageIndex = pageIndex, .layerIndex = layerIndex});
            }
        }
    };

    if (scope == "selection") {
        if (!this->currentSelection) {
            return result;
        }
        for (const auto* element: this->currentSelection->elements) {
            if (element && element->getType() == type) {
                result.push_back(QtPluginElementRef{
                        .element = element,
                        .pageIndex = this->currentSelection->pageIndex,
                        .layerIndex = 0U,
                });
            }
        }
        return result;
    }

    if (scope == "layer") {
        if (currentPageIndex >= this->document->getPageCount()) {
            return result;
        }
        auto page = this->document->getPage(currentPageIndex);
        const auto layerIndex = page ? static_cast<std::size_t>(page->getSelectedLayerId()) : 0U;
        auto layer = page ? page->getSelectedLayer() : nullptr;
        appendLayer(currentPageIndex, layerIndex, layer);
        return result;
    }

    const auto appendPage = [&](std::size_t pageIndex) {
        auto page = this->document->getPage(pageIndex);
        if (!page) {
            return;
        }
        const auto& layers = page->getLayers();
        for (std::size_t layerIndex = 0; layerIndex < layers.size(); ++layerIndex) {
            appendLayer(pageIndex, layerIndex, layers[layerIndex]);
        }
    };

    if (scope == "page") {
        if (currentPageIndex < this->document->getPageCount()) {
            appendPage(currentPageIndex);
        }
        return result;
    }

    if (scope == "all") {
        for (std::size_t pageIndex = 0; pageIndex < this->document->getPageCount(); ++pageIndex) {
            appendPage(pageIndex);
        }
    }
    return result;
}

auto QtDocumentController::selectElementsByPluginRefs(std::size_t pageIndex,
                                                      const std::vector<const Element*>& refs) -> bool {
    if (!this->document || pageIndex >= this->document->getPageCount()) {
        return false;
    }

    auto page = this->document->getPage(pageIndex);
    auto* layer = page ? page->getSelectedLayer() : nullptr;
    if (!layer) {
        return false;
    }

    std::vector<const Element*> selected;
    selected.reserve(refs.size());
    for (const auto* candidate: refs) {
        if (candidate && layer->indexOf(candidate) != Element::InvalidIndex &&
            std::find(selected.begin(), selected.end(), candidate) == selected.end()) {
            selected.push_back(candidate);
        }
    }

    if (selected.empty()) {
        this->currentSelection.reset();
        return false;
    }
    this->currentSelection = QtElementSelection{.pageIndex = pageIndex, .elements = std::move(selected)};
    return true;
}

auto QtDocumentController::colorSelectedElements(Color color) -> bool {
    if (!this->document || !this->currentSelection || this->currentSelection->elements.empty() ||
        this->currentSelection->pageIndex >= this->document->getPageCount()) {
        return false;
    }

    bool changed = false;
    this->document->lock();
    auto page = this->document->getPage(this->currentSelection->pageIndex);
    if (page) {
        for (auto* layer: page->getLayers()) {
            if (!layer) {
                continue;
            }
            for (auto& element: layer->getElements()) {
                const auto* ptr = element.get();
                if (ptr && std::find(this->currentSelection->elements.begin(), this->currentSelection->elements.end(),
                                     ptr) != this->currentSelection->elements.end()) {
                    element->setColor(color);
                    changed = true;
                }
            }
        }
    }
    this->document->unlock();
    if (changed) {
        rebuildPageSnapshots();
    }
    return changed;
}

// ---------------------------------------------------------------------------
// Element operations (delete, select all, clipboard)
// ---------------------------------------------------------------------------

auto QtDocumentController::deleteSelectedElements() -> bool {
    if (!this->currentSelection || this->currentSelection->elements.empty() || !this->document) {
        return false;
    }

    const auto pageIndex = this->currentSelection->pageIndex;
    if (pageIndex >= this->document->getPageCount()) {
        return false;
    }

    this->document->lock();
    auto page = this->document->getPage(pageIndex);
    if (!page) {
        this->document->unlock();
        return false;
    }

    InsertionOrder removedElements;
    for (const auto* elem: this->currentSelection->elements) {
        for (auto* layer: page->getLayers()) {
            if (!layer) {
                continue;
            }
            auto removed = layer->removeElement(elem);
            if (removed.e) {
                removedElements.push_back(std::move(removed));
                break;
            }
        }
    }
    this->document->unlock();

    if (removedElements.empty()) {
        return false;
    }

    std::vector<const Element*> ptrs;
    ptrs.reserve(removedElements.size());
    for (const auto& ip: removedElements) {
        ptrs.push_back(ip.e.get());
    }

    pushHistory(QtHistoryEntry{
            .data = QtDeleteHistoryEntry{.pageIndex = pageIndex,
                                         .removedElements = std::move(removedElements),
                                         .elementPtrs = std::move(ptrs),
                                         .text = "Delete selection"}});
    clearElementSelection();
    rebuildPageSnapshots();
    return true;
}

void QtDocumentController::selectAllElements(std::size_t pageIndex) {
    if (!this->document || pageIndex >= this->document->getPageCount()) {
        return;
    }

    auto page = this->document->getPage(pageIndex);
    if (!page) {
        return;
    }

    std::vector<const Element*> allElements;
    for (const Layer* layer: page->getLayersView()) {
        if (!layer || !layer->isVisible()) {
            continue;
        }
        for (const Element* element: layer->getElementsView()) {
            if (element) {
                allElements.push_back(element);
            }
        }
    }

    if (allElements.empty()) {
        this->currentSelection.reset();
    } else {
        this->currentSelection = QtElementSelection{.pageIndex = pageIndex, .elements = std::move(allElements)};
    }
}

auto QtDocumentController::copySelectedElements() -> std::vector<ElementPtr> {
    std::vector<ElementPtr> clones;
    if (!this->currentSelection || this->currentSelection->elements.empty()) {
        return clones;
    }

    clones.reserve(this->currentSelection->elements.size());
    for (const auto* elem: this->currentSelection->elements) {
        if (elem) {
            clones.push_back(elem->clone());
        }
    }
    return clones;
}

auto QtDocumentController::cutSelectedElements() -> std::vector<ElementPtr> {
    auto clones = copySelectedElements();
    if (!clones.empty()) {
        (void)deleteSelectedElements();
    }
    return clones;
}

auto QtDocumentController::pasteElements(std::size_t pageIndex, std::vector<ElementPtr> elements, double offsetX,
                                         double offsetY) -> bool {
    if (elements.empty() || !this->document || pageIndex >= this->document->getPageCount()) {
        return false;
    }

    this->document->lock();
    auto page = this->document->getPage(pageIndex);
    auto* layer = page ? page->getSelectedLayer() : nullptr;
    if (!layer) {
        this->document->unlock();
        return false;
    }

    std::vector<const Element*> pastedPtrs;
    pastedPtrs.reserve(elements.size());
    for (auto& elem: elements) {
        elem->move(offsetX, offsetY);
        pastedPtrs.push_back(elem.get());
        layer->addElement(std::move(elem));
    }
    this->document->unlock();

    // Select pasted elements
    this->currentSelection = QtElementSelection{.pageIndex = pageIndex, .elements = std::move(pastedPtrs)};
    rebuildPageSnapshots();
    return true;
}

// ---------------------------------------------------------------------------
// Z-order operations
// ---------------------------------------------------------------------------

namespace {
auto findElementLayer(NotePage* page, const Element* elem) -> Layer* {
    if (!page || !elem) {
        return nullptr;
    }
    for (auto* layer: page->getLayers()) {
        if (!layer) {
            continue;
        }
        for (const auto& e: layer->getElements()) {
            if (e.get() == elem) {
                return layer;
            }
        }
    }
    return nullptr;
}
}  // namespace

auto QtDocumentController::bringSelectionToFront() -> bool {
    if (!this->currentSelection || this->currentSelection->elements.empty() || !this->document) {
        return false;
    }
    const auto pageIndex = this->currentSelection->pageIndex;
    if (pageIndex >= this->document->getPageCount()) {
        return false;
    }
    this->document->lock();
    auto page = this->document->getPage(pageIndex);
    bool changed = false;
    for (const auto* elem: this->currentSelection->elements) {
        auto* layer = findElementLayer(page.get(), elem);
        if (!layer) {
            continue;
        }
        auto removed = layer->removeElement(elem);
        if (removed.e) {
            layer->addElement(std::move(removed.e));
            changed = true;
        }
    }
    this->document->unlock();
    if (changed) {
        rebuildPageSnapshots();
    }
    return changed;
}

auto QtDocumentController::sendSelectionToBack() -> bool {
    if (!this->currentSelection || this->currentSelection->elements.empty() || !this->document) {
        return false;
    }
    const auto pageIndex = this->currentSelection->pageIndex;
    if (pageIndex >= this->document->getPageCount()) {
        return false;
    }
    this->document->lock();
    auto page = this->document->getPage(pageIndex);
    bool changed = false;
    Element::Index insertIdx = 0;
    for (const auto* elem: this->currentSelection->elements) {
        auto* layer = findElementLayer(page.get(), elem);
        if (!layer) {
            continue;
        }
        auto removed = layer->removeElement(elem);
        if (removed.e) {
            layer->insertElement(std::move(removed.e), insertIdx);
            ++insertIdx;
            changed = true;
        }
    }
    this->document->unlock();
    if (changed) {
        rebuildPageSnapshots();
    }
    return changed;
}

auto QtDocumentController::bringSelectionForward() -> bool {
    if (!this->currentSelection || this->currentSelection->elements.empty() || !this->document) {
        return false;
    }
    const auto pageIndex = this->currentSelection->pageIndex;
    if (pageIndex >= this->document->getPageCount()) {
        return false;
    }
    this->document->lock();
    auto page = this->document->getPage(pageIndex);
    bool changed = false;
    for (const auto* elem: this->currentSelection->elements) {
        auto* layer = findElementLayer(page.get(), elem);
        if (!layer) {
            continue;
        }
        const auto idx = layer->indexOf(elem);
        if (idx == Element::InvalidIndex) {
            continue;
        }
        const auto maxIdx = static_cast<Element::Index>(layer->getElements().size()) - 1;
        if (idx < maxIdx) {
            auto removed = layer->removeElement(elem);
            if (removed.e) {
                layer->insertElement(std::move(removed.e), idx + 1);
                changed = true;
            }
        }
    }
    this->document->unlock();
    if (changed) {
        rebuildPageSnapshots();
    }
    return changed;
}

auto QtDocumentController::sendSelectionBackward() -> bool {
    if (!this->currentSelection || this->currentSelection->elements.empty() || !this->document) {
        return false;
    }
    const auto pageIndex = this->currentSelection->pageIndex;
    if (pageIndex >= this->document->getPageCount()) {
        return false;
    }
    this->document->lock();
    auto page = this->document->getPage(pageIndex);
    bool changed = false;
    for (const auto* elem: this->currentSelection->elements) {
        auto* layer = findElementLayer(page.get(), elem);
        if (!layer) {
            continue;
        }
        const auto idx = layer->indexOf(elem);
        if (idx == Element::InvalidIndex) {
            continue;
        }
        if (idx > 0) {
            auto removed = layer->removeElement(elem);
            if (removed.e) {
                layer->insertElement(std::move(removed.e), idx - 1);
                changed = true;
            }
        }
    }
    this->document->unlock();
    if (changed) {
        rebuildPageSnapshots();
    }
    return changed;
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

void QtDocumentController::copyLayer(std::size_t pageIndex, std::size_t layerIndex) {
    if (!this->document || pageIndex >= this->document->getPageCount()) {
        return;
    }
    this->document->lock();
    auto page = this->document->getPage(pageIndex);
    if (page) {
        auto& layers = page->getLayers();
        if (layerIndex < layers.size()) {
            Layer* cloned = layers[layerIndex]->clone();
            layers.insert(layers.begin() + static_cast<std::ptrdiff_t>(layerIndex) + 1, cloned);
            page->setSelectedLayerId(static_cast<Layer::Index>(layerIndex + 1));
        }
    }
    this->document->unlock();
    rebuildPageSnapshots();
}

void QtDocumentController::mergeLayerDown(std::size_t pageIndex, std::size_t layerIndex) {
    if (!this->document || pageIndex >= this->document->getPageCount()) {
        return;
    }
    if (layerIndex == 0) {
        return;  // Can't merge bottom layer down
    }
    this->document->lock();
    auto page = this->document->getPage(pageIndex);
    if (page) {
        auto& layers = page->getLayers();
        if (layerIndex < layers.size()) {
            Layer* srcLayer = layers[layerIndex];
            Layer* dstLayer = layers[layerIndex - 1];
            // Move all elements from source into destination
            auto elements = srcLayer->clearNoFree();
            for (auto& elem: elements) {
                dstLayer->addElement(std::move(elem));
            }
            // Remove empty source layer
            delete srcLayer;
            layers.erase(layers.begin() + static_cast<std::ptrdiff_t>(layerIndex));
            page->setSelectedLayerId(static_cast<Layer::Index>(layerIndex - 1));
        }
    }
    this->document->unlock();
    rebuildPageSnapshots();
}

void QtDocumentController::showAllLayers(std::size_t pageIndex) {
    if (!this->document || pageIndex >= this->document->getPageCount()) {
        return;
    }
    auto page = this->document->getPage(pageIndex);
    if (page) {
        for (auto* layer: page->getLayers()) {
            if (layer) {
                layer->setVisible(true);
            }
        }
    }
    rebuildPageSnapshots();
}

void QtDocumentController::hideAllLayers(std::size_t pageIndex) {
    if (!this->document || pageIndex >= this->document->getPageCount()) {
        return;
    }
    auto page = this->document->getPage(pageIndex);
    if (page) {
        for (auto* layer: page->getLayers()) {
            if (layer) {
                layer->setVisible(false);
            }
        }
    }
    rebuildPageSnapshots();
}

auto QtDocumentController::isPageAnnotated(std::size_t pageIndex) const -> bool {
    if (!this->document || pageIndex >= this->document->getPageCount()) {
        return false;
    }
    auto page = this->document->getPage(pageIndex);
    return page && page->isAnnotated();
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

auto QtDocumentController::canMoveSelectionToAdjacentLayer(int direction) const -> bool {
    if (!this->currentSelection || this->currentSelection->elements.empty() || !this->document ||
        this->currentSelection->pageIndex >= this->document->getPageCount() || direction == 0) {
        return false;
    }

    auto page = this->document->getPage(this->currentSelection->pageIndex);
    if (!page) {
        return false;
    }

    const auto& layers = page->getLayers();
    for (std::size_t layerIndex = 0; layerIndex < layers.size(); ++layerIndex) {
        const auto* layer = layers[layerIndex];
        if (!layer) {
            continue;
        }
        for (const auto* element: this->currentSelection->elements) {
            if (layer->indexOf(element) == Element::InvalidIndex) {
                continue;
            }

            if (direction > 0 && layerIndex + 1 < layers.size()) {
                return true;
            }
            if (direction < 0 && layerIndex > 0) {
                return true;
            }
        }
    }

    return false;
}

auto QtDocumentController::moveSelectionToAdjacentLayer(int direction) -> bool {
    if (!canMoveSelectionToAdjacentLayer(direction)) {
        return false;
    }

    this->document->lock();
    auto page = this->document->getPage(this->currentSelection->pageIndex);
    if (!page) {
        this->document->unlock();
        return false;
    }

    auto& layers = page->getLayers();
    std::vector<QtLayerTransferRecord> records;
    for (const auto* element: this->currentSelection->elements) {
        for (std::size_t fromLayerIndex = 0; fromLayerIndex < layers.size(); ++fromLayerIndex) {
            auto* sourceLayer = layers[fromLayerIndex];
            if (!sourceLayer) {
                continue;
            }

            const auto fromPos = sourceLayer->indexOf(element);
            if (fromPos == Element::InvalidIndex) {
                continue;
            }

            const auto toLayerIndex = direction > 0 ? fromLayerIndex + 1 : fromLayerIndex - 1;
            if (toLayerIndex >= layers.size() || !layers[toLayerIndex]) {
                break;
            }

            auto* targetLayer = layers[toLayerIndex];
            const auto toPos = static_cast<Element::Index>(targetLayer->getElementsView().size());
            auto removed = sourceLayer->removeElement(element);
            if (!removed.e) {
                break;
            }
            const auto* movedPtr = removed.e.get();
            targetLayer->insertElement(std::move(removed.e), toPos);
            records.push_back(QtLayerTransferRecord{.element = movedPtr,
                                                    .fromLayerIndex = fromLayerIndex,
                                                    .toLayerIndex = toLayerIndex,
                                                    .fromPos = fromPos,
                                                    .toPos = toPos});
            break;
        }
    }
    this->document->unlock();

    if (records.empty()) {
        return false;
    }

    pushHistory(QtHistoryEntry{QtLayerTransferHistoryEntry{
            .pageIndex = this->currentSelection->pageIndex,
            .records = std::move(records),
            .text = direction > 0 ? "Move selection layer up" : "Move selection layer down",
    }});
    rebuildPageSnapshots();
    return true;
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

auto QtDocumentController::changePagePdfBackground(std::size_t pageIndex, ptrdiff_t pageNumber, bool relative,
                                                   std::string* errorMessage) -> bool {
    const auto setError = [errorMessage](std::string message) {
        if (errorMessage) {
            *errorMessage = std::move(message);
        }
    };

    if (!this->document || pageIndex >= this->document->getPageCount()) {
        setError("No active page");
        return false;
    }
    if (this->document->getPdfPageCount() == 0U) {
        setError("The current document has no PDF background");
        return false;
    }

    this->document->lock();
    auto page = this->document->getPage(pageIndex);
    if (!page) {
        this->document->unlock();
        setError("No active page");
        return false;
    }

    ptrdiff_t selected = pageNumber - 1;
    if (relative) {
        if (!page->getBackgroundType().isPdfPage()) {
            this->document->unlock();
            setError("Current page has no PDF background");
            return false;
        }
        selected = static_cast<ptrdiff_t>(page->getPdfPageNr()) + pageNumber;
    }

    if (selected < 0 || static_cast<std::size_t>(selected) >= this->document->getPdfPageCount()) {
        this->document->unlock();
        setError("PDF page number does not exist");
        return false;
    }

    const auto pdfPageIndex = static_cast<std::size_t>(selected);
    auto pdfPage = this->document->getPdfPage(pdfPageIndex);
    if (!pdfPage) {
        this->document->unlock();
        setError("PDF page could not be loaded");
        return false;
    }

    page->setBackgroundPdfPageNr(pdfPageIndex);
    Document::setPageSize(page, pdfPage->getWidth(), pdfPage->getHeight());
    this->document->unlock();
    rebuildPageSnapshots();
    return true;
}

auto QtDocumentController::canResizePage(std::size_t pageIndex) const -> bool {
    if (!this->document || pageIndex >= this->document->getPageCount()) {
        return false;
    }
    auto page = this->document->getPage(pageIndex);
    return page && !page->getBackgroundType().isPdfPage();
}

auto QtDocumentController::resizePage(std::size_t pageIndex, double width, double height) -> bool {
    if (!canResizePage(pageIndex) || width <= 0.0 || height <= 0.0) {
        return false;
    }

    this->document->lock();
    auto page = this->document->getPage(pageIndex);
    if (!page) {
        this->document->unlock();
        return false;
    }

    const double beforeWidth = page->getWidth();
    const double beforeHeight = page->getHeight();
    if (std::abs(beforeWidth - width) < 0.01 && std::abs(beforeHeight - height) < 0.01) {
        this->document->unlock();
        return false;
    }

    Document::setPageSize(page, width, height);
    this->document->unlock();

    pushHistory(QtHistoryEntry{QtPageSizeHistoryEntry{.pageIndex = pageIndex,
                                                      .beforeWidth = beforeWidth,
                                                      .beforeHeight = beforeHeight,
                                                      .afterWidth = width,
                                                      .afterHeight = height,
                                                      .text = "Change page size"}});
    rebuildPageSnapshots();
    return true;
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
// Page management
// ---------------------------------------------------------------------------

void QtDocumentController::addPageAfter(std::size_t afterPageIndex) {
    if (!this->document) {
        return;
    }
    this->document->lock();
    // Clone dimensions from the reference page if it exists
    double width = 595.0;
    double height = 842.0;
    if (afterPageIndex < this->document->getPageCount()) {
        auto ref = this->document->getPage(afterPageIndex);
        if (ref) {
            width = ref->getWidth();
            height = ref->getHeight();
        }
    }
    auto newPage = std::make_shared<NotePage>(width, height);
    const auto insertPos = std::min(afterPageIndex + 1, this->document->getPageCount());
    this->document->insertPage(newPage, insertPos);
    this->document->unlock();
    rebuildPageSnapshots();
}

void QtDocumentController::duplicatePage(std::size_t pageIndex) {
    if (!this->document || pageIndex >= this->document->getPageCount()) {
        return;
    }
    this->document->lock();
    auto srcPage = this->document->getPage(pageIndex);
    if (!srcPage) {
        this->document->unlock();
        return;
    }
    // Create a new page with same dimensions and background
    auto newPage = std::make_shared<NotePage>(srcPage->getWidth(), srcPage->getHeight());
    newPage->setBackgroundType(srcPage->getBackgroundType());
    newPage->setBackgroundColor(srcPage->getBackgroundColor());

    // Clone elements from all layers
    for (auto* srcLayer: srcPage->getLayers()) {
        if (!srcLayer) {
            continue;
        }
        // NotePage creates with one layer; reuse for first, add for subsequent
        Layer* dstLayer = nullptr;
        if (newPage->getLayers().empty()) {
            // Should not happen, but handle gracefully
            this->document->unlock();
            return;
        }
        dstLayer = newPage->getLayers().back();

        for (const auto& elem: srcLayer->getElements()) {
            if (elem) {
                dstLayer->addElement(elem->clone());
            }
        }
    }

    this->document->insertPage(newPage, pageIndex + 1);
    this->document->unlock();
    rebuildPageSnapshots();
}

void QtDocumentController::deletePage(std::size_t pageIndex) {
    if (!this->document || pageIndex >= this->document->getPageCount()) {
        return;
    }
    // Don't delete the last page — keep at least one
    if (this->document->getPageCount() <= 1) {
        return;
    }
    this->document->lock();
    this->document->deletePage(pageIndex);
    this->document->unlock();
    rebuildPageSnapshots();
}

void QtDocumentController::addPageBefore(std::size_t beforePageIndex) {
    if (!this->document) {
        return;
    }
    this->document->lock();
    double width = 595.0;
    double height = 842.0;
    if (beforePageIndex < this->document->getPageCount()) {
        auto ref = this->document->getPage(beforePageIndex);
        if (ref) {
            width = ref->getWidth();
            height = ref->getHeight();
        }
    }
    auto newPage = std::make_shared<NotePage>(width, height);
    const auto insertPos = std::min(beforePageIndex, this->document->getPageCount());
    this->document->insertPage(newPage, insertPos);
    this->document->unlock();
    rebuildPageSnapshots();
}

void QtDocumentController::movePageTowards(std::size_t pageIndex, int direction) {
    if (!this->document) {
        return;
    }
    const auto count = this->document->getPageCount();
    if (pageIndex >= count) {
        return;
    }
    const auto target = static_cast<std::ptrdiff_t>(pageIndex) + direction;
    if (target < 0 || static_cast<std::size_t>(target) >= count) {
        return;
    }
    this->document->lock();
    // Swap pages in document
    auto pageA = this->document->getPage(pageIndex);
    auto pageB = this->document->getPage(static_cast<std::size_t>(target));
    if (pageA && pageB) {
        // Remove both and re-insert in swapped positions
        // Use a simpler approach: just swap in the document's page vector
        this->document->deletePage(pageIndex);
        this->document->insertPage(pageA, static_cast<std::size_t>(target));
    }
    this->document->unlock();
    rebuildPageSnapshots();
}

auto QtDocumentController::appendNewPdfPages() -> int {
    if (!this->document || this->document->getPdfPageCount() == 0U) {
        return -1;
    }

    std::size_t currentPdfPageCount = 0U;
    for (std::size_t index = 0; index < this->document->getPageCount(); ++index) {
        auto page = this->document->getPage(index);
        if (page && page->getBackgroundType().isPdfPage()) {
            currentPdfPageCount = std::max(currentPdfPageCount, page->getPdfPageNr() + 1U);
        }
    }

    const std::size_t pdfPageCount = this->document->getPdfPageCount();
    if (currentPdfPageCount >= pdfPageCount) {
        return 0;
    }

    int inserted = 0;
    this->document->lock();
    for (std::size_t pdfIndex = currentPdfPageCount; pdfIndex < pdfPageCount; ++pdfIndex) {
        const auto pdfPage = this->document->getPdfPage(pdfIndex);
        if (!pdfPage) {
            continue;
        }
        auto page = std::make_shared<NotePage>(pdfPage->getWidth(), pdfPage->getHeight());
        page->setBackgroundPdfPageNr(pdfIndex);
        this->document->addPage(page);
        ++inserted;
    }
    this->document->unlock();

    if (inserted > 0) {
        clearInteractiveGeometryState();
        rebuildPageSnapshots();
    }
    return inserted;
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
                } else if constexpr (std::is_same_v<T, QtDeleteHistoryEntry>) {
                    // Delete undo: re-insert all removed elements
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
                    std::sort(e.removedElements.begin(), e.removedElements.end());
                    e.elementPtrs.clear();
                    for (auto& ip: e.removedElements) {
                        e.elementPtrs.push_back(ip.e.get());
                        layer->insertElement(std::move(ip.e), ip.pos);
                    }
                    this->document->unlock();
                    rebuildPageSnapshots();
                    return true;
                } else if constexpr (std::is_same_v<T, QtLayerTransferHistoryEntry>) {
                    if (e.records.empty() || !this->document || e.pageIndex >= this->document->getPageCount()) {
                        return false;
                    }
                    this->document->lock();
                    auto page = this->document->getPage(e.pageIndex);
                    if (!page) {
                        this->document->unlock();
                        return false;
                    }
                    auto& layers = page->getLayers();
                    std::vector<std::size_t> order(e.records.size());
                    std::iota(order.begin(), order.end(), 0U);
                    std::sort(order.begin(), order.end(), [&](std::size_t lhs, std::size_t rhs) {
                        return e.records[lhs].fromPos < e.records[rhs].fromPos;
                    });
                    for (auto recordIndex: order) {
                        auto& record = e.records[recordIndex];
                        if (record.toLayerIndex >= layers.size() || record.fromLayerIndex >= layers.size() ||
                            !layers[record.toLayerIndex] || !layers[record.fromLayerIndex]) {
                            continue;
                        }
                        auto removed = layers[record.toLayerIndex]->removeElement(record.element);
                        if (!removed.e) {
                            continue;
                        }
                        layers[record.fromLayerIndex]->insertElement(std::move(removed.e), record.fromPos);
                    }
                    this->document->unlock();
                    rebuildPageSnapshots();
                    return true;
                } else if constexpr (std::is_same_v<T, QtPageSizeHistoryEntry>) {
                    if (!this->document || e.pageIndex >= this->document->getPageCount()) {
                        return false;
                    }
                    this->document->lock();
                    auto page = this->document->getPage(e.pageIndex);
                    if (!page) {
                        this->document->unlock();
                        return false;
                    }
                    Document::setPageSize(page, e.beforeWidth, e.beforeHeight);
                    this->document->unlock();
                    rebuildPageSnapshots();
                    return true;
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
                } else if constexpr (std::is_same_v<T, QtDeleteHistoryEntry>) {
                    // Delete redo: remove elements again using saved raw pointers
                    if (e.elementPtrs.empty() || !this->document ||
                        e.pageIndex >= this->document->getPageCount()) {
                        return false;
                    }
                    this->document->lock();
                    auto page = this->document->getPage(e.pageIndex);
                    if (!page) {
                        this->document->unlock();
                        return false;
                    }
                    e.removedElements.clear();
                    for (auto* layer: page->getLayers()) {
                        if (!layer) {
                            continue;
                        }
                        for (const auto* ptr: e.elementPtrs) {
                            auto removed = layer->removeElement(ptr);
                            if (removed.e) {
                                e.removedElements.push_back(std::move(removed));
                            }
                        }
                    }
                    e.elementPtrs.clear();
                    this->document->unlock();
                    if (!e.removedElements.empty()) {
                        rebuildPageSnapshots();
                        return true;
                    }
                    return false;
                } else if constexpr (std::is_same_v<T, QtLayerTransferHistoryEntry>) {
                    if (e.records.empty() || !this->document || e.pageIndex >= this->document->getPageCount()) {
                        return false;
                    }
                    this->document->lock();
                    auto page = this->document->getPage(e.pageIndex);
                    if (!page) {
                        this->document->unlock();
                        return false;
                    }
                    auto& layers = page->getLayers();
                    std::vector<std::size_t> order(e.records.size());
                    std::iota(order.begin(), order.end(), 0U);
                    std::sort(order.begin(), order.end(), [&](std::size_t lhs, std::size_t rhs) {
                        return e.records[lhs].toPos < e.records[rhs].toPos;
                    });
                    for (auto recordIndex: order) {
                        auto& record = e.records[recordIndex];
                        if (record.fromLayerIndex >= layers.size() || record.toLayerIndex >= layers.size() ||
                            !layers[record.fromLayerIndex] || !layers[record.toLayerIndex]) {
                            continue;
                        }
                        auto removed = layers[record.fromLayerIndex]->removeElement(record.element);
                        if (!removed.e) {
                            continue;
                        }
                        layers[record.toLayerIndex]->insertElement(std::move(removed.e), record.toPos);
                    }
                    this->document->unlock();
                    rebuildPageSnapshots();
                    return true;
                } else if constexpr (std::is_same_v<T, QtPageSizeHistoryEntry>) {
                    if (!this->document || e.pageIndex >= this->document->getPageCount()) {
                        return false;
                    }
                    this->document->lock();
                    auto page = this->document->getPage(e.pageIndex);
                    if (!page) {
                        this->document->unlock();
                        return false;
                    }
                    Document::setPageSize(page, e.afterWidth, e.afterHeight);
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
