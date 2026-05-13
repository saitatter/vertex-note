/*
 * VertexNote
 *
 * Qt document controller backed by the shared core model.
 */

#pragma once

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <deque>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

#include "model/ElementInsertionPosition.h"

#include "model/Document.h"
#include "model/DocumentHandler.h"
#include "model/Element.h"
#include "model/Image.h"
#include "model/Point.h"
#include "model/Stroke.h"
#include "model/Text.h"
#include "pdf/base/PdfPage.h"
#include "util/Color.h"
#include "vertexnote/constraints/GeometryConstraintCatalog.h"
#include "vertexnote/constraints/GeometryConstraintSolver.h"
#include "vertexnote/geometry/GeometryObject.h"
#include "vertexnote/geometry/GeometryTypes.h"
#include "vertexnote/snapping/ISnapProvider.h"
#include "vertexnote/snapping/SnapTypes.h"
#include "view/render/GeometryHitTest.h"
#include "view/render/PageRenderSnapshotFactory.h"

namespace vn::geom {
class GeometryElement;
}

struct QtGeometryHit {
    std::size_t pageIndex = 0U;
    vn::view::render::GeometryHitResult hit;
};

struct QtGeometryDragState {
    std::size_t pageIndex = 0U;
    vn::geom::ObjectId objectId = vn::geom::InvalidObjectId;
    vn::geom::VertexId vertexId = vn::geom::InvalidVertexId;
    std::vector<vn::geom::VertexId> vertexIds;
    vn::geom::Vec2 originalPosition;
    std::vector<vn::geom::Vec2> originalPositions;
    vn::geom::Vec2 currentPosition;
    std::vector<vn::geom::Vec2> currentPositions;
    vn::geom::GeometryObject beforeGeometry;
    std::optional<vn::snap::SnapKind> snapKind;
    std::optional<vn::snap::SnapCandidate> snapCandidate;
    vn::geom::Vec2 snapPoint;
    bool changed = false;
};

struct QtGeometryTransformState {
    std::size_t pageIndex = 0U;
    vn::geom::ObjectId objectId = vn::geom::InvalidObjectId;
    std::vector<vn::geom::VertexId> vertexIds;
    vn::geom::GeometryObject beforeGeometry;
    vn::geom::GeometryObject currentGeometry;
    vn::geom::Vec2 center;
    double currentDx = 0.0;
    double currentDy = 0.0;
    double currentDegrees = 0.0;
    bool transformedWholeObject = false;
    bool changed = false;
};

struct QtSnapOptions {
    bool geometryEnabled = true;
    bool gridEnabled = false;
    double gridSize = 14.17;
    double gridTolerance = 0.50;
    double screenTolerance = 18.0;
};

struct QtSnapPointResult {
    vn::geom::Vec2 pagePoint;
    std::optional<vn::snap::SnapKind> snapKind;
    std::optional<vn::snap::SnapCandidate> snapCandidate;
    bool snapped = false;
};

struct QtGeometryHistoryObjectState {
    std::size_t pageIndex = 0U;
    vn::geom::ObjectId objectId = vn::geom::InvalidObjectId;
    vn::geom::GeometryObject before;
    vn::geom::GeometryObject after;
};

struct QtGeometryHistoryRemovedElement {
    std::size_t pageIndex = 0U;
    std::size_t layerIndex = 0U;
    InsertionPosition removed;
    const Element* element = nullptr;
};

struct QtGeometryHistoryEntry {
    std::size_t pageIndex = 0U;
    vn::geom::ObjectId objectId = vn::geom::InvalidObjectId;
    vn::geom::GeometryObject before;
    vn::geom::GeometryObject after;
    std::vector<QtGeometryHistoryObjectState> linkedObjects;
    std::vector<QtGeometryHistoryRemovedElement> removedElements;
    bool removesPrimaryObject = false;
    std::string text;
};

struct QtStrokeHistoryEntry {
    std::size_t pageIndex = 0U;
    const Element* element = nullptr;
    ElementPtr ownedElement;        // populated after undo (for redo)
    Element::Index insertionPos{};  // original position in layer
    std::string text;
};

