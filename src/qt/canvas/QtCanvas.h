/*
 * VertexNote
 *
 * Qt canvas bootstrap.
 */

#pragma once

#include <memory>
#include <optional>
#include <utility>
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
class QInputDevice;

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
    void setPairOffset(int offset);
    [[nodiscard]] auto pairOffset() const -> int;
    void setLayoutColumns(int columns);
    void setLayoutRows(int rows);
    [[nodiscard]] auto layoutColumnsRows() const -> int;
    void setVerticalLayout(bool enabled);
    [[nodiscard]] auto isVerticalLayout() const -> bool;
    void setRightToLeftLayout(bool enabled);
    [[nodiscard]] auto isRightToLeftLayout() const -> bool;
    void setBottomToTopLayout(bool enabled);
    [[nodiscard]] auto isBottomToTopLayout() const -> bool;
    void setGeometrySnapEnabled(bool enabled);
    void setGridSnapEnabled(bool enabled);
    void setRotationSnapEnabled(bool enabled);
    void setViewInteractionOptions(double zoomStepPercent, double zoomStepScrollPercent, double rotationSnapTolerance);
    void setTouchGestureOptions(bool zoomGesturesEnabled, double touchZoomStartThreshold,
                                bool touchInertialScrolling);
    void setDrawDirectionModifiers(bool enabled, int radiusPixels);
    void setPageSpaceOptions(bool horizontalEnabled, int left, int right, bool verticalEnabled, int above, int below);
    void setTouchDrawingEnabled(bool enabled);
    void setPressureOptions(double minimumPressure, double pressureMultiplier, bool pressureGuessing);
    void setStrokeStabilizerOptions(bool enabled, int samples, double strength, bool finalizeStroke,
                                    int averagingMethod, int preprocessor, double sigma, double deadzoneRadius,
                                    double drag, double mass, bool cuspDetection);
    void setGridSnapOptions(double gridSize, double tolerance);
    void setVertexSnapMarkerSize(int sizePixels);
    void setEraserCursorHidden(bool hidden);
    void setInputSystemOptions(int ignoredStylusEvents, bool tpcButtonEnabled, bool drawOutsideWindowEnabled);
    void setPointerButtonActions(const QtPointerButtonMatrix& buttonMatrix);
    void setInputDeviceButtonProfiles(std::vector<QtInputDeviceButtonProfile> profiles);
    void setRestoreLineWidthOnScale(bool enabled);
    void setPageShadowEnabled(bool enabled);
    void setSelectionColor(Color color);
    void setCanvasBackgroundColor(Color color);
    void setCursorHighlightOptions(bool enabled, Color fillColor, Color borderColor, int radiusPixels,
                                   int borderWidthPixels);
    void setRecolorOptions(bool recolorMainView, Color light, Color dark);
    void setShapeRecognizerMinSize(double value);
    void setSnapRecognizedShapesEnabled(bool enabled);
    void setLaserPointerFadeOutMs(int value);
    void setTextEditorTabOptions(bool useSpaces, int numberOfSpaces);
    void setEdgePanOptions(double speed, double maxMultiplier);
    void setUnlimitedScrolling(bool enabled);
    void setStrokeFilterOptions(bool enabled, int ignoreTimeMs, double ignoreLengthMm, int successiveTimeMs,
                                bool doActionOnFiltered, bool trySelectOnFiltered);
    void setEmptyLastPageAppendMode(std::string mode);
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
    [[nodiscard]] auto zoom() const -> double;
    void setZoom(double zoom);

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
    void selectionStateChanged();
    void toolStateChanged();

