/*
 * VertexNote
 *
 * Qt canvas bootstrap.
 */

#pragma once

#include <memory>
#include <optional>
#include <vector>

#include <QWidget>

#include "QtDocumentController.h"
#include "QtDocumentSession.h"
#include "QtInputAdapter.h"
#include "QtPageContentRenderer.h"
#include "QtTextEditor.h"
#include "QtToolState.h"
#include "ui/common/ICanvasHost.h"
#include "ui/input/UiInputEvents.h"
#include "view/render/Renderers.h"

class QtCanvas: public QWidget, public vn::ui::common::ICanvasHost, public vn::ui::input::IInputEventSink {
    Q_OBJECT

public:
    explicit QtCanvas(QWidget* parent = nullptr);

public:
    void invalidateCanvas() override;
    void invalidateRect(double x, double y, double width, double height) override;
    void setCanvasCursor(vn::ui::common::CanvasCursor cursor) override;
    [[nodiscard]] auto viewport() const -> vn::ui::common::CanvasViewport override;
    void handlePointerEvent(const vn::ui::input::PointerEvent& event) override;
    void handleKeyboardEvent(const vn::ui::input::KeyboardEvent& event) override;
    void handleTouchEvent(const vn::ui::input::TouchEvent& event) override;
    void setDocumentController(QtDocumentController* documentController);
    void newBlankDocument();
    void setViewportState(double zoom, double scrollX, double scrollY);
    [[nodiscard]] auto sessionViewportState() const -> QtViewportState;
    void zoomIn();
    void zoomOut();
    void resetViewport();
    void fitPage(bool edited = true);
    void panBy(double dx, double dy);
    void setGeometrySnapEnabled(bool enabled);
    void setGridSnapEnabled(bool enabled);
    [[nodiscard]] auto isGeometrySnapEnabled() const -> bool;
    [[nodiscard]] auto isGridSnapEnabled() const -> bool;
    [[nodiscard]] auto deleteSelectedGeometry() -> bool;
    [[nodiscard]] auto insertVertexOnSelectedEdge() -> bool;
    [[nodiscard]] auto canUndoGeometryEdit() const -> bool;
    [[nodiscard]] auto canRedoGeometryEdit() const -> bool;
    [[nodiscard]] auto undoGeometryEdit() -> bool;
    [[nodiscard]] auto redoGeometryEdit() -> bool;

    // Tool state
    void setActiveTool(QtToolType tool);
    [[nodiscard]] auto activeTool() const -> QtToolType;
    [[nodiscard]] auto toolState() -> QtToolState&;
    [[nodiscard]] auto toolState() const -> const QtToolState&;

    // Unified undo/redo
    [[nodiscard]] auto canUndo() const -> bool;
    [[nodiscard]] auto canRedo() const -> bool;
    [[nodiscard]] auto performUndo() -> bool;
    [[nodiscard]] auto performRedo() -> bool;