struct QtEraseHistoryEntry {
    std::size_t pageIndex = 0U;
    InsertionOrder removedElements;           // owned elements with original positions
    std::vector<const Element*> elementPtrs;  // raw pointers for re-removal on redo
    std::string text;
};

struct QtSegmentEraseHistoryEntry {
    std::size_t pageIndex = 0U;
    InsertionOrder removedOriginals;                  // original strokes removed
    std::vector<const Element*> removedOriginalPtrs;  // raw ptrs for redo removal of re-inserted originals
    InsertionOrder ownedFragments;                    // fragments owned after undo
    std::vector<const Element*> fragmentPtrs;         // raw ptrs of fragments currently in layer
    std::string text;
};

struct QtMoveHistoryEntry {
    std::size_t pageIndex = 0U;
    std::vector<const Element*> elements;
    double dx = 0.0;
    double dy = 0.0;
    std::string text;
};

struct QtScaleHistoryEntry {
    std::size_t pageIndex = 0U;
    std::vector<const Element*> elements;
    double originX = 0.0;
    double originY = 0.0;
    double fx = 1.0;
    double fy = 1.0;
    bool restoreLineWidth = false;
    std::string text;
};

struct QtTextHistoryEntry {
    std::size_t pageIndex = 0U;
    const Element* element = nullptr;
    ElementPtr ownedElement;        // populated after undo (for redo)
    Element::Index insertionPos{};  // original position in layer
    std::string previousContent;    // for edit undo (empty if new)
    bool isNew = false;             // true if this was an insert, false if edit
    std::string text;
};

struct QtDeleteHistoryEntry {
    std::size_t pageIndex = 0U;
    InsertionOrder removedElements;
    std::vector<const Element*> elementPtrs;
    std::string text;
};

struct QtInsertElementsHistoryEntry {
    std::size_t pageIndex = 0U;
    std::vector<const Element*> elements;
    InsertionOrder ownedElements;
    std::string text;
};

struct QtLayerTransferRecord {
    const Element* element = nullptr;
    std::size_t fromLayerIndex = 0U;
    std::size_t toLayerIndex = 0U;
    Element::Index fromPos{};
    Element::Index toPos{};
};

struct QtLayerTransferHistoryEntry {
    std::size_t pageIndex = 0U;
    std::vector<QtLayerTransferRecord> records;
    std::string text;
};

struct QtPageSizeHistoryEntry {
    std::size_t pageIndex = 0U;
    double beforeWidth = 0.0;
    double beforeHeight = 0.0;
    double afterWidth = 0.0;
    double afterHeight = 0.0;
    std::string text;
};

struct QtHistoryEntry {
    std::variant<QtGeometryHistoryEntry, QtStrokeHistoryEntry, QtEraseHistoryEntry, QtSegmentEraseHistoryEntry,
                 QtMoveHistoryEntry, QtScaleHistoryEntry, QtTextHistoryEntry, QtDeleteHistoryEntry,
                 QtInsertElementsHistoryEntry, QtLayerTransferHistoryEntry, QtPageSizeHistoryEntry>
            data;
    [[nodiscard]] auto text() const -> std::string;
};

struct QtElementSelection {
    std::size_t pageIndex = 0U;
    std::vector<const Element*> elements;
};

struct QtSelectionBounds {
    double x = 0.0;
    double y = 0.0;
    double width = 0.0;
    double height = 0.0;
};

struct QtMoveState {
    double startX = 0.0;
    double startY = 0.0;
    double currentDx = 0.0;
    double currentDy = 0.0;
    std::vector<const Element*> elements;
    std::size_t pageIndex = 0U;
};

struct QtScaleState {
    double originX = 0.0;
    double originY = 0.0;
    double startX = 0.0;
    double startY = 0.0;
    double currentFx = 1.0;
    double currentFy = 1.0;
    bool scaleX = true;
    bool scaleY = true;
    bool preserveAspectRatio = false;
    bool supportMirroring = true;
    bool restoreLineWidth = false;
    std::vector<const Element*> elements;
    std::size_t pageIndex = 0U;
};