private:
    enum class InstrumentToolKind { None, Setsquare, Compass };
    enum class InstrumentStrokeKind { None, SetsquareEdge, SetsquareRadial, CompassOutline, CompassRadius };
    enum class GeometryTransformHandle { None, TranslateXY, TranslateX, TranslateY, RotateZ, ScaleUniform };

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

    struct GeometryTransformInteraction {
        GeometryTransformHandle handle = GeometryTransformHandle::None;
        std::size_t pageIndex = 0U;
        QPointF startPagePoint;
        QPointF centerPagePoint;
        double startAngle = 0.0;
        double startRadius = 1.0;
    };

    struct GeometrySelectionSceneBounds {
        std::size_t pageIndex = 0U;
        QRectF bounds;
        QPointF center;
    };

    void updateDebugOverlay(QString summary);
    void emitViewportUpdate(bool edited = true);
    void zoomAroundScreenPoint(double factor, const QPointF& screenPoint);
    [[nodiscard]] auto pageRects() const -> std::vector<QRectF>;
    [[nodiscard]] auto documentSceneBounds() const -> QRectF;
    void constrainScrollToDocumentBounds();
    void drawPageContents(QPainter& painter, const QRectF& rect, const vn::view::render::PageRenderSnapshot& pageInfo,
                          std::size_t pageIndex, bool selected) const;
    void drawGeometryInteractionOverlay(QPainter& painter, const QRectF& rect,
                                        const vn::view::render::PageRenderSnapshot& pageInfo,
                                        std::size_t pageIndex) const;
    void drawGeometryTransformGizmo(QPainter& painter) const;
    void drawOverlayHud(QPainter& painter) const;
    void drawCursorHighlight(QPainter& painter) const;
    [[nodiscard]] auto screenToScene(const QPointF& screenPoint) const -> QPointF;
    [[nodiscard]] auto pageIndexAtScenePoint(const QPointF& scenePoint) const -> std::optional<std::size_t>;
    void updateGeometryHover(const QPointF& screenPoint);
    void clearGeometryHover();
    void selectHoveredGeometry(bool additive = false);
    [[nodiscard]] auto beginSelectedGeometryMoveAtScreen(const QPointF& screenPoint) -> bool;
    [[nodiscard]] auto selectedGeometrySceneBounds() const -> std::optional<GeometrySelectionSceneBounds>;
    [[nodiscard]] auto geometryTransformHandleAtScreen(const QPointF& screenPoint) const -> GeometryTransformHandle;
    [[nodiscard]] auto beginGeometryTransformAtScreen(GeometryTransformHandle handle, const QPointF& screenPoint)
            -> bool;
    void updateGeometryTransformAtScreen(const QPointF& screenPoint);
    void finalizeGeometryTransform();
    void cancelGeometryTransform();
    void beginPan(const QPointF& position);
    void endPan();
    [[nodiscard]] auto pointerButtonMatrixForDevice(const QInputDevice* device) const -> const QtPointerButtonMatrix&;
    [[nodiscard]] auto pointerActionForMouseButton(Qt::MouseButton button, const QInputDevice* device) const
            -> QtPointerButtonAction;
    [[nodiscard]] auto pointerActionForTabletEvent(const QTabletEvent& event) const -> QtPointerButtonAction;
    [[nodiscard]] auto pointerActionForTouchDevice(const QInputDevice* device) const -> QtPointerButtonAction;
    [[nodiscard]] auto beginPointerAction(QtPointerButtonAction action, const QPointF& screenPoint, double pressure) -> bool;
    [[nodiscard]] auto releasePointerAction(QtPointerButtonAction action) -> bool;
    void setCursorForTool(QtToolType tool);
    void refreshToolCursor();
    void beginStrokeAtScreen(const QPointF& screenPoint, double pressure);
    void updateStrokeAtScreen(const QPointF& screenPoint, double pressure);
    [[nodiscard]] auto snapInputPagePoint(std::size_t pageIndex, const QPointF& pagePoint,
                                          std::optional<vn::snap::SnapKind>* snapKind = nullptr,
                                          bool geometrySnapAllowed = true, bool gridSnapAllowed = true) const -> QPointF;
    [[nodiscard]] auto adjustedPressure(double pressure) const -> double;
    [[nodiscard]] auto stabilizedStrokePoint(const QPointF& pagePoint, double pressure) -> std::pair<QPointF, double>;
    void resetStrokeStabilizer(const QPointF& pagePoint, double pressure);
    void maybeFinalizeStabilizedStroke();
    void finalizeActiveStroke();
    [[nodiscard]] auto shouldFilterActiveStroke(qint64 nowMs) -> bool;
    void maybeAppendEmptyLastPageOnDraw(std::size_t pageIndex, bool strokeAdded);
    [[nodiscard]] auto maybeAppendEmptyLastPageOnScroll() -> bool;
    void cancelActiveStroke();
    void drawActiveStroke(QPainter& painter) const;
    void drawLaserPointerStrokes(QPainter& painter) const;
    void pruneLaserPointerStrokes();
    void drawEraserPreview(QPainter& painter) const;
    void beginEraseAtScreen(const QPointF& screenPoint);
    void eraseAtScreen(const QPointF& screenPoint);
    void finalizeErase();
    void cancelErase();
    void updateEraserPreviewAtScreen(const QPointF& screenPoint);
    void clearEraserPreview();
    [[nodiscard]] auto usesMaskEraser() const -> bool;
    [[nodiscard]] auto currentEraserHalfSize() const -> double;

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
    [[nodiscard]] auto selectionScaleHandleAtScreen(const QPointF& screenPoint) const -> int;
    void beginScaleSelectionAtScreen(const QPointF& screenPoint, int handleIndex);
    void updateScaleSelectionAtScreen(const QPointF& screenPoint);
    void finalizeScaleSelection();
    void cancelScaleSelection();
    void drawSelectionOverlay(QPainter& painter) const;
    void drawPdfTextSelectionOverlay(QPainter& painter) const;
    void drawRubberBand(QPainter& painter) const;
    void beginVerticalSpaceAtScreen(const QPointF& screenPoint, bool moveAbove);
    void updateVerticalSpaceAtScreen(const QPointF& screenPoint);
    void finalizeVerticalSpace();
    void cancelVerticalSpace();
    void drawVerticalSpacePreview(QPainter& painter) const;
    void beginPdfTextSelectionAtScreen(const QPointF& screenPoint);
    void updatePdfTextSelectionAtScreen(const QPointF& screenPoint);
    void finalizePdfTextSelection();
    void cancelPdfTextSelection();

    // Shape drawing helpers
    void beginShapeAtScreen(const QPointF& screenPoint);
    void updateShapeAtScreen(const QPointF& screenPoint);
    [[nodiscard]] auto snapShapePoint(std::size_t pageIndex, const QPointF& pagePoint) -> QPointF;
    [[nodiscard]] auto applyShapeDirectionModifiers(const QPointF& pagePoint) -> QPointF;
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
    [[nodiscard]] auto handleTouchGesture(const QTouchEvent& event) -> bool;
    void updateEdgePanAtScreen(const QPointF& screenPoint);
    void stopEdgePan();
    void applyEdgePanStep();
    [[nodiscard]] auto edgePanDeltaFor(const QPointF& screenPoint) const -> QPointF;
    [[nodiscard]] auto hasEdgePanDrag() const -> bool;
    void updateGeometryDragAtScreen(const QPointF& screenPoint);

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
    double minimumPressure = 0.05;
    double pressureMultiplier = 1.0;
    bool pressureGuessing = false;
    bool strokeStabilizerEnabled = false;
    int strokeStabilizerSamples = 6;
    double strokeStabilizerStrength = 0.65;
    bool strokeStabilizerFinalizeStroke = true;
    int strokeStabilizerAveragingMethod = 1;
    int strokeStabilizerPreprocessor = 0;
    double strokeStabilizerSigma = 0.5;
    double strokeStabilizerDeadzoneRadius = 1.3;
    double strokeStabilizerDrag = 0.4;
    double strokeStabilizerMass = 5.0;
    bool strokeStabilizerCuspDetection = true;
    double snapGridTolerance = 0.50;
    double snapGridSize = 14.17;
    int vertexSnapMarkerSize = 15;
    double zoomStepFactor = 1.10;
    double zoomStepScrollFactor = 1.02;
    bool zoomGesturesEnabled = true;
    double touchZoomStartThreshold = 0.0;
    bool touchInertialScrolling = true;
    bool touchZoomGestureActive = false;
    double touchZoomInitialDistance = 0.0;
    double touchZoomLastDistance = 0.0;
    QPointF touchZoomLastCenter;
    double rotationSnapTolerance = 0.30;
    double extraPageSpaceLeft = 0.0;
    double extraPageSpaceRight = 0.0;
    double extraPageSpaceAbove = 0.0;
    double extraPageSpaceBelow = 0.0;
    bool eraserCursorHidden = true;
    bool restoreLineWidthOnScale = false;
    int ignoredStylusEvents = 0;
    int ignoredStylusEventsRemaining = 0;
    bool inputSystemTPCButton = false;
    bool inputSystemDrawOutsideWindow = true;
    QtPointerButtonMatrix buttonMatrix;
    std::vector<QtInputDeviceButtonProfile> inputDeviceButtonProfiles;
    Color selectionColor{0, 120, 255, 255};
    Color canvasBackgroundColor = Colors::xopp_gainsboro02;
    bool cursorHighlightEnabled = false;
    Color cursorHighlightFill{255, 255, 0, 90};
    Color cursorHighlightBorder{255, 0, 0, 180};
    int cursorHighlightRadiusPixels = 30;
    int cursorHighlightBorderWidthPixels = 2;
    bool cursorHighlightVisible = false;
    QPointF lastCursorScreenPosition;
    bool recolorMainView = false;
    Color recolorLight{198, 208, 245, 255};
    Color recolorDark{48, 52, 70, 255};
    bool pairedPagesEnabled = false;
    int pairOffsetValue = 0;
    int layoutColumnsRowsValue = 1;
    bool verticalLayoutEnabled = true;
    bool rightToLeftLayoutEnabled = false;
    bool bottomToTopLayoutEnabled = false;
    bool spaceHeld = false;
    bool panning = false;
    bool drawing = false;
    bool erasing = false;
    bool temporaryRightButtonEraser = false;
    bool rubberBanding = false;
    bool movingSelection = false;
    bool scalingSelection = false;
    std::optional<GeometryTransformInteraction> geometryTransformInteraction;
    bool shapeDrawing = false;
    bool pdfTextSelecting = false;
    bool movingInstrumentOverlay = false;
    bool fitWidthModeEnabled = true;
    bool deferredFitWidthPending = false;
    QPointF lastPanScreenPosition;
    struct StabilizerSample {
        QPointF point;
        double pressure = 0.5;
    };
    std::vector<StabilizerSample> strokeStabilizerSamplesBuffer;
    std::optional<StabilizerSample> lastRawStrokeSample;
    std::optional<StabilizerSample> lastEmittedStrokeSample;
    QPointF strokeStabilizerVelocity;
    QPointF strokeStabilizerDeadzoneDirection;
    std::optional<std::size_t> eraserPreviewPageIndex;
    std::optional<QtPointerButtonAction> activeTouchAction;
    QPointF eraserPreviewPagePoint;
    int activeTouchPointId = -1;
    QPointF rubberBandOrigin;
    QPointF rubberBandCurrent;
    int activeSelectionScaleHandle = -1;
    struct VerticalSpacePreview {
        std::size_t pageIndex = 0U;
        double startY = 0.0;
        double currentY = 0.0;
        bool moveAbove = false;
    };
    std::optional<VerticalSpacePreview> verticalSpacePreview;
    QPointF shapeStartScene;
    QPointF shapeCurrentScene;
    QPointF shapeSnapPoint;
    std::vector<QPointF> shapeClickPoints;  // For multi-click tools (polyline, arc)
    std::optional<vn::snap::SnapKind> shapeSnapKind;
    std::size_t shapePageIndex = 0U;
    InstrumentOverlayState instrumentOverlay;
    std::optional<InstrumentStrokeState> activeInstrumentStroke;
    double shapeRecognizerMinSize = 40.0;
    bool snapRecognizedShapesEnabled = false;
    bool drawDirectionModifiersEnabled = false;
    int drawDirectionModifiersRadiusPixels = 50;
    bool shapeDirectionModifiersFixed = false;
    bool shapeDirectionModifierShift = false;
    bool shapeDirectionModifierControl = false;
    bool shapeEffectiveShiftModifier = false;
    bool shapeEffectiveControlModifier = false;
    int laserPointerFadeOutMs = 1500;
    bool useSpacesForTab = false;
    int numberOfSpacesForTab = 4;
    bool strokeFilterEnabled = false;
    int strokeFilterIgnoreTimeMs = 150;
    double strokeFilterIgnoreLengthMm = 1.0;
    int strokeFilterSuccessiveTimeMs = 500;
    bool doActionOnStrokeFiltered = false;
    bool trySelectOnStrokeFiltered = false;
    qint64 activeStrokeStartedMs = 0;
    qint64 lastFilteredStrokeMs = 0;
    std::string emptyLastPageAppendMode = "disabled";
    struct QtLaserOverlayStroke {
        std::size_t pageIndex = 0U;
        vn::view::render::StrokeRenderModel model;
        qint64 createdMs = 0;
    };
    std::vector<QtLaserOverlayStroke> laserOverlayStrokes;
    QTimer* laserFadeTimer = nullptr;
    QTimer* edgePanTimer = nullptr;
    QPointF edgePanScreenPoint;
    double edgePanSpeed = 20.0;
    double edgePanMaxMultiplier = 5.0;
    bool unlimitedScrolling = false;
    QtToolState currentToolState;
    QtTextEditor* textEditor = nullptr;
};