    // Content renderer (shared for sidebar thumbnails / export)
    [[nodiscard]] auto contentRenderer() const -> vn::view::render::PageContentRenderer*;

protected:
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseDoubleClickEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void wheelEvent(QWheelEvent* event) override;
    void tabletEvent(QTabletEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;
    void keyReleaseEvent(QKeyEvent* event) override;
    bool event(QEvent* event) override;

Q_SIGNALS:
    void viewportStateChanged();
    void statusHintChanged(const QString& text);
    void documentEdited();

private:
    void updateDebugOverlay(QString summary);
    void emitViewportUpdate(bool edited = true);
    void zoomAroundScreenPoint(double factor, const QPointF& screenPoint);
    [[nodiscard]] auto pageRects() const -> std::vector<QRectF>;
    [[nodiscard]] auto documentSceneBounds() const -> QRectF;
    void drawPageContents(QPainter& painter, const QRectF& rect, const vn::view::render::PageRenderSnapshot& pageInfo,
                          std::size_t pageIndex) const;
    void drawGeometryInteractionOverlay(QPainter& painter, const QRectF& rect,
                                        const vn::view::render::PageRenderSnapshot& pageInfo,
                                        std::size_t pageIndex) const;
    void drawOverlayHud(QPainter& painter) const;
    [[nodiscard]] auto screenToScene(const QPointF& screenPoint) const -> QPointF;
    [[nodiscard]] auto pageIndexAtScenePoint(const QPointF& scenePoint) const -> std::optional<std::size_t>;
    void updateGeometryHover(const QPointF& screenPoint);
    void clearGeometryHover();
    void selectHoveredGeometry(bool additive = false);
    void beginPan(const QPointF& position);
    void endPan();
    void beginStrokeAtScreen(const QPointF& screenPoint, double pressure);
    void updateStrokeAtScreen(const QPointF& screenPoint, double pressure);
    void finalizeActiveStroke();
    void cancelActiveStroke();
    void drawActiveStroke(QPainter& painter) const;
    void beginEraseAtScreen(const QPointF& screenPoint);
    void eraseAtScreen(const QPointF& screenPoint);
    void finalizeErase();
    void cancelErase();

    // Text editing helpers
    void beginTextEditAtScreen(const QPointF& screenPoint);
    void commitTextEdit();
    void cancelTextEdit();

    // Selection helpers
    void selectElementAtScreen(const QPointF& screenPoint, bool additive);
    void beginRubberBand(const QPointF& screenPoint);
    void updateRubberBand(const QPointF& screenPoint);
    void finalizeRubberBand();
    void cancelRubberBand();
    void beginMoveSelectionAtScreen(const QPointF& screenPoint);
    void updateMoveSelectionAtScreen(const QPointF& screenPoint);
    void finalizeMoveSelection();
    void cancelMoveSelection();
    void drawSelectionOverlay(QPainter& painter) const;
    void drawRubberBand(QPainter& painter) const;

    // Shape drawing helpers
    void beginShapeAtScreen(const QPointF& screenPoint);
    void updateShapeAtScreen(const QPointF& screenPoint);
    void addShapeClickAtScreen(const QPointF& screenPoint);
    void finalizeShape();
    void cancelShape();
    void drawShapePreview(QPainter& painter) const;
    [[nodiscard]] auto isMultiClickShapeTool() const -> bool;

private:
    std::unique_ptr<QtInputAdapter> inputAdapter;
    std::unique_ptr<vn::view::render::BackgroundRenderer> backgroundRenderer;
    std::unique_ptr<vn::view::render::GeometryRenderer> geometryRenderer;
    std::unique_ptr<vn::view::render::StrokeRenderer> strokeRenderer;
    std::unique_ptr<vn::view::render::TextRenderer> textRenderer;
    std::unique_ptr<vn::view::render::ImageRenderer> imageRenderer;
    std::unique_ptr<vn::view::render::PageContentRenderer> pageContentRenderer;
    QString lastEventSummary;
    QtDocumentController* documentController = nullptr;
    double zoomFactor = 1.0;
    double scrollX = 0.0;
    double scrollY = 0.0;
    bool geometrySnapEnabled = true;
    bool gridSnapEnabled = false;
    bool spaceHeld = false;
    bool panning = false;
    bool drawing = false;
    bool erasing = false;
    bool rubberBanding = false;
    bool movingSelection = false;
    bool shapeDrawing = false;
    QPointF lastPanScreenPosition;
    QPointF rubberBandOrigin;
    QPointF rubberBandCurrent;
    QPointF shapeStartScene;
    QPointF shapeCurrentScene;
    std::vector<QPointF> shapeClickPoints;  // For multi-click tools (polyline, arc)
    std::size_t shapePageIndex = 0U;
    QtToolState currentToolState;
    QtTextEditor* textEditor = nullptr;
};