struct QtVerticalSpaceState {
    double startY = 0.0;
    double currentDy = 0.0;
    std::vector<const Element*> elements;
    std::size_t pageIndex = 0U;
};

struct QtActiveStroke {
    std::size_t pageIndex = 0U;
    std::unique_ptr<Stroke> stroke;
    bool hasPressure = false;
};

struct QtPdfTextSelectionState {
    std::size_t pageIndex = 0U;
    PdfPageSelectionStyle style = PdfPageSelectionStyle::Linear;
    PdfRectangle bounds;
    std::vector<PdfRectangle> previewRects;
    std::string selectedText;
    bool finalized = false;
};

enum class QtPdfTextMarkerKind { Highlight, Underline, Strikethrough };

struct QtLayerInfo {
    std::size_t index = 0U;
    std::string name;
    bool visible = true;
    bool selected = false;
    std::size_t elementCount = 0U;
};

struct QtPluginElementRef {
    const Element* element = nullptr;
    std::size_t pageIndex = 0U;
    std::size_t layerIndex = 0U;
};

class QtDocumentController {
public:
    QtDocumentController();

public:
    void newBlankDocument();
    auto loadFrom(const std::filesystem::path& path, std::string* errorMessage = nullptr) -> bool;
    auto loadPdfAsDocument(const std::filesystem::path& path, bool attachToDocument,
                           std::string* errorMessage = nullptr) -> bool;

    [[nodiscard]] auto hasDocument() const -> bool;
    [[nodiscard]] auto pageCount() const -> std::size_t;
    [[nodiscard]] auto hasPdfBackgroundDocument() const -> bool;
    [[nodiscard]] auto snapshotPages() const -> const std::vector<vn::view::render::PageRenderSnapshot>&;
    void preparePdfRasterCache(const std::vector<std::size_t>& visiblePageIndices);
    void setPdfCacheOptions(int pageCacheSize, int preloadPagesBefore, int preloadPagesAfter, bool eagerCleanup,
                            double pageRerenderThreshold = 5.0);
    [[nodiscard]] auto sourcePath() const -> const std::optional<std::filesystem::path>&;
    [[nodiscard]] auto titleText() const -> std::string;
    [[nodiscard]] auto hitTestGeometry(std::size_t pageIndex, double pageX, double pageY, double zoom,
                                       double maxScreenDistance = 8.0) const -> std::optional<QtGeometryHit>;
    [[nodiscard]] auto hitTestGeometry(std::size_t pageIndex, double pageX, double pageY, double zoom,
                                       double maxScreenDistance, bool includeVertices, bool includeEdges) const
            -> std::optional<QtGeometryHit>;
    void setHoveredGeometry(std::optional<QtGeometryHit> hit);
    void setSelectedGeometry(std::optional<QtGeometryHit> hit, bool additive = false);
    void setSelectedGeometryObject(std::optional<QtGeometryHit> hit);
    void clearInteractiveGeometryState();
    [[nodiscard]] auto hoveredGeometry() const -> const std::optional<QtGeometryHit>&;
    [[nodiscard]] auto selectedGeometry() const -> const std::optional<QtGeometryHit>&;
    [[nodiscard]] auto selectedVertexIds() const -> const std::vector<vn::geom::VertexId>&;
    [[nodiscard]] auto selectedEdgeIds() const -> const std::vector<vn::geom::EdgeId>&;
    [[nodiscard]] auto snapPagePoint(std::size_t pageIndex, double pageX, double pageY, double zoom,
                                     const QtSnapOptions& options,
                                     std::optional<vn::geom::ObjectId> ignoredObjectId = std::nullopt) const
            -> QtSnapPointResult;
    [[nodiscard]] auto beginGeometryVertexDrag(const QtGeometryHit& hit) -> bool;
    [[nodiscard]] auto updateGeometryVertexDrag(double pageX, double pageY, double zoom,
                                                const QtSnapOptions& options) -> bool;
    [[nodiscard]] auto endGeometryVertexDrag() -> bool;
    [[nodiscard]] auto activeGeometryDrag() const -> const std::optional<QtGeometryDragState>&;
    [[nodiscard]] auto deleteSelectedGeometry() -> bool;
    [[nodiscard]] auto insertVertexOnSelectedEdge() -> bool;
    [[nodiscard]] auto translateSelectedVertices(double dx, double dy) -> bool;
    [[nodiscard]] auto rotateSelectedGeometry(double degrees) -> bool;
    [[nodiscard]] auto beginSelectedGeometryTransform() -> bool;
    [[nodiscard]] auto updateSelectedGeometryTransform(double dx, double dy, double degrees) -> bool;
    [[nodiscard]] auto endSelectedGeometryTransform() -> bool;
    void cancelSelectedGeometryTransform();
    [[nodiscard]] auto activeGeometryTransform() const -> const std::optional<QtGeometryTransformState>&;

