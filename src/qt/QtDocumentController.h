/*
 * VertexNote
 *
 * Qt document controller backed by the shared core model.
 */

#pragma once

#include <filesystem>
#include <deque>
#include <memory>
#include <optional>
#include <string>
#include <variant>
#include <vector>

#include "model/ElementInsertionPosition.h"

#include "model/Document.h"
#include "model/DocumentHandler.h"
#include "model/Element.h"
#include "model/Point.h"
#include "model/Stroke.h"
#include "model/Text.h"
#include "util/Color.h"
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
    vn::geom::Vec2 snapPoint;
    bool changed = false;
};

struct QtSnapOptions {
    bool geometryEnabled = true;
    bool gridEnabled = false;
};

struct QtGeometryHistoryEntry {
    std::size_t pageIndex = 0U;
    vn::geom::ObjectId objectId = vn::geom::InvalidObjectId;
    vn::geom::GeometryObject before;
    vn::geom::GeometryObject after;
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

struct QtTextHistoryEntry {
    std::size_t pageIndex = 0U;
    const Element* element = nullptr;
    ElementPtr ownedElement;        // populated after undo (for redo)
    Element::Index insertionPos{};  // original position in layer
    std::string previousContent;    // for edit undo (empty if new)
    bool isNew = false;             // true if this was an insert, false if edit
    std::string text;
};

struct QtHistoryEntry {
    std::variant<QtGeometryHistoryEntry, QtStrokeHistoryEntry, QtEraseHistoryEntry, QtSegmentEraseHistoryEntry,
                 QtMoveHistoryEntry, QtTextHistoryEntry>
            data;
    [[nodiscard]] auto text() const -> std::string;
};

struct QtElementSelection {
    std::size_t pageIndex = 0U;
    std::vector<const Element*> elements;
};

struct QtMoveState {
    double startX = 0.0;
    double startY = 0.0;
    double currentDx = 0.0;
    double currentDy = 0.0;
    std::vector<const Element*> elements;
    std::size_t pageIndex = 0U;
};

struct QtActiveStroke {
    std::size_t pageIndex = 0U;
    std::unique_ptr<Stroke> stroke;
    bool hasPressure = false;
};

struct QtLayerInfo {
    std::size_t index = 0U;
    std::string name;
    bool visible = true;
    bool selected = false;
    std::size_t elementCount = 0U;
};

class QtDocumentController {
public:
    QtDocumentController();

public:
    void newBlankDocument();
    auto loadFrom(const std::filesystem::path& path, std::string* errorMessage = nullptr) -> bool;

    [[nodiscard]] auto hasDocument() const -> bool;
    [[nodiscard]] auto pageCount() const -> std::size_t;
    [[nodiscard]] auto snapshotPages() const -> const std::vector<vn::view::render::PageRenderSnapshot>&;
    [[nodiscard]] auto sourcePath() const -> const std::optional<std::filesystem::path>&;
    [[nodiscard]] auto titleText() const -> std::string;
    [[nodiscard]] auto hitTestGeometry(std::size_t pageIndex, double pageX, double pageY, double zoom,
                                       double maxScreenDistance = 8.0) const -> std::optional<QtGeometryHit>;
    void setHoveredGeometry(std::optional<QtGeometryHit> hit);
    void setSelectedGeometry(std::optional<QtGeometryHit> hit, bool additive = false);
    void clearInteractiveGeometryState();
    [[nodiscard]] auto hoveredGeometry() const -> const std::optional<QtGeometryHit>&;
    [[nodiscard]] auto selectedGeometry() const -> const std::optional<QtGeometryHit>&;
    [[nodiscard]] auto selectedVertexIds() const -> const std::vector<vn::geom::VertexId>&;
    [[nodiscard]] auto beginGeometryVertexDrag(const QtGeometryHit& hit) -> bool;
    [[nodiscard]] auto updateGeometryVertexDrag(double pageX, double pageY, double zoom,
                                                const QtSnapOptions& options) -> bool;
    [[nodiscard]] auto endGeometryVertexDrag() -> bool;
    [[nodiscard]] auto activeGeometryDrag() const -> const std::optional<QtGeometryDragState>&;
    [[nodiscard]] auto deleteSelectedGeometry() -> bool;
    [[nodiscard]] auto insertVertexOnSelectedEdge() -> bool;
    [[nodiscard]] auto canUndoGeometryEdit() const -> bool;
    [[nodiscard]] auto canRedoGeometryEdit() const -> bool;
    [[nodiscard]] auto undoGeometryEdit() -> bool;
    [[nodiscard]] auto redoGeometryEdit() -> bool;
    [[nodiscard]] auto undoGeometryEditText() const -> std::string;
    [[nodiscard]] auto redoGeometryEditText() const -> std::string;

