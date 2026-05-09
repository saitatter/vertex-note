/*
 * VertexNote
 *
 * Experimental Qt document controller backed by the shared core model.
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
#include "view/render/Renderers.h"

namespace vn::geom {
class GeometryElement;
}

struct QtExperimentalPageInfo {
    double width = 0.0;
    double height = 0.0;
    vn::view::render::PageBackgroundRenderModel background;
    std::vector<vn::view::render::PageDrawableRenderModel> drawables;
};

struct QtExperimentalGeometryHit {
    std::size_t pageIndex = 0U;
    vn::view::render::GeometryHitResult hit;
};

struct QtExperimentalGeometryDragState {
    std::size_t pageIndex = 0U;
    vn::geom::ObjectId objectId = vn::geom::InvalidObjectId;
    vn::geom::VertexId vertexId = vn::geom::InvalidVertexId;
    vn::geom::Vec2 originalPosition;
    vn::geom::Vec2 currentPosition;
    vn::geom::GeometryObject beforeGeometry;
    std::optional<vn::snap::SnapKind> snapKind;
    vn::geom::Vec2 snapPoint;
    bool changed = false;
};

struct QtExperimentalSnapOptions {
    bool geometryEnabled = true;
    bool gridEnabled = false;
};

struct QtExperimentalGeometryHistoryEntry {
    std::size_t pageIndex = 0U;
    vn::geom::ObjectId objectId = vn::geom::InvalidObjectId;
    vn::geom::GeometryObject before;
    vn::geom::GeometryObject after;
    std::string text;
};

class QtExperimentalDocumentController {
public:
    QtExperimentalDocumentController();

public:
    void newBlankDocument();
    auto loadFrom(const std::filesystem::path& path, std::string* errorMessage = nullptr) -> bool;

    [[nodiscard]] auto hasDocument() const -> bool;
    [[nodiscard]] auto pageCount() const -> std::size_t;
    [[nodiscard]] auto snapshotPages() const -> const std::vector<QtExperimentalPageInfo>&;
    [[nodiscard]] auto sourcePath() const -> const std::optional<std::filesystem::path>&;
    [[nodiscard]] auto titleText() const -> std::string;
    [[nodiscard]] auto hitTestGeometry(std::size_t pageIndex, double pageX, double pageY, double zoom,
                                       double maxScreenDistance = 8.0) const -> std::optional<QtExperimentalGeometryHit>;
    void setHoveredGeometry(std::optional<QtExperimentalGeometryHit> hit);
    void setSelectedGeometry(std::optional<QtExperimentalGeometryHit> hit);
    void clearInteractiveGeometryState();
    [[nodiscard]] auto hoveredGeometry() const -> const std::optional<QtExperimentalGeometryHit>&;
    [[nodiscard]] auto selectedGeometry() const -> const std::optional<QtExperimentalGeometryHit>&;
    [[nodiscard]] auto beginGeometryVertexDrag(const QtExperimentalGeometryHit& hit) -> bool;
    [[nodiscard]] auto updateGeometryVertexDrag(double pageX, double pageY, double zoom,
                                                const QtExperimentalSnapOptions& options) -> bool;
    [[nodiscard]] auto endGeometryVertexDrag() -> bool;
    [[nodiscard]] auto activeGeometryDrag() const -> const std::optional<QtExperimentalGeometryDragState>&;
    [[nodiscard]] auto deleteSelectedGeometry() -> bool;
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
    void pushGeometryHistory(QtExperimentalGeometryHistoryEntry entry);
    [[nodiscard]] auto applyGeometryHistoryEntry(const QtExperimentalGeometryHistoryEntry& entry, bool useAfterState) -> bool;
    [[nodiscard]] auto findMutableGeometryElement(std::size_t pageIndex, vn::geom::ObjectId objectId)
            -> vn::geom::GeometryElement*;
    [[nodiscard]] static auto gridSnapProviderFor(PageTypeFormat format) -> std::shared_ptr<const vn::snap::ISnapProvider>;

private:
    DocumentHandler documentHandler;
    std::unique_ptr<Document> document;
    std::optional<std::filesystem::path> loadedPath;
    std::vector<QtExperimentalPageInfo> pageSnapshots;
    std::optional<QtExperimentalGeometryHit> hoveredGeometryHit;
    std::optional<QtExperimentalGeometryHit> selectedGeometryHit;
    std::optional<QtExperimentalGeometryDragState> geometryDragState;
    std::deque<QtExperimentalGeometryHistoryEntry> geometryUndoHistory;
    std::deque<QtExperimentalGeometryHistoryEntry> geometryRedoHistory;
};