    // Geometry constraints
    [[nodiscard]] auto applyConstraint(vn::geom::ConstraintKind kind) -> bool;
    [[nodiscard]] auto deleteSelectedConstraints() -> bool;
    [[nodiscard]] auto selectedFixedLengthConstraint() -> std::optional<vn::geom::Constraint>;
    [[nodiscard]] auto updateFixedLengthConstraint(double value) -> bool;

    // Shape creation
    auto createLine(std::size_t pageIndex, double x1, double y1, double x2, double y2, Color color, double width,
                    const std::string& lineStyle = "plain") -> const Element*;
    auto createEdge(std::size_t pageIndex, double x1, double y1, double x2, double y2, Color color, double width)
            -> const Element*;
    auto createRectangle(std::size_t pageIndex, double x1, double y1, double x2, double y2, Color color, double width)
            -> const Element*;
    auto createCircle(std::size_t pageIndex, double cx, double cy, double rx, double ry, Color color, double width)
            -> const Element*;
    auto createEllipse(std::size_t pageIndex, double x1, double y1, double x2, double y2, Color color, double width,
                       const std::string& lineStyle, int fill) -> const Element*;
    auto createArc(std::size_t pageIndex, double cx, double cy, double sx, double sy, double ex, double ey, Color color,
                   double width) -> const Element*;
    auto createArrow(std::size_t pageIndex, double x1, double y1, double x2, double y2, Color color, double width,
                     const std::string& lineStyle, bool doubleEnded) -> const Element*;
    auto createCoordinateSystem(std::size_t pageIndex, double x1, double y1, double x2, double y2, Color color,
                                double width, const std::string& lineStyle) -> const Element*;
    auto createSpline(std::size_t pageIndex, const std::vector<std::pair<double, double>>& points, Color color,
                      double width, const std::string& lineStyle) -> const Element*;
    auto createSetsquareStroke(std::size_t pageIndex, const std::vector<std::pair<double, double>>& points,
                               Color color, double width, const std::string& lineStyle) -> const Element*;
    auto createCompassStroke(std::size_t pageIndex, const std::vector<std::pair<double, double>>& points, Color color,
                             double width, const std::string& lineStyle) -> const Element*;
    auto createPolyline(std::size_t pageIndex, const std::vector<std::pair<double, double>>& points, Color color,
                        double width) -> const Element*;
    auto createConstructionLine(std::size_t pageIndex, double x1, double y1, double x2, double y2, Color color,
                                double width) -> const Element*;
    auto createConstructionCircle(std::size_t pageIndex, double cx, double cy, double rx, double ry, Color color,
                                  double width) -> const Element*;

    [[nodiscard]] auto canUndoGeometryEdit() const -> bool;
    [[nodiscard]] auto canRedoGeometryEdit() const -> bool;
    [[nodiscard]] auto undoGeometryEdit() -> bool;
    [[nodiscard]] auto redoGeometryEdit() -> bool;
    [[nodiscard]] auto undoGeometryEditText() const -> std::string;
    [[nodiscard]] auto redoGeometryEditText() const -> std::string;

