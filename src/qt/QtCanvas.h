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

class QTimer;
class QString;

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
    void setPairedPagesEnabled(bool enabled);
    [[nodiscard]] auto isPairedPagesEnabled() const -> bool;
    void setVerticalLayout(bool enabled);
    [[nodiscard]] auto isVerticalLayout() const -> bool;
    void setRightToLeftLayout(bool enabled);
    [[nodiscard]] auto isRightToLeftLayout() const -> bool;
    void setBottomToTopLayout(bool enabled);
    [[nodiscard]] auto isBottomToTopLayout() const -> bool;
    void setGeometrySnapEnabled(bool enabled);
    void setGridSnapEnabled(bool enabled);
    void setRotationSnapEnabled(bool enabled);
    void setTouchDrawingEnabled(bool enabled);
    void setShapeRecognizerMinSize(double value);
    void setLaserPointerFadeOutMs(int value);
    [[nodiscard]] auto isGeometrySnapEnabled() const -> bool;
    [[nodiscard]] auto isGridSnapEnabled() const -> bool;
    [[nodiscard]] auto isRotationSnapEnabled() const -> bool;
    [[nodiscard]] auto isTouchDrawingEnabled() const -> bool;
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

    // Page navigation
    [[nodiscard]] auto currentPageIndex() const -> std::size_t;
    void scrollToPage(std::size_t pageIndex);
    void fitWidth();
    void zoomToActualSize();

    // Content renderer (shared for sidebar thumbnails / export)
    [[nodiscard]] auto contentRenderer() const -> vn::view::render::PageContentRenderer*;

protected:
    void paintEvent(QPaintEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;
    void showEvent(QShowEvent* event) override;
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
    enum class InstrumentToolKind { None, Setsquare, Compass };
    enum class InstrumentStrokeKind { None, SetsquareEdge, SetsquareRadial, CompassOutline, CompassRadius };

    struct InstrumentOverlayState {
        bool visible = false;
        std::size_t pageIndex = 0U;
        QPointF origin;
        double rotation = 0.0;
        double size = 0.0;
    };

    struct InstrumentStrokeState {
        InstrumentStrokeKind kind = InstrumentStrokeKind::None;
        std::size_t pageIndex = 0U;
        QPointF origin;
        double rotation = 0.0;
        double size = 0.0;
        double anchor = 0.0;
        double extentMin = 0.0;
        double extentMax = 0.0;
        double lastAngle = 0.0;
        std::vector<QPointF> previewPoints;
    };

    void updateDebugOverlay(QString summary);
    void emitViewportUpdate(bool edited = true);
    void zoomAroundScreenPoint(double factor, const QPointF& screenPoint);
    [[nodiscard]] auto pageRects() const -> std::vector<QRectF>;
    [[nodiscard]] auto documentSceneBounds() const -> QRectF;
    void drawPageContents(QPainter& painter, const QRectF& rect, const vn::view::render::PageRenderSnapshot& pageInfo,
                          std::size_t pageIndex, bool selected) const;
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
    void drawLaserPointerStrokes(QPainter& painter) const;
    void pruneLaserPointerStrokes();
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
    void drawPdfTextSelectionOverlay(QPainter& painter) const;
    void drawRubberBand(QPainter& painter) const;
    void beginPdfTextSelectionAtScreen(const QPointF& screenPoint);
    void updatePdfTextSelectionAtScreen(const QPointF& screenPoint);
    void finalizePdfTextSelection();
    void cancelPdfTextSelection();

    // Shape drawing helpers
    void beginShapeAtScreen(const QPointF& screenPoint);
    void updateShapeAtScreen(const QPointF& screenPoint);
    void addShapeClickAtScreen(const QPointF& screenPoint);
    void finalizeShape();
    void cancelShape();
    void drawShapePreview(QPainter& painter) const;
    void drawInstrumentOverlay(QPainter& painter) const;
    [[nodiscard]] auto isMultiClickShapeTool() const -> bool;
    [[nodiscard]] auto applyRotationSnap(const QPointF& origin, const QPointF& point) const -> QPointF;
    [[nodiscard]] auto activeInstrumentTool() const -> InstrumentToolKind;
    void ensureInstrumentOverlay(std::size_t pageIndex, const QPointF& pagePoint);
    void beginInstrumentToolAtScreen(const QPointF& screenPoint, Qt::MouseButton button);
    void updateInstrumentToolAtScreen(const QPointF& screenPoint);
    void finalizeInstrumentTool();
    void cancelInstrumentTool();
    void processTouchDrawing(const vn::ui::input::TouchEvent& event);

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
    bool rotationSnapEnabled = false;
    bool touchDrawingEnabled = false;
    bool pairedPagesEnabled = false;
    bool verticalLayoutEnabled = true;
    bool rightToLeftLayoutEnabled = false;
    bool bottomToTopLayoutEnabled = false;
    bool spaceHeld = false;
    bool panning = false;
    bool drawing = false;
    bool erasing = false;
    bool rubberBanding = false;
    bool movingSelection = false;
    bool shapeDrawing = false;
    bool pdfTextSelecting = false;
    bool movingInstrumentOverlay = false;
    bool deferredFitWidthPending = false;
    QPointF lastPanScreenPosition;
    int activeTouchPointId = -1;
    QPointF rubberBandOrigin;
    QPointF rubberBandCurrent;
    QPointF shapeStartScene;
    QPointF shapeCurrentScene;
    std::vector<QPointF> shapeClickPoints;  // For multi-click tools (polyline, arc)
    std::size_t shapePageIndex = 0U;
    InstrumentOverlayState instrumentOverlay;
    std::optional<InstrumentStrokeState> activeInstrumentStroke;
    double shapeRecognizerMinSize = 40.0;
    int laserPointerFadeOutMs = 1500;
    struct QtLaserOverlayStroke {
        std::size_t pageIndex = 0U;
        vn::view::render::StrokeRenderModel model;
        qint64 createdMs = 0;
    };
    std::vector<QtLaserOverlayStroke> laserOverlayStrokes;
    QTimer* laserFadeTimer = nullptr;
    QtToolState currentToolState;
    QtTextEditor* textEditor = nullptr;
};