    // Stroke input
    auto beginStroke(std::size_t pageIndex, double x, double y, double pressure, Color color, double width,
                     StrokeTool::Value toolType, bool pressureSensitive) -> bool;
    auto updateStroke(double x, double y, double pressure) -> bool;
    auto finalizeStroke() -> bool;
    auto cancelStroke() -> void;
    [[nodiscard]] auto activeStroke() const -> const QtActiveStroke*;

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
    void clearElementSelection();
    [[nodiscard]] auto elementSelection() const -> const std::optional<QtElementSelection>&;
    [[nodiscard]] auto isElementSelected(const Element* e) const -> bool;

    // Element move
    auto beginMoveSelection(double pageX, double pageY) -> bool;
    auto updateMoveSelection(double pageX, double pageY) -> bool;
    auto endMoveSelection() -> bool;
    auto cancelMoveSelection() -> void;
    [[nodiscard]] auto isMovingSelection() const -> bool;

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

    // Page background
    void setPageBackgroundColor(std::size_t pageIndex, Color color);
    void setPageBackgroundType(std::size_t pageIndex, PageTypeFormat format);

    // Text editing
    auto insertTextElement(std::size_t pageIndex, std::unique_ptr<Text> text) -> const Element*;
    auto hitTestTextElement(std::size_t pageIndex, double pageX, double pageY, double maxDistance) const -> Text*;

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
    void clearGeometryHistory();
    void pushGeometryHistory(QtGeometryHistoryEntry entry);
    [[nodiscard]] auto applyGeometryHistoryEntry(const QtGeometryHistoryEntry& entry, bool useAfterState) -> bool;
    [[nodiscard]] auto findMutableGeometryElement(std::size_t pageIndex, vn::geom::ObjectId objectId)
            -> vn::geom::GeometryElement*;
    [[nodiscard]] static auto gridSnapProviderFor(PageTypeFormat format) -> std::shared_ptr<const vn::snap::ISnapProvider>;
    void clearHistory();
    void pushHistory(QtHistoryEntry entry);
    [[nodiscard]] auto applyHistoryUndo(QtHistoryEntry& entry) -> bool;
    [[nodiscard]] auto applyHistoryRedo(QtHistoryEntry& entry) -> bool;

private:
    DocumentHandler documentHandler;
    std::unique_ptr<Document> document;
    std::optional<std::filesystem::path> loadedPath;
    std::vector<vn::view::render::PageRenderSnapshot> pageSnapshots;
    std::optional<QtGeometryHit> hoveredGeometryHit;
    std::optional<QtGeometryHit> selectedGeometryHit;
    std::vector<vn::geom::VertexId> selectedGeometryVertexIds;
    std::optional<QtGeometryDragState> geometryDragState;
    std::deque<QtGeometryHistoryEntry> geometryUndoHistory;
    std::deque<QtGeometryHistoryEntry> geometryRedoHistory;
    std::optional<QtActiveStroke> currentStroke;
    std::optional<QtEraseHistoryEntry> pendingErase;
    std::optional<QtSegmentEraseHistoryEntry> pendingSegmentErase;
    std::optional<QtElementSelection> currentSelection;
    std::optional<QtMoveState> moveState;
    std::deque<QtHistoryEntry> undoHistory;
    std::deque<QtHistoryEntry> redoHistory;
};