    // Stroke input
    auto beginStroke(std::size_t pageIndex, double x, double y, double pressure, Color color, double width,
                     StrokeTool::Value toolType, bool pressureSensitive, const std::string& lineStyle = "plain",
                     int fill = -1) -> bool;
    auto updateStroke(double x, double y, double pressure) -> bool;
    auto finalizeStroke(bool recognizeShape = false, double recognizerMinSize = 40.0,
                        bool snapRecognizedToGrid = false) -> bool;
    auto cancelStroke() -> void;
    [[nodiscard]] auto activeStroke() const -> const QtActiveStroke*;
    [[nodiscard]] auto beginPdfTextSelection(std::size_t pageIndex, double x, double y, PdfPageSelectionStyle style)
            -> bool;
    [[nodiscard]] auto updatePdfTextSelection(double x, double y) -> bool;
    [[nodiscard]] auto finalizePdfTextSelection() -> std::string;
    void cancelPdfTextSelection();
    [[nodiscard]] auto pdfTextSelection() const -> const std::optional<QtPdfTextSelectionState>&;
    [[nodiscard]] auto createPdfTextMarkerStrokes(std::size_t pageIndex, const std::vector<PdfRectangle>& rects,
                                                  QtPdfTextMarkerKind kind, int opacity, Color color) -> int;
    [[nodiscard]] auto createPdfTextMarkerStrokesForSelection(QtPdfTextMarkerKind kind, int opacity, Color color)
            -> int;

    // Eraser
    auto beginErase(std::size_t pageIndex) -> void;
    auto eraseAt(std::size_t pageIndex, double x, double y, double halfSize) -> int;
    auto eraseSegmentAt(std::size_t pageIndex, double x, double y, double halfSize) -> int;
    auto finalizeErase() -> bool;
    auto cancelErase() -> void;
    [[nodiscard]] auto isErasing() const -> bool;

    // Element selection
    [[nodiscard]] auto hitTestElement(std::size_t pageIndex, double pageX, double pageY, double maxDistance) const
            -> const Element*;
    void selectElementAt(std::size_t pageIndex, double pageX, double pageY, double maxDistance, bool additive = false);
    void selectElementsInRect(std::size_t pageIndex, double x, double y, double w, double h);
    [[nodiscard]] auto selectGeometryVerticesInRect(std::size_t pageIndex, double x, double y, double w, double h,
                                                    bool additive = false) -> bool;
    [[nodiscard]] auto selectGeometryEdgesInRect(std::size_t pageIndex, double x, double y, double w, double h,
                                                 bool additive = false) -> bool;
    [[nodiscard]] auto selectGeometryObjectInRect(std::size_t pageIndex, double x, double y, double w, double h)
            -> bool;
    void clearElementSelection();
    [[nodiscard]] auto elementSelection() const -> const std::optional<QtElementSelection>&;
    [[nodiscard]] auto selectionBounds() const -> std::optional<QtSelectionBounds>;
    [[nodiscard]] auto isElementSelected(const Element* e) const -> bool;
    [[nodiscard]] auto elementsForPluginScope(std::string_view scope, ElementType type,
                                              std::size_t currentPageIndex) const
            -> std::vector<QtPluginElementRef>;
    [[nodiscard]] auto selectElementsByPluginRefs(std::size_t pageIndex,
                                                  const std::vector<const Element*>& refs) -> bool;
    [[nodiscard]] auto colorSelectedElements(Color color) -> bool;

    // Element operations
    [[nodiscard]] auto deleteSelectedElements() -> bool;
    void selectAllElements(std::size_t pageIndex);
    [[nodiscard]] auto copySelectedElements() -> std::vector<ElementPtr>;
    [[nodiscard]] auto cutSelectedElements() -> std::vector<ElementPtr>;
    auto pasteElements(std::size_t pageIndex, std::vector<ElementPtr> elements, double offsetX = 10.0,
                       double offsetY = 10.0) -> bool;

    // Z-order operations
    auto bringSelectionToFront() -> bool;
    auto sendSelectionToBack() -> bool;
    auto bringSelectionForward() -> bool;
    auto sendSelectionBackward() -> bool;

