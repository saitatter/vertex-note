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
#include <vector>

#include "model/Document.h"
#include "model/DocumentHandler.h"
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
};