    // Element move
    auto beginMoveSelection(double pageX, double pageY) -> bool;
    auto updateMoveSelection(double pageX, double pageY) -> bool;
    auto endMoveSelection() -> bool;
    auto cancelMoveSelection() -> void;
    [[nodiscard]] auto isMovingSelection() const -> bool;
    auto beginScaleSelection(double originX, double originY, double startX, double startY, bool scaleX, bool scaleY,
                             bool restoreLineWidth) -> bool;
    auto updateScaleSelection(double pageX, double pageY) -> bool;
    auto endScaleSelection() -> bool;
    auto cancelScaleSelection() -> void;
    [[nodiscard]] auto isScalingSelection() const -> bool;
    auto beginVerticalSpace(std::size_t pageIndex, double pageY, bool moveAbove) -> bool;
    auto updateVerticalSpace(double pageY) -> bool;
    auto endVerticalSpace() -> bool;
    auto cancelVerticalSpace() -> void;
    [[nodiscard]] auto isVerticalSpacing() const -> bool;

    // Layer management
    [[nodiscard]] auto layerCount(std::size_t pageIndex) const -> std::size_t;
    [[nodiscard]] auto layerInfos(std::size_t pageIndex) const -> std::vector<QtLayerInfo>;
    [[nodiscard]] auto selectedLayerIndex(std::size_t pageIndex) const -> std::size_t;
    void selectLayer(std::size_t pageIndex, std::size_t layerIndex);
    void setLayerVisible(std::size_t pageIndex, std::size_t layerIndex, bool visible);
    void addLayer(std::size_t pageIndex);
    void removeLayer(std::size_t pageIndex, std::size_t layerIndex);
    void renameLayer(std::size_t pageIndex, std::size_t layerIndex, const std::string& name);
    void moveLayerUp(std::size_t pageIndex, std::size_t layerIndex);
    void moveLayerDown(std::size_t pageIndex, std::size_t layerIndex);
    void copyLayer(std::size_t pageIndex, std::size_t layerIndex);
    void mergeLayerDown(std::size_t pageIndex, std::size_t layerIndex);
    void showAllLayers(std::size_t pageIndex);
    void hideAllLayers(std::size_t pageIndex);
    [[nodiscard]] auto canMoveSelectionToAdjacentLayer(int direction) const -> bool;
    [[nodiscard]] auto moveSelectionToAdjacentLayer(int direction) -> bool;

    // Annotated page query
    [[nodiscard]] auto isPageAnnotated(std::size_t pageIndex) const -> bool;

    // Page background
    void setPageBackgroundColor(std::size_t pageIndex, Color color);
    void setPageBackgroundType(std::size_t pageIndex, PageTypeFormat format);
    void setPageBackgroundName(std::size_t pageIndex, const std::string& name);
    [[nodiscard]] auto changePagePdfBackground(std::size_t pageIndex, ptrdiff_t pageNumber, bool relative,
                                               std::string* errorMessage = nullptr) -> bool;
    [[nodiscard]] auto canResizePage(std::size_t pageIndex) const -> bool;
    [[nodiscard]] auto resizePage(std::size_t pageIndex, double width, double height) -> bool;

    // Text editing
    auto insertTextElement(std::size_t pageIndex, std::unique_ptr<Text> text) -> const Element*;
    auto hitTestTextElement(std::size_t pageIndex, double pageX, double pageY, double maxDistance) const -> Text*;

    // Page management
    void addPageAfter(std::size_t afterPageIndex);
    void addPageBefore(std::size_t beforePageIndex);
    void duplicatePage(std::size_t pageIndex);
    void deletePage(std::size_t pageIndex);
    void movePageTowards(std::size_t pageIndex, int direction);
    auto appendNewPdfPages() -> int;

    // Document save
    auto saveDocument(const std::filesystem::path& path, std::string* errorMessage = nullptr) -> bool;

    // Image insertion
    auto insertImage(std::size_t pageIndex, double x, double y, const std::string& imageData, double width,
                     double height) -> const Element*;
    auto insertElement(std::size_t pageIndex, ElementPtr element, std::string historyText) -> const Element*;

    // Text search
    struct TextSearchResult {
        std::size_t pageIndex = 0U;
        const Text* textElement = nullptr;
        std::string matchContext;
    };
    auto findTextInDocument(const std::string& query) const -> std::vector<TextSearchResult>;

    // Document access (for exporter/printer)
    [[nodiscard]] auto documentPtr() const -> const Document*;

    // Unified undo/redo
    [[nodiscard]] auto canUndo() const -> bool;
    [[nodiscard]] auto canRedo() const -> bool;
    [[nodiscard]] auto undo() -> bool;
    [[nodiscard]] auto redo() -> bool;
    [[nodiscard]] auto undoText() const -> std::string;
    [[nodiscard]] auto redoText() const -> std::string;

private:
    static auto isPdfPath(const std::filesystem::path& path) -> bool;
    static auto normalizeExtension(const std::filesystem::path& path) -> std::string;
    void rebuildPageSnapshots();
    [[nodiscard]] auto cachedPdfRaster(std::size_t pdfPageNumber, double pageWidth, double pageHeight)
            -> vn::util::RasterImageData;
    void prunePdfRasterCache();
    void clearPdfRasterCache();
    void clearGeometryHistory();
    void pushGeometryHistory(QtGeometryHistoryEntry entry);
    [[nodiscard]] auto applyGeometryHistoryEntry(QtGeometryHistoryEntry& entry, bool useAfterState) -> bool;
    [[nodiscard]] auto selectedGeometryTransformVertexIds(const vn::geom::GeometryObject& object) const
            -> std::vector<vn::geom::VertexId>;
    [[nodiscard]] auto findMutableGeometryElement(std::size_t pageIndex, vn::geom::ObjectId objectId)
            -> vn::geom::GeometryElement*;
    [[nodiscard]] auto removeGeometryElement(std::size_t pageIndex, vn::geom::ObjectId objectId)
            -> std::optional<QtGeometryHistoryRemovedElement>;
    [[nodiscard]] static auto gridSnapProviderFor(PageTypeFormat format, double gridSize, double gridTolerance)
            -> std::shared_ptr<const vn::snap::ISnapProvider>;
    void clearHistory();
    void pushHistory(QtHistoryEntry entry);
    [[nodiscard]] auto applyHistoryUndo(QtHistoryEntry& entry) -> bool;
    [[nodiscard]] auto applyHistoryRedo(QtHistoryEntry& entry) -> bool;

private:
    DocumentHandler documentHandler;
    std::unique_ptr<Document> document;
    std::optional<std::filesystem::path> loadedPath;
    std::vector<vn::view::render::PageRenderSnapshot> pageSnapshots;
    struct QtPdfRasterCacheEntry {
        std::size_t pdfPageNumber = 0U;
        double pageWidth = 0.0;
        double pageHeight = 0.0;
        std::uint64_t lastUsed = 0U;
        vn::util::RasterImageData raster;
    };
    std::vector<QtPdfRasterCacheEntry> pdfRasterCache;
    std::uint64_t pdfRasterUseCounter = 0U;
    int pdfPageCacheSize = 10;
    int pdfPreloadPagesBefore = 1;
    int pdfPreloadPagesAfter = 1;
    bool pdfEagerPageCleanup = false;
    double pdfPageRerenderThreshold = 5.0;
    std::optional<QtGeometryHit> hoveredGeometryHit;
    std::optional<QtGeometryHit> selectedGeometryHit;
    std::vector<vn::geom::VertexId> selectedGeometryVertexIds;
    std::vector<vn::geom::EdgeId> selectedGeometryEdgeIds;
    std::optional<QtGeometryDragState> geometryDragState;
    std::optional<QtGeometryTransformState> geometryTransformState;
    std::deque<QtGeometryHistoryEntry> geometryUndoHistory;
    std::deque<QtGeometryHistoryEntry> geometryRedoHistory;
    std::optional<QtActiveStroke> currentStroke;
    std::optional<QtPdfTextSelectionState> activePdfTextSelection;
    std::optional<QtEraseHistoryEntry> pendingErase;
    std::optional<QtSegmentEraseHistoryEntry> pendingSegmentErase;
    std::optional<QtElementSelection> currentSelection;
    std::optional<QtMoveState> moveState;
    std::optional<QtScaleState> scaleState;
    std::optional<QtVerticalSpaceState> verticalSpaceState;
    std::deque<QtHistoryEntry> undoHistory;
    std::deque<QtHistoryEntry> redoHistory;
};
