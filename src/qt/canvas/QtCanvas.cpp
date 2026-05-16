/*
 * VertexNote
 *
 * Qt canvas bootstrap.
 */

#include "QtCanvas.h"

#include "QtInputDeviceKey.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <iterator>
#include <limits>
#include <utility>

#include <QCursor>
#include <QDateTime>
#include <QEvent>
#include <QFontMetrics>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QPaintEvent>
#include <QPainter>
#include <QPainterPath>
#include <QPalette>
#include <QPen>
#include <QPointingDevice>
#include <QRect>
#include <QApplication>
#include <QBrush>
#include <QClipboard>
#include <QTabletEvent>
#include <QTouchEvent>
#include <QTransform>
#include <QResizeEvent>
#include <QShowEvent>
#include <QStringList>
#include <QTimer>
#include <QWheelEvent>

#include "QtCanvasLayout.h"
#include "QtPreviewBackgroundRenderer.h"
#include "QtPreviewGeometryRenderer.h"
#include "QtPreviewImageRenderer.h"
#include "QtPreviewStrokeRenderer.h"
#include "QtPreviewTextRenderer.h"
#include "QtPageContentRenderer.h"
#include "view/render/QtPainterRenderContext.h"
#include "view/render/StrokeRenderModelFactory.h"

namespace {

constexpr double MIN_ZOOM = 0.1;
constexpr double MAX_ZOOM = 8.0;
constexpr double GEOMETRY_HIT_RADIUS_PIXELS = 10.0;
constexpr double LINKED_VERTEX_EPSILON = 1e-6;
constexpr double ROTATION_SNAP_STEP_RADIANS = M_PI / 12.0;
constexpr double INSTRUMENT_MIN_SIZE = 48.0;
constexpr double INSTRUMENT_MAX_SIZE = 420.0;

auto toQtCursor(vn::ui::common::CanvasCursor cursor) -> Qt::CursorShape {
    using vn::ui::common::CanvasCursor;

    switch (cursor) {
        case CanvasCursor::Arrow:
            return Qt::ArrowCursor;
        case CanvasCursor::Crosshair:
            return Qt::CrossCursor;
        case CanvasCursor::Hand:
            return Qt::OpenHandCursor;
        case CanvasCursor::IBeam:
            return Qt::IBeamCursor;
        case CanvasCursor::Wait:
            return Qt::WaitCursor;
        case CanvasCursor::Hidden:
            return Qt::BlankCursor;
    }

    return Qt::ArrowCursor;
}

auto clampZoom(double zoom) -> double { return std::clamp(zoom, MIN_ZOOM, MAX_ZOOM); }

auto qColorFromColor(Color color, int alphaOverride = -1) -> QColor {
    return QColor(color.red, color.green, color.blue, alphaOverride >= 0 ? alphaOverride : color.alpha);
}

auto coincidentRenderPoint(const Point& lhs, const Point& rhs) -> bool {
    return std::hypot(lhs.x - rhs.x, lhs.y - rhs.y) <= LINKED_VERTEX_EPSILON;
}

auto recolorDifference(Color light, Color dark) -> QColor {
    return QColor(std::abs(static_cast<int>(dark.red) - static_cast<int>(light.red)),
                  std::abs(static_cast<int>(dark.green) - static_cast<int>(light.green)),
                  std::abs(static_cast<int>(dark.blue) - static_cast<int>(light.blue)));
}

auto recolorOffset(Color light, Color dark) -> QColor {
    return QColor(std::min(light.red, dark.red), std::min(light.green, dark.green), std::min(light.blue, dark.blue));
}

auto recolorReference(Color light, Color dark) -> QColor {
    return QColor(light.red < dark.red ? 255 : 0, light.green < dark.green ? 255 : 0, light.blue < dark.blue ? 255 : 0);
}

auto snapColor(std::optional<vn::snap::SnapKind> kind) -> QColor {
    if (!kind) {
        return QColor(45, 125, 255);
    }

    switch (*kind) {
        case vn::snap::SnapKind::Grid:
            return QColor(90, 90, 90);
        case vn::snap::SnapKind::ExplicitVertex:
        case vn::snap::SnapKind::EdgeEndpoint:
            return QColor(0, 115, 255);
        case vn::snap::SnapKind::Midpoint:
            return QColor(0, 166, 89);
        case vn::snap::SnapKind::EdgeProjection:
            return QColor(255, 140, 20);
        case vn::snap::SnapKind::Intersection:
            return QColor(203, 30, 203);
        case vn::snap::SnapKind::ConstraintGuide:
            return QColor(0, 153, 191);
    }

    return QColor(45, 125, 255);
}

auto shouldDrawPassiveSnapMarker(std::optional<vn::snap::SnapKind> kind) -> bool {
    return kind && *kind != vn::snap::SnapKind::EdgeProjection;
}

auto snapHint(std::optional<vn::snap::SnapKind> kind) -> QString {
    if (!kind) {
        return QStringLiteral("hit");
    }

    switch (*kind) {
        case vn::snap::SnapKind::Grid:
            return QStringLiteral("grid");
        case vn::snap::SnapKind::ExplicitVertex:
        case vn::snap::SnapKind::EdgeEndpoint:
            return QStringLiteral("vertex");
        case vn::snap::SnapKind::Midpoint:
            return QStringLiteral("midpoint");
        case vn::snap::SnapKind::EdgeProjection:
            return QStringLiteral("edge projection");
        case vn::snap::SnapKind::Intersection:
            return QStringLiteral("intersection");
        case vn::snap::SnapKind::ConstraintGuide:
            return QStringLiteral("constraint guide");
    }

    return QStringLiteral("hit");
}

auto geometryHitTypeHint(vn::view::render::GeometryHitType type) -> QString {
    switch (type) {
        case vn::view::render::GeometryHitType::Vertex:
            return QStringLiteral("vertex");
        case vn::view::render::GeometryHitType::Edge:
            return QStringLiteral("edge");
        case vn::view::render::GeometryHitType::Face:
            return QStringLiteral("face");
    }

    return QStringLiteral("geometry");
}

auto geometrySelectionModeName(QtGeometrySelectionMode mode) -> QString {
    switch (mode) {
        case QtGeometrySelectionMode::Vertex:
            return QStringLiteral("Vertex");
        case QtGeometrySelectionMode::Edge:
            return QStringLiteral("Edge");
        case QtGeometrySelectionMode::Face:
            return QStringLiteral("Face");
        case QtGeometrySelectionMode::Object:
            return QStringLiteral("Object");
    }

    return QStringLiteral("Geometry");
}

auto geometrySelectionModeAccent(QtGeometrySelectionMode mode) -> QColor {
    switch (mode) {
        case QtGeometrySelectionMode::Vertex:
            return QColor(245, 130, 32);
        case QtGeometrySelectionMode::Edge:
            return QColor(0, 145, 220);
        case QtGeometrySelectionMode::Face:
            return QColor(18, 154, 112);
        case QtGeometrySelectionMode::Object:
            return QColor(132, 85, 214);
    }

    return QColor(45, 125, 255);
}

auto isVertexSnapKind(std::optional<vn::snap::SnapKind> kind) -> bool {
    return kind == vn::snap::SnapKind::ExplicitVertex || kind == vn::snap::SnapKind::EdgeEndpoint;
}

void drawSnapMarker(QPainter& painter, const QPointF& center, std::optional<vn::snap::SnapKind> kind,
                    int vertexMarkerSizePixels) {
    const double scale = std::max(0.1, painter.transform().m11());
    const QColor color = snapColor(kind);
    if (isVertexSnapKind(kind)) {
        const double halfSize = std::clamp(vertexMarkerSizePixels + 6, 12, 56) * 0.5 / scale;
        const QRectF rect(center.x() - halfSize, center.y() - halfSize, halfSize * 2.0, halfSize * 2.0);
        painter.setBrush(QColor(0, 115, 255, 44));
        painter.setPen(QPen(QColor(255, 255, 255, 245), 3.8 / scale));
        painter.drawRect(rect);
        painter.setPen(QPen(QColor(0, 92, 255), 2.4 / scale));
        painter.drawRect(rect.adjusted(0.7 / scale, 0.7 / scale, -0.7 / scale, -0.7 / scale));
        painter.setPen(Qt::NoPen);
        painter.setBrush(QColor(0, 92, 255, 235));
        const double dotSize = 3.4 / scale;
        painter.drawRect(QRectF(center.x() - dotSize / 2.0, center.y() - dotSize / 2.0, dotSize, dotSize));
        return;
    }

    const double radius = 4.8 / scale;
    painter.setPen(QPen(QColor(255, 255, 255, 235), 3.2 / scale));
    painter.setBrush(QColor(color.red(), color.green(), color.blue(), 40));
    painter.drawEllipse(center, radius, radius);
    painter.setPen(QPen(color, 1.4 / scale));
    painter.drawEllipse(center, radius, radius);
    painter.drawLine(QPointF(center.x() - radius, center.y()), QPointF(center.x() + radius, center.y()));
    painter.drawLine(QPointF(center.x(), center.y() - radius), QPointF(center.x(), center.y() + radius));
}

}  // namespace

QtCanvas::QtCanvas(QWidget* parent): QWidget(parent) {
    setObjectName("vertexNoteQtCanvas");
    setMinimumSize(960, 640);
    setAutoFillBackground(true);
    setMouseTracking(true);
    setFocusPolicy(Qt::StrongFocus);
    setAttribute(Qt::WA_AcceptTouchEvents, true);
    auto palette = this->palette();
    palette.setColor(QPalette::Window, QColor(214, 210, 201));
    setPalette(palette);
    this->inputAdapter = std::make_unique<QtInputAdapter>(this);
    this->backgroundRenderer = std::make_unique<vn::view::render::QtPreviewBackgroundRenderer>();
    this->geometryRenderer = std::make_unique<vn::view::render::QtPreviewGeometryRenderer>();
    this->imageRenderer = std::make_unique<vn::view::render::QtPreviewImageRenderer>();
    this->strokeRenderer = std::make_unique<vn::view::render::QtPreviewStrokeRenderer>();
    this->textRenderer = std::make_unique<vn::view::render::QtPreviewTextRenderer>();
    this->pageContentRenderer = std::make_unique<vn::view::render::PageContentRenderer>(
            this->strokeRenderer.get(), this->textRenderer.get(), this->imageRenderer.get(),
            this->backgroundRenderer.get(), this->geometryRenderer.get());
    this->laserFadeTimer = new QTimer(this);
    this->laserFadeTimer->setInterval(33);
    QObject::connect(this->laserFadeTimer, &QTimer::timeout, this, [this]() {
        pruneLaserPointerStrokes();
        if (this->laserOverlayStrokes.empty()) {
            this->laserFadeTimer->stop();
        }
        update();
    });
    this->edgePanTimer = new QTimer(this);
    this->edgePanTimer->setInterval(33);
    QObject::connect(this->edgePanTimer, &QTimer::timeout, this, [this]() { applyEdgePanStep(); });
    newBlankDocument();
}

void QtCanvas::invalidateCanvas() { update(); }

void QtCanvas::invalidateRect(double x, double y, double width, double height) {
    update(QRect(static_cast<int>(x), static_cast<int>(y), static_cast<int>(width), static_cast<int>(height)));
}

void QtCanvas::setCanvasCursor(vn::ui::common::CanvasCursor cursor) {
    setCursor(QCursor(toQtCursor(cursor)));
}

auto QtCanvas::viewport() const -> vn::ui::common::CanvasViewport {
    return {.zoom = this->zoomFactor,
            .scrollX = this->scrollX,
            .scrollY = this->scrollY,
            .width = static_cast<double>(width()),
            .height = static_cast<double>(height()),
            .devicePixelRatio = devicePixelRatioF()};
}

void QtCanvas::handlePointerEvent(const vn::ui::input::PointerEvent& event) {
    updateDebugOverlay(QStringLiteral("pointer x=%1 y=%2 pressure=%3")
                               .arg(event.x, 0, 'f', 1)
                               .arg(event.y, 0, 'f', 1)
                               .arg(event.pressure, 0, 'f', 2));
}

void QtCanvas::handleKeyboardEvent(const vn::ui::input::KeyboardEvent& event) {
    updateDebugOverlay(QStringLiteral("key code=%1 text=%2").arg(event.key).arg(QString::fromStdString(event.text)));
}

void QtCanvas::handleTouchEvent(const vn::ui::input::TouchEvent& event) {
    updateDebugOverlay(QStringLiteral("touch points=%1").arg(static_cast<int>(event.points.size())));
    processTouchDrawing(event);
}

void QtCanvas::setDocumentController(QtDocumentController* documentController) {
    this->documentController = documentController;
    fitWidth();
}

void QtCanvas::newBlankDocument() {
    this->zoomFactor = 1.0;
    this->scrollX = 0.0;
    this->scrollY = 0.0;
    fitWidth();
    updateDebugOverlay(QStringLiteral("new document"));
}

void QtCanvas::setViewportState(double zoom, double scrollX, double scrollY) {
    this->fitWidthModeEnabled = false;
    this->zoomFactor = clampZoom(zoom);
    this->scrollX = scrollX;
    this->scrollY = scrollY;
    emitViewportUpdate(false);
}

auto QtCanvas::sessionViewportState() const -> QtViewportState {
    return {.zoom = this->zoomFactor, .scrollX = this->scrollX, .scrollY = this->scrollY};
}

void QtCanvas::zoomIn() { zoomAroundScreenPoint(this->zoomStepFactor, rect().center()); }

void QtCanvas::zoomOut() { zoomAroundScreenPoint(1.0 / this->zoomStepFactor, rect().center()); }

void QtCanvas::resetViewport() {
    this->fitWidthModeEnabled = false;
    fitPage();
    updateDebugOverlay(QStringLiteral("viewport reset"));
}

void QtCanvas::fitPage(bool edited) {
    this->fitWidthModeEnabled = false;
    const QRectF documentBounds = documentSceneBounds();
    const double padding = 40.0;
    const double availableWidth = std::max(1.0, width() - 2.0 * padding);
    const double availableHeight = std::max(1.0, height() - 2.0 * padding);
    this->zoomFactor =
            clampZoom(std::min(availableWidth / std::max(documentBounds.width(), 1.0),
                               availableHeight / std::max(documentBounds.height(), 1.0)));

    const double visibleWorldWidth = width() / this->zoomFactor;
    const double visibleWorldHeight = height() / this->zoomFactor;
    this->scrollX = documentBounds.left() - (visibleWorldWidth - documentBounds.width()) / 2.0;
    this->scrollY = documentBounds.top() - (visibleWorldHeight - documentBounds.height()) / 2.0;
    emitViewportUpdate(edited);
}

void QtCanvas::panBy(double dx, double dy) {
    this->scrollX += dx;
    this->scrollY += dy;
    emitViewportUpdate();
}

void QtCanvas::setPairedPagesEnabled(bool enabled) {
    const int wantedColumns = enabled ? 2 : 1;
    if (this->pairedPagesEnabled == enabled && this->layoutColumnsRowsValue == wantedColumns) {
        return;
    }
    this->pairedPagesEnabled = enabled;
    this->layoutColumnsRowsValue = wantedColumns;
    this->fitWidthModeEnabled ? fitWidth() : fitPage();
}

auto QtCanvas::isPairedPagesEnabled() const -> bool { return this->pairedPagesEnabled; }

void QtCanvas::setPairOffset(int offset) {
    offset = std::clamp(offset, 0, 1);
    if (this->pairOffsetValue == offset) {
        return;
    }
    this->pairOffsetValue = offset;
    this->fitWidthModeEnabled ? fitWidth() : fitPage();
}

auto QtCanvas::pairOffset() const -> int { return this->pairOffsetValue; }

void QtCanvas::setLayoutColumns(int columns) {
    columns = std::clamp(columns, 1, 8);
    if (this->layoutColumnsRowsValue == columns) {
        return;
    }
    this->layoutColumnsRowsValue = columns;
    this->pairedPagesEnabled = columns == 2;
    this->fitWidthModeEnabled ? fitWidth() : fitPage();
}

void QtCanvas::setLayoutRows(int rows) {
    rows = std::clamp(rows, 1, 8);
    const int value = -rows;
    if (this->layoutColumnsRowsValue == value) {
        return;
    }
    this->layoutColumnsRowsValue = value;
    this->pairedPagesEnabled = false;
    this->fitWidthModeEnabled ? fitWidth() : fitPage();
}

auto QtCanvas::layoutColumnsRows() const -> int { return this->layoutColumnsRowsValue; }

void QtCanvas::setVerticalLayout(bool enabled) {
    if (this->verticalLayoutEnabled == enabled) {
        return;
    }
    this->verticalLayoutEnabled = enabled;
    this->fitWidthModeEnabled ? fitWidth() : fitPage();
}

auto QtCanvas::isVerticalLayout() const -> bool { return this->verticalLayoutEnabled; }

void QtCanvas::setRightToLeftLayout(bool enabled) {
    if (this->rightToLeftLayoutEnabled == enabled) {
        return;
    }
    this->rightToLeftLayoutEnabled = enabled;
    this->fitWidthModeEnabled ? fitWidth() : fitPage();
}

auto QtCanvas::isRightToLeftLayout() const -> bool { return this->rightToLeftLayoutEnabled; }

void QtCanvas::setBottomToTopLayout(bool enabled) {
    if (this->bottomToTopLayoutEnabled == enabled) {
        return;
    }
    this->bottomToTopLayoutEnabled = enabled;
    this->fitWidthModeEnabled ? fitWidth() : fitPage();
}

auto QtCanvas::isBottomToTopLayout() const -> bool { return this->bottomToTopLayoutEnabled; }

auto QtCanvas::currentPageIndex() const -> std::size_t {
    // Determine which page is most visible in the viewport center
    const QPointF center(width() / 2.0, height() / 2.0);
    const QPointF scenePt = screenToScene(center);
    const auto rects = pageRects();
    if (rects.empty()) {
        return 0;
    }
    // Check which page contains the center point
    for (std::size_t i = 0; i < rects.size(); ++i) {
        if (rects[i].contains(scenePt)) {
            return i;
        }
    }
    // Fallback: closest page by vertical distance
    double bestDist = std::numeric_limits<double>::max();
    std::size_t bestIdx = 0;
    for (std::size_t i = 0; i < rects.size(); ++i) {
        const double dist = std::abs(rects[i].center().y() - scenePt.y());
        if (dist < bestDist) {
            bestDist = dist;
            bestIdx = i;
        }
    }
    return bestIdx;
}

void QtCanvas::scrollToPage(std::size_t pageIndex) {
    const auto rects = pageRects();
    if (pageIndex >= rects.size()) {
        return;
    }
    this->scrollX = rects[pageIndex].left() - 40.0;
    this->scrollY = rects[pageIndex].top() - 20.0;
    emitViewportUpdate();
}

void QtCanvas::fitWidth() {
    this->fitWidthModeEnabled = true;
    const auto rects = pageRects();
    if (rects.empty()) {
        return;
    }
    if (!isVisible()) {
        this->deferredFitWidthPending = true;
        return;
    }
    // Find the widest page
    double maxWidth = 0.0;
    for (const auto& r: rects) {
        maxWidth = std::max(maxWidth, r.width());
    }
    const double padding = 24.0;
    const double availableWidth = std::max(1.0, width() - 2.0 * padding);
    this->zoomFactor = clampZoom(availableWidth / std::max(maxWidth, 1.0));
    const double visibleWorldWidth = width() / this->zoomFactor;
    this->scrollX = rects[0].left() - (visibleWorldWidth - maxWidth) / 2.0;
    this->scrollY = std::max(0.0, rects[0].top() - 12.0 / this->zoomFactor);
    this->deferredFitWidthPending = false;
    emitViewportUpdate();
}

void QtCanvas::zoomToActualSize() {
    this->fitWidthModeEnabled = false;
    this->zoomFactor = 1.0;
    emitViewportUpdate();
}

auto QtCanvas::zoom() const -> double { return this->zoomFactor; }

void QtCanvas::setZoom(double zoom) {
    this->fitWidthModeEnabled = false;
    this->zoomFactor = clampZoom(zoom);
    emitViewportUpdate();
}

void QtCanvas::setGeometrySnapEnabled(bool enabled) {
    this->geometrySnapEnabled = enabled;
    update();
}

void QtCanvas::setGridSnapEnabled(bool enabled) {
    this->gridSnapEnabled = enabled;
    update();
}

auto QtCanvas::isGeometrySnapEnabled() const -> bool { return this->geometrySnapEnabled; }

auto QtCanvas::isGridSnapEnabled() const -> bool { return this->gridSnapEnabled; }

void QtCanvas::setPressureOptions(double minimumPressure, double pressureMultiplier, bool pressureGuessing) {
    this->minimumPressure = std::clamp(minimumPressure, 0.0, 0.95);
    this->pressureMultiplier = std::clamp(pressureMultiplier, 0.1, 4.0);
    this->pressureGuessing = pressureGuessing;
}

void QtCanvas::setStrokeStabilizerOptions(bool enabled, int samples, double strength, bool finalizeStroke,
                                          int averagingMethod, int preprocessor, double sigma, double deadzoneRadius,
                                          double drag, double mass, bool cuspDetection) {
    this->strokeStabilizerEnabled = enabled;
    this->strokeStabilizerSamples = std::clamp(samples, 2, 64);
    this->strokeStabilizerStrength = std::clamp(strength, 0.0, 1.0);
    this->strokeStabilizerFinalizeStroke = finalizeStroke;
    this->strokeStabilizerAveragingMethod = std::clamp(averagingMethod, 0, 2);
    this->strokeStabilizerPreprocessor = std::clamp(preprocessor, 0, 2);
    this->strokeStabilizerSigma = std::clamp(sigma, 0.05, 20.0);
    this->strokeStabilizerDeadzoneRadius = std::clamp(deadzoneRadius, 0.0, 100.0);
    this->strokeStabilizerDrag = std::clamp(drag, 0.0, 0.99);
    this->strokeStabilizerMass = std::clamp(mass, 0.1, 100.0);
    this->strokeStabilizerCuspDetection = cuspDetection;
}

void QtCanvas::setGridSnapOptions(double gridSize, double tolerance) {
    this->snapGridSize = std::clamp(gridSize, 1.0, 500.0);
    this->snapGridTolerance = std::clamp(tolerance, 0.01, 10.0);
}

void QtCanvas::setVertexSnapMarkerSize(int sizePixels) { this->vertexSnapMarkerSize = std::clamp(sizePixels, 8, 48); }

void QtCanvas::setEraserCursorHidden(bool hidden) {
    this->eraserCursorHidden = hidden;
    refreshToolCursor();
}

void QtCanvas::setInputSystemOptions(int ignoredEvents, bool tpcButtonEnabled, bool drawOutsideWindowEnabled) {
    this->ignoredStylusEvents = std::clamp(ignoredEvents, 0, 20);
    this->inputSystemTPCButton = tpcButtonEnabled;
    this->inputSystemDrawOutsideWindow = drawOutsideWindowEnabled;
}

void QtCanvas::setRestoreLineWidthOnScale(bool enabled) { this->restoreLineWidthOnScale = enabled; }

void QtCanvas::setPointerButtonActions(const QtPointerButtonMatrix& buttonMatrix) { this->buttonMatrix = buttonMatrix; }

void QtCanvas::setInputDeviceButtonProfiles(std::vector<QtInputDeviceButtonProfile> profiles) {
    this->inputDeviceButtonProfiles = std::move(profiles);
}

void QtCanvas::setPageShadowEnabled(bool enabled) {
    if (auto* renderer = dynamic_cast<vn::view::render::QtPreviewBackgroundRenderer*>(this->backgroundRenderer.get())) {
        renderer->setPageShadowEnabled(enabled);
        update();
    }
}

void QtCanvas::setSelectionColor(Color color) {
    this->selectionColor = color;
    update();
}

void QtCanvas::setCanvasBackgroundColor(Color color) {
    this->canvasBackgroundColor = color;
    update();
}

void QtCanvas::setCursorHighlightOptions(bool enabled, Color fillColor, Color borderColor, int radiusPixels,
                                         int borderWidthPixels) {
    this->cursorHighlightEnabled = enabled;
    this->cursorHighlightFill = fillColor;
    this->cursorHighlightBorder = borderColor;
    this->cursorHighlightRadiusPixels = std::clamp(radiusPixels, 1, 500);
    this->cursorHighlightBorderWidthPixels = std::clamp(borderWidthPixels, 0, 50);
    update();
}

void QtCanvas::setRecolorOptions(bool recolorMainView, Color light, Color dark) {
    this->recolorMainView = recolorMainView;
    this->recolorLight = light;
    this->recolorDark = dark;
    update();
}

void QtCanvas::setGeometryWireframeViewEnabled(bool enabled) {
    this->geometryWireframeViewEnabled = enabled;
    if (auto* renderer = dynamic_cast<vn::view::render::QtPreviewGeometryRenderer*>(this->geometryRenderer.get())) {
        renderer->setWireframeViewEnabled(enabled);
    }
    update();
}

void QtCanvas::setGeometryVertexOverlayEnabled(bool enabled) {
    this->geometryVertexOverlayEnabled = enabled;
    update();
}

void QtCanvas::setGeometryLinkedVertexOverlayEnabled(bool enabled) {
    this->geometryLinkedVertexOverlayEnabled = enabled;
    update();
}

void QtCanvas::setGeometryFaceFillVisible(bool visible) {
    this->geometryFaceFillVisible = visible;
    if (auto* renderer = dynamic_cast<vn::view::render::QtPreviewGeometryRenderer*>(this->geometryRenderer.get())) {
        renderer->setFaceFillVisible(visible);
    }
    update();
}

void QtCanvas::setRotationSnapEnabled(bool enabled) {
    this->rotationSnapEnabled = enabled;
    update();
}

void QtCanvas::setViewInteractionOptions(double zoomStepPercent, double zoomStepScrollPercent,
                                         double rotationSnapTolerance) {
    this->zoomStepFactor = 1.0 + std::clamp(zoomStepPercent, 1.0, 100.0) / 100.0;
    this->zoomStepScrollFactor = 1.0 + std::clamp(zoomStepScrollPercent, 1.0, 100.0) / 100.0;
    this->rotationSnapTolerance = std::clamp(rotationSnapTolerance, 0.01, M_PI / 2.0);
}

void QtCanvas::setTouchGestureOptions(bool zoomEnabled, double zoomStartThreshold, bool inertialScrolling) {
    this->zoomGesturesEnabled = zoomEnabled;
    this->touchZoomStartThreshold = std::clamp(zoomStartThreshold, 0.0, 200.0);
    this->touchInertialScrolling = inertialScrolling;
    if (!this->zoomGesturesEnabled) {
        this->touchZoomGestureActive = false;
        this->touchZoomInitialDistance = 0.0;
        this->touchZoomLastDistance = 0.0;
    }
}

void QtCanvas::setDrawDirectionModifiers(bool enabled, int radiusPixels) {
    this->drawDirectionModifiersEnabled = enabled;
    this->drawDirectionModifiersRadiusPixels = std::clamp(radiusPixels, 1, 500);
}

void QtCanvas::setPageSpaceOptions(bool horizontalEnabled, int left, int right, bool verticalEnabled, int above, int below) {
    this->extraPageSpaceLeft = horizontalEnabled ? std::max(0, left) : 0;
    this->extraPageSpaceRight = horizontalEnabled ? std::max(0, right) : 0;
    this->extraPageSpaceAbove = verticalEnabled ? std::max(0, above) : 0;
    this->extraPageSpaceBelow = verticalEnabled ? std::max(0, below) : 0;
    update();
}

void QtCanvas::setTouchDrawingEnabled(bool enabled) {
    if (!enabled && this->drawing) {
        finalizeActiveStroke();
    }
    this->touchDrawingEnabled = enabled;
}

void QtCanvas::setShapeRecognizerMinSize(double value) { this->shapeRecognizerMinSize = std::max(5.0, value); }

void QtCanvas::setSnapRecognizedShapesEnabled(bool enabled) { this->snapRecognizedShapesEnabled = enabled; }

void QtCanvas::setLaserPointerFadeOutMs(int value) { this->laserPointerFadeOutMs = std::max(100, value); }

void QtCanvas::setTextEditorTabOptions(bool useSpaces, int numberOfSpaces) {
    this->useSpacesForTab = useSpaces;
    this->numberOfSpacesForTab = std::clamp(numberOfSpaces, 1, 32);
    if (this->textEditor) {
        this->textEditor->setTabOptions(this->useSpacesForTab, this->numberOfSpacesForTab);
    }
}

void QtCanvas::setEdgePanOptions(double speed, double maxMultiplier) {
    this->edgePanSpeed = std::clamp(speed, 0.0, 200.0);
    this->edgePanMaxMultiplier = std::clamp(maxMultiplier, 1.0, 20.0);
}

void QtCanvas::setUnlimitedScrolling(bool enabled) {
    this->unlimitedScrolling = enabled;
    constrainScrollToDocumentBounds();
    emitViewportUpdate(false);
}

void QtCanvas::setStrokeFilterOptions(bool enabled, int ignoreTimeMs, double ignoreLengthMm, int successiveTimeMs,
                                      bool doActionOnFiltered, bool trySelectOnFiltered) {
    this->strokeFilterEnabled = enabled;
    this->strokeFilterIgnoreTimeMs = std::clamp(ignoreTimeMs, 0, 5000);
    this->strokeFilterIgnoreLengthMm = std::clamp(ignoreLengthMm, 0.0, 100.0);
    this->strokeFilterSuccessiveTimeMs = std::clamp(successiveTimeMs, 0, 5000);
    this->doActionOnStrokeFiltered = doActionOnFiltered;
    this->trySelectOnStrokeFiltered = trySelectOnFiltered;
}

void QtCanvas::setEmptyLastPageAppendMode(std::string mode) {
    if (mode != "onDrawOfLastPage" && mode != "onScrollOfLastPage") {
        mode = "disabled";
    }
    this->emptyLastPageAppendMode = std::move(mode);
}

auto QtCanvas::isRotationSnapEnabled() const -> bool { return this->rotationSnapEnabled; }

auto QtCanvas::isTouchDrawingEnabled() const -> bool { return this->touchDrawingEnabled; }

auto QtCanvas::isGeometryWireframeViewEnabled() const -> bool { return this->geometryWireframeViewEnabled; }

auto QtCanvas::isGeometryVertexOverlayEnabled() const -> bool { return this->geometryVertexOverlayEnabled; }

auto QtCanvas::isGeometryLinkedVertexOverlayEnabled() const -> bool {
    return this->geometryLinkedVertexOverlayEnabled;
}

auto QtCanvas::isGeometryFaceFillVisible() const -> bool { return this->geometryFaceFillVisible; }

auto QtCanvas::deleteSelectedGeometry() -> bool {
    if (!this->documentController) {
        return false;
    }

    const bool changed = this->documentController->deleteSelectedGeometry();
    if (changed) {
        updateDebugOverlay(QStringLiteral("deleted selected geometry"));
        update();
        Q_EMIT documentEdited();
    }
    return changed;
}

auto QtCanvas::detachSelectedGeometry() -> bool {
    if (!this->documentController) {
        return false;
    }

    const bool changed = this->documentController->detachSelectedGeometry();
    if (changed) {
        updateDebugOverlay(QStringLiteral("detached selected geometry"));
        update();
        Q_EMIT documentEdited();
    }
    return changed;
}

auto QtCanvas::weldSelectedGeometry() -> bool {
    if (!this->documentController) {
        return false;
    }

    const bool changed = this->documentController->weldSelectedGeometry();
    if (changed) {
        updateDebugOverlay(QStringLiteral("welded selected geometry"));
        update();
        Q_EMIT documentEdited();
    }
    return changed;
}

auto QtCanvas::fillSelectedGeometryFace(int fillOpacity) -> bool {
    if (!this->documentController) {
        return false;
    }

    const bool changed = this->documentController->fillSelectedGeometryFace(fillOpacity);
    if (changed) {
        updateDebugOverlay(QStringLiteral("filled selected geometry face"));
        Q_EMIT statusHintChanged(QStringLiteral("Face filled from closed edge loop"));
        update();
        Q_EMIT documentEdited();
    } else {
        const auto status = this->documentController->selectedGeometryFaceLoopStatus();
        Q_EMIT statusHintChanged(QString::fromStdString(status.message));
    }
    return changed;
}

auto QtCanvas::deleteSelectedGeometryFace() -> bool {
    if (!this->documentController) {
        return false;
    }

    const bool changed = this->documentController->deleteSelectedGeometryFace();
    if (changed) {
        updateDebugOverlay(QStringLiteral("deleted selected geometry face"));
        Q_EMIT statusHintChanged(QStringLiteral("Face deleted; edges kept"));
        update();
        Q_EMIT documentEdited();
    } else {
        Q_EMIT statusHintChanged(QStringLiteral("Select a filled face before Delete Face"));
    }
    return changed;
}

auto QtCanvas::splitSelectedGeometryFace() -> bool {
    const auto diagonals =
            this->documentController ? this->documentController->selectedGeometryFaceSplitDiagonals()
                                     : std::vector<QtGeometryFaceDiagonal>{};
    if (diagonals.empty()) {
        Q_EMIT statusHintChanged(QStringLiteral("Split needs one selected face with four or more vertices"));
        return false;
    }
    return splitSelectedGeometryFace(diagonals.front().lhsIndex, diagonals.front().rhsIndex);
}

auto QtCanvas::splitSelectedGeometryFace(std::size_t lhsIndex, std::size_t rhsIndex) -> bool {
    if (!this->documentController) {
        return false;
    }

    const bool changed = this->documentController->splitSelectedGeometryFace(lhsIndex, rhsIndex);
    if (changed) {
        updateDebugOverlay(QStringLiteral("split selected geometry face"));
        Q_EMIT statusHintChanged(QStringLiteral("Face split"));
        update();
        Q_EMIT documentEdited();
    } else {
        Q_EMIT statusHintChanged(QStringLiteral("Split needs one selected face with four or more vertices"));
    }
    return changed;
}

void QtCanvas::setGeometryFaceSplitPreview(std::optional<QtGeometryFaceDiagonal> diagonal) {
    this->geometryFaceSplitPreview = diagonal;
    update();
}

void QtCanvas::clearGeometryFaceSplitPreview() {
    if (!this->geometryFaceSplitPreview) {
        return;
    }
    this->geometryFaceSplitPreview.reset();
    update();
}

auto QtCanvas::triangulateSelectedGeometryFace() -> bool {
    if (!this->documentController) {
        return false;
    }

    const bool changed = this->documentController->triangulateSelectedGeometryFace();
    if (changed) {
        updateDebugOverlay(QStringLiteral("triangulated selected geometry face"));
        Q_EMIT statusHintChanged(QStringLiteral("Face triangulated"));
        update();
        Q_EMIT documentEdited();
    } else {
        Q_EMIT statusHintChanged(QStringLiteral("Triangulate needs one selected filled face"));
    }
    return changed;
}

auto QtCanvas::geometryProjectionCameraAtPagePoint(vn::geom::Vec2 offset) const -> vn::geom::ProjectionCamera {
    switch (this->geometryProjectionView) {
        case GeometryProjectionView::Isometric:
            return vn::geom::ProjectionCamera{.yaw = 0.7853981633974483,
                                              .pitch = -0.5235987755982988,
                                              .roll = 0.0,
                                              .zoom = 1.0,
                                              .offset = offset};
        case GeometryProjectionView::Front:
            return vn::geom::ProjectionCamera{.yaw = 0.0, .pitch = 0.0, .roll = 0.0, .zoom = 1.0, .offset = offset};
        case GeometryProjectionView::Top:
            return vn::geom::ProjectionCamera{.yaw = 0.0,
                                              .pitch = -1.5707963267948966,
                                              .roll = 0.0,
                                              .zoom = 1.0,
                                              .offset = offset};
    }

    return vn::geom::ProjectionCamera{.yaw = 0.7853981633974483,
                                      .pitch = -0.5235987755982988,
                                      .roll = 0.0,
                                      .zoom = 1.0,
                                      .offset = offset};
}

auto QtCanvas::createVertex3D() -> bool {
    if (!this->documentController) {
        return false;
    }

    const auto pageIndex = currentPageIndex();
    const auto& pages = this->documentController->snapshotPages();
    if (pageIndex >= pages.size()) {
        return false;
    }

    vn::geom::Vec3 modelPosition{0.0, 0.0, 0.0};
    std::optional<vn::geom::ProjectionCamera> camera;
    if (this->documentController->selectedGeometry() &&
        this->documentController->selectedGeometry()->pageIndex == pageIndex) {
        if (const auto range = this->documentController->selectedGeometryModelRange()) {
            modelPosition = vn::geom::Vec3{(range->minX + range->maxX) * 0.5,
                                           (range->minY + range->maxY) * 0.5,
                                           (range->minZ + range->maxZ) * 0.5};
        }
        camera = geometryProjectionCameraForActiveView();
    }
    if (!camera) {
        const auto& page = pages[pageIndex];
        camera = geometryProjectionCameraAtPagePoint(vn::geom::Vec2{page.width * 0.5, page.height * 0.42});
    }

    auto created = this->documentController->createVertex3D(pageIndex, modelPosition, *camera,
                                                            this->currentToolState.penColor,
                                                            this->currentToolState.penWidth);
    if (!created) {
        return false;
    }

    this->documentController->setSelectedGeometry(*created);
    this->documentController->clearElementSelection();
    updateDebugOverlay(QStringLiteral("created 3D vertex"));
    Q_EMIT statusHintChanged(QStringLiteral("Created 3D vertex; edit X/Y/Z in the 3D panel"));
    update();
    Q_EMIT selectionStateChanged();
    Q_EMIT documentEdited();
    return true;
}

auto QtCanvas::createEdge3D() -> bool {
    if (!this->documentController) {
        return false;
    }

    const auto pageIndex = currentPageIndex();
    const auto& pages = this->documentController->snapshotPages();
    if (pageIndex >= pages.size()) {
        return false;
    }

    const auto& page = pages[pageIndex];
    const double length = std::max(48.0, std::min(page.width, page.height) * 0.14);
    vn::geom::Vec3 center{0.0, 0.0, 0.0};
    std::optional<vn::geom::ProjectionCamera> camera;
    if (this->documentController->selectedGeometry() &&
        this->documentController->selectedGeometry()->pageIndex == pageIndex) {
        if (const auto range = this->documentController->selectedGeometryModelRange()) {
            center = vn::geom::Vec3{(range->minX + range->maxX) * 0.5,
                                    (range->minY + range->maxY) * 0.5,
                                    (range->minZ + range->maxZ) * 0.5};
        }
        camera = geometryProjectionCameraForActiveView();
    }
    if (!camera) {
        camera = geometryProjectionCameraAtPagePoint(vn::geom::Vec2{page.width * 0.5, page.height * 0.42});
    }

    const vn::geom::Vec3 start{center.x - length * 0.5, center.y, center.z};
    const vn::geom::Vec3 end{center.x + length * 0.5, center.y, center.z};
    auto created = this->documentController->createEdge3D(pageIndex, start, end, *camera,
                                                          this->currentToolState.penColor,
                                                          this->currentToolState.penWidth);
    if (!created) {
        return false;
    }

    this->documentController->setSelectedGeometry(*created);
    this->documentController->clearElementSelection();
    updateDebugOverlay(QStringLiteral("created 3D edge"));
    Q_EMIT statusHintChanged(QStringLiteral("Created 3D edge; edit endpoints or center from the 3D panel"));
    update();
    Q_EMIT selectionStateChanged();
    Q_EMIT documentEdited();
    return true;
}

auto QtCanvas::createWireframeBox3D() -> bool {
    if (!this->documentController) {
        return false;
    }

    const auto pageIndex = currentPageIndex();
    const auto& pages = this->documentController->snapshotPages();
    if (pageIndex >= pages.size()) {
        return false;
    }

    const auto& page = pages[pageIndex];
    const double size = std::max(48.0, std::min(page.width, page.height) * 0.18);
    const int fill = this->currentToolState.fillEnabled ? this->currentToolState.fillOpacity : -1;
    const auto* created = this->documentController->createWireframeBox3D(
            pageIndex, page.width * 0.5, page.height * 0.42, size, size, this->currentToolState.penColor,
            this->currentToolState.penWidth, fill);
    if (!created) {
        return false;
    }

    updateDebugOverlay(QStringLiteral("created 3D wireframe box"));
    update();
    Q_EMIT documentEdited();
    return true;
}

auto QtCanvas::geometryProjectionCameraForSelection(double yaw, double pitch, double roll) const
        -> std::optional<vn::geom::ProjectionCamera> {
    const auto bounds = selectedGeometrySceneBounds();
    if (!bounds) {
        return std::nullopt;
    }

    const auto rects = pageRects();
    if (bounds->pageIndex >= rects.size()) {
        return std::nullopt;
    }

    const auto& pageRect = rects[bounds->pageIndex];
    return vn::geom::ProjectionCamera{.yaw = yaw,
                                      .pitch = pitch,
                                      .roll = roll,
                                      .zoom = 1.0,
                                      .offset = vn::geom::Vec2{bounds->center.x() - pageRect.x(),
                                                               bounds->center.y() - pageRect.y()}};
}

auto QtCanvas::geometryProjectionCameraForActiveView() const -> std::optional<vn::geom::ProjectionCamera> {
    switch (this->geometryProjectionView) {
        case GeometryProjectionView::Isometric:
            return geometryProjectionCameraForSelection(0.7853981633974483, -0.5235987755982988);
        case GeometryProjectionView::Front:
            return geometryProjectionCameraForSelection(0.0, 0.0);
        case GeometryProjectionView::Top:
            return geometryProjectionCameraForSelection(0.0, -1.5707963267948966);
    }

    return geometryProjectionCameraForSelection(0.7853981633974483, -0.5235987755982988);
}

auto QtCanvas::geometryProjectionViewName() const -> QString {
    switch (this->geometryProjectionView) {
        case GeometryProjectionView::Isometric:
            return QStringLiteral("Iso");
        case GeometryProjectionView::Front:
            return QStringLiteral("Front");
        case GeometryProjectionView::Top:
            return QStringLiteral("Top");
    }

    return QStringLiteral("Iso");
}

auto QtCanvas::isGeometryProjectionIsometric() const -> bool {
    return this->geometryProjectionView == GeometryProjectionView::Isometric;
}

auto QtCanvas::isGeometryProjectionFront() const -> bool {
    return this->geometryProjectionView == GeometryProjectionView::Front;
}

auto QtCanvas::isGeometryProjectionTop() const -> bool {
    return this->geometryProjectionView == GeometryProjectionView::Top;
}

auto QtCanvas::projectSelectedGeometry3DIsometric() -> bool {
    if (!this->documentController) {
        return false;
    }

    const auto camera = geometryProjectionCameraForSelection(0.7853981633974483, -0.5235987755982988);
    const bool changed = camera && this->documentController->projectSelectedGeometry3D(*camera);
    if (changed) {
        this->geometryProjectionView = GeometryProjectionView::Isometric;
        updateDebugOverlay(QStringLiteral("projected geometry isometric"));
        Q_EMIT statusHintChanged(QStringLiteral("3D projection: isometric view of the same geometry"));
        update();
        Q_EMIT documentEdited();
    } else {
        Q_EMIT statusHintChanged(QStringLiteral("Select 3D geometry before projecting"));
    }
    return changed;
}

auto QtCanvas::projectSelectedGeometry3DFront() -> bool {
    if (!this->documentController) {
        return false;
    }

    const auto camera = geometryProjectionCameraForSelection(0.0, 0.0);
    const bool changed = camera && this->documentController->projectSelectedGeometry3D(*camera);
    if (changed) {
        this->geometryProjectionView = GeometryProjectionView::Front;
        updateDebugOverlay(QStringLiteral("projected geometry front"));
        Q_EMIT statusHintChanged(QStringLiteral("3D projection: front view of the same geometry"));
        update();
        Q_EMIT documentEdited();
    } else {
        Q_EMIT statusHintChanged(QStringLiteral("Select 3D geometry before projecting"));
    }
    return changed;
}

auto QtCanvas::projectSelectedGeometry3DTop() -> bool {
    if (!this->documentController) {
        return false;
    }

    const auto camera = geometryProjectionCameraForSelection(0.0, -1.5707963267948966);
    const bool changed = camera && this->documentController->projectSelectedGeometry3D(*camera);
    if (changed) {
        this->geometryProjectionView = GeometryProjectionView::Top;
        updateDebugOverlay(QStringLiteral("projected geometry top"));
        Q_EMIT statusHintChanged(QStringLiteral("3D projection: top view of the same geometry"));
        update();
        Q_EMIT documentEdited();
    } else {
        Q_EMIT statusHintChanged(QStringLiteral("Select 3D geometry before projecting"));
    }
    return changed;
}

auto QtCanvas::nudgeSelectedGeometryZ(double delta) -> bool {
    if (!this->documentController) {
        return false;
    }

    const auto camera = geometryProjectionCameraForActiveView();
    const bool changed = camera && this->documentController->nudgeSelectedGeometryZ(delta, *camera);
    if (changed) {
        updateDebugOverlay(QStringLiteral("nudged geometry z"));
        Q_EMIT statusHintChanged(delta >= 0.0 ? QStringLiteral("Depth Z increased; projection refreshed")
                                              : QStringLiteral("Depth Z decreased; projection refreshed"));
        update();
        Q_EMIT documentEdited();
    } else {
        Q_EMIT statusHintChanged(QStringLiteral("Select geometry vertices before changing Z depth"));
    }
    return changed;
}

auto QtCanvas::setSelectedGeometryZ(double z) -> bool {
    if (!this->documentController) {
        return false;
    }

    const auto camera = geometryProjectionCameraForActiveView();
    const bool changed = camera && this->documentController->setSelectedGeometryZ(z, *camera);
    if (changed) {
        updateDebugOverlay(QStringLiteral("set geometry z"));
        Q_EMIT statusHintChanged(QStringLiteral("Depth Z set to %1; projection refreshed").arg(z, 0, 'f', 1));
        update();
        Q_EMIT documentEdited();
    }
    return changed;
}

auto QtCanvas::setSelectedGeometryModelCenter(double x, double y, double z) -> bool {
    if (!this->documentController) {
        return false;
    }

    const auto camera = geometryProjectionCameraForActiveView();
    const bool changed = camera && this->documentController->setSelectedGeometryModelCenter(
                                           vn::geom::Vec3{x, y, z}, *camera);
    if (changed) {
        updateDebugOverlay(QStringLiteral("set geometry 3d position"));
        Q_EMIT statusHintChanged(QStringLiteral("3D center set to X %1, Y %2, Z %3; projection refreshed")
                                         .arg(x, 0, 'f', 1)
                                         .arg(y, 0, 'f', 1)
                                         .arg(z, 0, 'f', 1));
        update();
        Q_EMIT documentEdited();
    }
    return changed;
}

auto QtCanvas::insertVertexOnSelectedEdge() -> bool {
    if (!this->documentController) {
        return false;
    }

    const bool changed = this->documentController->insertVertexOnSelectedEdge();
    if (changed) {
        updateDebugOverlay(QStringLiteral("inserted geometry vertex"));
        update();
        Q_EMIT documentEdited();
    }
    return changed;
}

auto QtCanvas::canUndoGeometryEdit() const -> bool {
    return this->documentController && this->documentController->canUndoGeometryEdit();
}

auto QtCanvas::canRedoGeometryEdit() const -> bool {
    return this->documentController && this->documentController->canRedoGeometryEdit();
}

auto QtCanvas::undoGeometryEdit() -> bool {
    if (!this->documentController) {
        return false;
    }

    const bool changed = this->documentController->undoGeometryEdit();
    if (changed) {
        updateDebugOverlay(QStringLiteral("undo geometry edit"));
        update();
        Q_EMIT documentEdited();
    }
    return changed;
}

auto QtCanvas::redoGeometryEdit() -> bool {
    if (!this->documentController) {
        return false;
    }

    const bool changed = this->documentController->redoGeometryEdit();
    if (changed) {
        updateDebugOverlay(QStringLiteral("redo geometry edit"));
        update();
        Q_EMIT documentEdited();
    }
    return changed;
}

void QtCanvas::paintEvent(QPaintEvent* event) {
    QWidget::paintEvent(event);

    QPainter painter(this);
    vn::view::render::QtPainterRenderContext renderContext(&painter, devicePixelRatioF());
    painter.fillRect(rect(), qColorFromColor(this->canvasBackgroundColor));

    QTransform viewTransform;
    viewTransform.translate(-this->scrollX * this->zoomFactor, -this->scrollY * this->zoomFactor);
    viewTransform.scale(this->zoomFactor, this->zoomFactor);
    painter.setTransform(viewTransform);

    painter.setRenderHint(QPainter::Antialiasing, true);
    const auto rects = pageRects();
    const auto currentPage = currentPageIndex();
    const QRectF visibleScene(this->scrollX, this->scrollY, width() / this->zoomFactor, height() / this->zoomFactor);
    std::vector<std::size_t> visiblePageIndices;
    for (std::size_t index = 0; index < rects.size(); ++index) {
        if (rects[index].intersects(visibleScene)) {
            visiblePageIndices.push_back(index);
        }
    }
    if (visiblePageIndices.empty() && currentPage < rects.size()) {
        visiblePageIndices.push_back(currentPage);
    }
    if (this->documentController) {
        this->documentController->preparePdfRasterCache(visiblePageIndices);
    }
    const auto pages =
            this->documentController ? this->documentController->snapshotPages()
                                     : std::vector<vn::view::render::PageRenderSnapshot>{};
    for (std::size_t index = 0; index < rects.size(); ++index) {
        if (std::ranges::find(visiblePageIndices, index) == visiblePageIndices.end()) {
            continue;
        }
        drawPageContents(painter, rects[index],
                         index < pages.size() ? pages[index] : vn::view::render::PageRenderSnapshot{}, index,
                         index == currentPage);
    }

    drawActiveStroke(painter);
    drawLaserPointerStrokes(painter);
    drawSelectionOverlay(painter);
    drawPdfTextSelectionOverlay(painter);
    drawRubberBand(painter);
    drawVerticalSpacePreview(painter);
    drawShapePreview(painter);
    drawInstrumentOverlay(painter);
    drawGeometryTransformGizmo(painter);
    drawEraserPreview(painter);
    drawCursorHighlight(painter);

    if (this->recolorMainView) {
        painter.resetTransform();
        painter.setCompositionMode(QPainter::CompositionMode_Difference);
        painter.fillRect(rect(), recolorReference(this->recolorLight, this->recolorDark));
        painter.setCompositionMode(QPainter::CompositionMode_Multiply);
        painter.fillRect(rect(), recolorDifference(this->recolorLight, this->recolorDark));
        painter.setCompositionMode(QPainter::CompositionMode_Plus);
        painter.fillRect(rect(), recolorOffset(this->recolorLight, this->recolorDark));
        painter.setCompositionMode(QPainter::CompositionMode_SourceOver);
    }

    painter.resetTransform();

    if (event) {
        event->accept();
    }
}

void QtCanvas::resizeEvent(QResizeEvent* event) {
    QWidget::resizeEvent(event);
    if ((this->deferredFitWidthPending || this->fitWidthModeEnabled) && width() > 0 && height() > 0) {
        fitWidth();
    }
}

void QtCanvas::showEvent(QShowEvent* event) {
    QWidget::showEvent(event);
    if ((this->deferredFitWidthPending || this->fitWidthModeEnabled) && width() > 0 && height() > 0) {
        fitWidth();
    }
}

void QtCanvas::mousePressEvent(QMouseEvent* event) {
    this->inputAdapter->handleMousePress(*event);
    if (this->spaceHeld && event->button() == Qt::LeftButton) {
        beginPan(event->position());
        event->accept();
        return;
    }
    if (this->shapeDrawing && event->button() == Qt::RightButton) {
        cancelShape();
        event->accept();
        return;
    }
    if (beginPointerAction(pointerActionForMouseButton(event->button(), event->device()), event->position(), 0.5)) {
        event->accept();
        return;
    }
    if (this->currentToolState.activeTool == QtToolType::Setsquare ||
        this->currentToolState.activeTool == QtToolType::Compass) {
        beginInstrumentToolAtScreen(event->position(), event->button());
        event->accept();
        return;
    }
    if (event->button() == Qt::LeftButton) {
        const auto tool = this->currentToolState.activeTool;
        if (tool == QtToolType::Pen || tool == QtToolType::Highlighter || tool == QtToolType::LaserPointerPen ||
            tool == QtToolType::LaserPointerHighlighter || tool == QtToolType::ShapeRecognizer) {
            beginStrokeAtScreen(event->position(), 0.5);
            event->accept();
            return;
        }
        if (tool == QtToolType::Eraser) {
            beginEraseAtScreen(event->position());
            event->accept();
            return;
        }
        if (tool == QtToolType::Text) {
            beginTextEditAtScreen(event->position());
            event->accept();
            return;
        }
        if (tool == QtToolType::PdfTextLinear || tool == QtToolType::PdfTextRect) {
            beginPdfTextSelectionAtScreen(event->position());
            event->accept();
            return;
        }
        if (tool == QtToolType::VerticalSpace) {
            beginVerticalSpaceAtScreen(event->position(), event->modifiers().testFlag(Qt::ControlModifier));
            event->accept();
            return;
        }
        if (tool == QtToolType::SelectObject) {
            const auto transformHandle = geometryTransformHandleAtScreen(event->position());
            if (transformHandle != GeometryTransformHandle::None &&
                beginGeometryTransformAtScreen(transformHandle, event->position())) {
                event->accept();
                return;
            }
            updateGeometryHover(event->position());
            if (this->documentController && this->documentController->hoveredGeometry()) {
                selectHoveredGeometry(event->modifiers().testFlag(Qt::ShiftModifier));
                this->documentController->clearElementSelection();
                static_cast<void>(beginSelectedGeometryMoveAtScreen(event->position()));
            } else {
                selectElementAtScreen(event->position(), event->modifiers().testFlag(Qt::ShiftModifier));
            }
            event->accept();
            return;
        }
        if (tool == QtToolType::SelectRect) {
            const auto transformHandle = geometryTransformHandleAtScreen(event->position());
            if (transformHandle != GeometryTransformHandle::None &&
                beginGeometryTransformAtScreen(transformHandle, event->position())) {
                event->accept();
                return;
            }
            if (this->documentController && this->documentController->elementSelection()) {
                const int handleIndex = selectionScaleHandleAtScreen(event->position());
                if (handleIndex >= 0) {
                    beginScaleSelectionAtScreen(event->position(), handleIndex);
                    event->accept();
                    return;
                }

                // Check if clicking on an already-selected element to start a move
                const auto& sel = *this->documentController->elementSelection();
                const QPointF scenePoint = screenToScene(event->position());
                const auto pageIdx = pageIndexAtScenePoint(scenePoint);
                if (pageIdx && *pageIdx == sel.pageIndex) {
                    const auto rects = pageRects();
                    const double pageX = scenePoint.x() - rects[*pageIdx].x();
                    const double pageY = scenePoint.y() - rects[*pageIdx].y();
                    const double hitRadius = 10.0 / this->zoomFactor;
                    const Element* hit = this->documentController->hitTestElement(*pageIdx, pageX, pageY, hitRadius);
                    if (hit && this->documentController->isElementSelected(hit)) {
                        beginMoveSelectionAtScreen(event->position());
                        event->accept();
                        return;
                    }
                }
            }
            // Start rubber band selection or single-click select
            beginRubberBand(event->position());
            event->accept();
            return;
        }
        if (this->currentToolState.isShapeDrawingTool()) {
            if (this->shapeDrawing) {
                addShapeClickAtScreen(event->position());
            } else {
                beginShapeAtScreen(event->position());
            }
            event->accept();
            return;
        }
        updateGeometryHover(event->position());
        selectHoveredGeometry(event->modifiers().testFlag(Qt::ShiftModifier));
        static_cast<void>(beginSelectedGeometryMoveAtScreen(event->position()));
        event->accept();
        return;
    }
    QWidget::mousePressEvent(event);
}

void QtCanvas::mouseDoubleClickEvent(QMouseEvent* event) {
    this->inputAdapter->handleMousePress(*event);
    if (event->button() == Qt::LeftButton) {
        // Double-click finalizes multi-click shape tools
        if (this->shapeDrawing && isMultiClickShapeTool()) {
            addShapeClickAtScreen(event->position());
            if (this->shapeDrawing) {
                finalizeShape();
            }
            event->accept();
            return;
        }
        updateGeometryHover(event->position());
        selectHoveredGeometry(event->modifiers().testFlag(Qt::ShiftModifier));
        if (insertVertexOnSelectedEdge()) {
            event->accept();
            return;
        }
    }
    QWidget::mouseDoubleClickEvent(event);
}

void QtCanvas::mouseReleaseEvent(QMouseEvent* event) {
    this->inputAdapter->handleMouseRelease(*event);
    stopEdgePan();
    const auto pointerAction = pointerActionForMouseButton(event->button(), event->device());
    if (this->panning && (pointerAction == QtPointerButtonAction::Pan ||
                          (this->spaceHeld && event->button() == Qt::LeftButton))) {
        endPan();
        event->accept();
        return;
    }
    if ((this->currentToolState.activeTool == QtToolType::Setsquare ||
         this->currentToolState.activeTool == QtToolType::Compass) &&
        (event->button() == Qt::LeftButton || event->button() == Qt::RightButton)) {
        if (this->movingInstrumentOverlay) {
            this->movingInstrumentOverlay = false;
            event->accept();
            return;
        }
        if (this->activeInstrumentStroke && event->button() == Qt::LeftButton) {
            finalizeInstrumentTool();
            event->accept();
            return;
        }
    }
    if (event->button() == Qt::LeftButton && this->drawing) {
        finalizeActiveStroke();
        event->accept();
        return;
    }
    if (releasePointerAction(pointerAction)) {
        event->accept();
        return;
    }
    if (event->button() == Qt::LeftButton && this->erasing) {
        finalizeErase();
        event->accept();
        return;
    }
    if (event->button() == Qt::LeftButton && this->movingSelection) {
        finalizeMoveSelection();
        event->accept();
        return;
    }
    if (event->button() == Qt::LeftButton && this->scalingSelection) {
        finalizeScaleSelection();
        event->accept();
        return;
    }
    if (event->button() == Qt::LeftButton && this->geometryTransformInteraction) {
        finalizeGeometryTransform();
        event->accept();
        return;
    }
    if (event->button() == Qt::LeftButton && this->rubberBanding) {
        finalizeRubberBand();
        event->accept();
        return;
    }
    if (event->button() == Qt::LeftButton && this->pdfTextSelecting) {
        finalizePdfTextSelection();
        event->accept();
        return;
    }
    if (event->button() == Qt::LeftButton && this->documentController && this->documentController->isVerticalSpacing()) {
        finalizeVerticalSpace();
        event->accept();
        return;
    }
    if (event->button() == Qt::LeftButton && this->shapeDrawing && !isMultiClickShapeTool()) {
        finalizeShape();
        event->accept();
        return;
    }
    if (event->button() == Qt::LeftButton && this->documentController && this->documentController->activeGeometryDrag()) {
        const bool changed = this->documentController->endGeometryVertexDrag();
        const auto& dragMessage = this->documentController->lastGeometryDragMessage();
        if (!dragMessage.empty()) {
            updateDebugOverlay(QString::fromStdString(dragMessage));
        }
        if (!this->spaceHeld) {
            setCursor(Qt::CrossCursor);
        }
        update();
        if (changed) {
            Q_EMIT documentEdited();
        }
        event->accept();
        return;
    }
    QWidget::mouseReleaseEvent(event);
}

void QtCanvas::mouseMoveEvent(QMouseEvent* event) {
    this->lastCursorScreenPosition = event->position();
    this->cursorHighlightVisible = true;
    this->inputAdapter->handleMouseMove(*event);
    updateEraserPreviewAtScreen(event->position());
    if (this->panning) {
        const QPointF delta = event->position() - this->lastPanScreenPosition;
        this->lastPanScreenPosition = event->position();
        this->scrollX -= delta.x() / this->zoomFactor;
        this->scrollY -= delta.y() / this->zoomFactor;
        updateDebugOverlay(QStringLiteral("pan dx=%1 dy=%2").arg(delta.x(), 0, 'f', 1).arg(delta.y(), 0, 'f', 1));
        emitViewportUpdate();
        event->accept();
        return;
    }
    if (this->drawing) {
        updateEdgePanAtScreen(event->position());
        updateStrokeAtScreen(event->position(), 0.5);
        event->accept();
        return;
    }
    if (this->movingInstrumentOverlay || this->activeInstrumentStroke) {
        updateInstrumentToolAtScreen(event->position());
        event->accept();
        return;
    }
    if (this->erasing) {
        updateEdgePanAtScreen(event->position());
        eraseAtScreen(event->position());
        event->accept();
        return;
    }
    if (this->movingSelection) {
        updateEdgePanAtScreen(event->position());
        updateMoveSelectionAtScreen(event->position());
        event->accept();
        return;
    }
    if (this->scalingSelection) {
        updateEdgePanAtScreen(event->position());
        updateScaleSelectionAtScreen(event->position());
        event->accept();
        return;
    }
    if (this->geometryTransformInteraction) {
        updateEdgePanAtScreen(event->position());
        updateGeometryTransformAtScreen(event->position());
        event->accept();
        return;
    }
    if (this->rubberBanding) {
        updateEdgePanAtScreen(event->position());
        updateRubberBand(event->position());
        event->accept();
        return;
    }
    if (this->pdfTextSelecting) {
        updateEdgePanAtScreen(event->position());
        updatePdfTextSelectionAtScreen(event->position());
        event->accept();
        return;
    }
    if (this->documentController && this->documentController->isVerticalSpacing()) {
        updateEdgePanAtScreen(event->position());
        updateVerticalSpaceAtScreen(event->position());
        event->accept();
        return;
    }
    if (this->shapeDrawing) {
        updateEdgePanAtScreen(event->position());
        updateShapeAtScreen(event->position());
        event->accept();
        return;
    }
    if (this->documentController && this->documentController->activeGeometryDrag()) {
        updateEdgePanAtScreen(event->position());
        updateGeometryDragAtScreen(event->position());
        event->accept();
        return;
    }
    stopEdgePan();
    const bool selectionTool = this->currentToolState.activeTool == QtToolType::SelectRect ||
                               this->currentToolState.activeTool == QtToolType::SelectObject;
    updateGeometryHover(event->position());
    const auto transformHandle = selectionTool ? geometryTransformHandleAtScreen(event->position())
                                               : GeometryTransformHandle::None;
    if (this->hoveredGeometryTransformHandle != transformHandle) {
        this->hoveredGeometryTransformHandle = transformHandle;
        update();
    }
    if (transformHandle != GeometryTransformHandle::None) {
        setGeometryTransformCursor(transformHandle);
    } else if (this->currentToolState.activeTool == QtToolType::SelectRect &&
               selectionScaleHandleAtScreen(event->position()) >= 0) {
        setCursor(Qt::SizeFDiagCursor);
    } else if (this->currentToolState.activeTool == QtToolType::SelectRect) {
        setCursor(Qt::ArrowCursor);
    }
    QWidget::mouseMoveEvent(event);
}

void QtCanvas::wheelEvent(QWheelEvent* event) {
    this->inputAdapter->handleWheel(*event);
    if ((this->currentToolState.activeTool == QtToolType::Setsquare ||
         this->currentToolState.activeTool == QtToolType::Compass) &&
        this->instrumentOverlay.visible) {
        if (event->modifiers().testFlag(Qt::AltModifier)) {
            const double factor = event->angleDelta().y() >= 0 ? this->zoomStepFactor : 1.0 / this->zoomStepFactor;
            this->instrumentOverlay.size =
                    std::clamp(this->instrumentOverlay.size * factor, INSTRUMENT_MIN_SIZE, INSTRUMENT_MAX_SIZE);
            update();
            event->accept();
            return;
        }
        if (event->modifiers().testFlag(Qt::ShiftModifier)) {
            this->instrumentOverlay.rotation += event->angleDelta().y() >= 0 ? ROTATION_SNAP_STEP_RADIANS
                                                                              : -ROTATION_SNAP_STEP_RADIANS;
            update();
            event->accept();
            return;
        }
    }
    if (event->modifiers().testFlag(Qt::ControlModifier)) {
        const double factor =
                event->angleDelta().y() >= 0 ? this->zoomStepScrollFactor : 1.0 / this->zoomStepScrollFactor;
        zoomAroundScreenPoint(factor, event->position());
        event->accept();
        return;
    }
    const QPointF delta = event->angleDelta();
    this->scrollX -= delta.x() / (this->zoomFactor * 4.0);
    this->scrollY -= delta.y() / (this->zoomFactor * 4.0);
    emitViewportUpdate();
    event->accept();
}

void QtCanvas::tabletEvent(QTabletEvent* event) {
    this->lastCursorScreenPosition = event->position();
    this->cursorHighlightVisible = true;
    this->inputAdapter->handleTablet(*event);
    if (event->type() == QEvent::TabletPress) {
        this->ignoredStylusEventsRemaining = this->ignoredStylusEvents;
    } else if (event->type() == QEvent::TabletMove && this->ignoredStylusEventsRemaining > 0) {
        --this->ignoredStylusEventsRemaining;
        event->accept();
        return;
    } else if (event->type() == QEvent::TabletRelease) {
        this->ignoredStylusEventsRemaining = 0;
    }
    const auto tool = this->currentToolState.activeTool;
    const auto pointerAction = pointerActionForTabletEvent(*event);
    const bool isDrawTool = tool == QtToolType::Pen || tool == QtToolType::Highlighter ||
                            tool == QtToolType::LaserPointerPen || tool == QtToolType::LaserPointerHighlighter ||
                            tool == QtToolType::ShapeRecognizer;
    const bool isEraserTool = tool == QtToolType::Eraser;
    if (pointerAction == QtPointerButtonAction::Pan) {
        if (event->type() == QEvent::TabletPress && beginPointerAction(pointerAction, event->position(), event->pressure())) {
            event->accept();
            return;
        }
        if (event->type() == QEvent::TabletMove && this->panning) {
            const QPointF delta = event->position() - this->lastPanScreenPosition;
            this->lastPanScreenPosition = event->position();
            this->scrollX -= delta.x() / this->zoomFactor;
            this->scrollY -= delta.y() / this->zoomFactor;
            emitViewportUpdate();
            event->accept();
            return;
        }
        if (event->type() == QEvent::TabletRelease && this->panning) {
            endPan();
            event->accept();
            return;
        }
    }
    if (pointerAction == QtPointerButtonAction::Eraser) {
        if (event->type() == QEvent::TabletPress && beginPointerAction(pointerAction, event->position(), event->pressure())) {
            event->accept();
            return;
        }
        if (event->type() == QEvent::TabletMove && this->erasing) {
            eraseAtScreen(event->position());
            event->accept();
            return;
        }
        if (event->type() == QEvent::TabletRelease && releasePointerAction(pointerAction)) {
            event->accept();
            return;
        }
    }
    if (isDrawTool) {
        if (event->type() == QEvent::TabletPress && event->buttons().testFlag(Qt::LeftButton) && !this->spaceHeld) {
            beginStrokeAtScreen(event->position(), event->pressure());
            event->accept();
            return;
        }
        if (event->type() == QEvent::TabletMove && this->drawing) {
            updateStrokeAtScreen(event->position(), event->pressure());
            event->accept();
            return;
        }
        if (event->type() == QEvent::TabletRelease && this->drawing) {
            finalizeActiveStroke();
            event->accept();
            return;
        }
    }
    if (isEraserTool) {
        if (event->type() == QEvent::TabletPress && event->buttons().testFlag(Qt::LeftButton) && !this->spaceHeld) {
            beginEraseAtScreen(event->position());
            event->accept();
            return;
        }
        if (event->type() == QEvent::TabletMove && this->erasing) {
            eraseAtScreen(event->position());
            event->accept();
            return;
        }
        if (event->type() == QEvent::TabletRelease && this->erasing) {
            finalizeErase();
            event->accept();
            return;
        }
    }
    QWidget::tabletEvent(event);
}

void QtCanvas::keyPressEvent(QKeyEvent* event) {
    this->inputAdapter->handleKeyPress(*event);
    if (!event->isAutoRepeat() && event->key() == Qt::Key_Space) {
        this->spaceHeld = true;
        setCursor(Qt::OpenHandCursor);
    } else if (!event->isAutoRepeat() && event->key() == Qt::Key_Insert && insertVertexOnSelectedEdge()) {
        event->accept();
        return;
    } else if (!event->isAutoRepeat() &&
               (event->key() == Qt::Key_Delete || event->key() == Qt::Key_Backspace) && deleteSelectedGeometry()) {
        event->accept();
        return;
    } else if (!event->isAutoRepeat() && event->key() == Qt::Key_Escape && this->documentController) {
        bool finalizedShape = false;
        if (this->pdfTextSelecting) {
            cancelPdfTextSelection();
        }
        if (this->activeInstrumentStroke || this->movingInstrumentOverlay) {
            cancelInstrumentTool();
        }
        if (this->rubberBanding) {
            cancelRubberBand();
        }
        if (this->movingSelection) {
            cancelMoveSelection();
        }
        if (this->scalingSelection) {
            cancelScaleSelection();
        }
        if (this->geometryTransformInteraction) {
            cancelGeometryTransform();
        }
        if (this->documentController->isVerticalSpacing()) {
            cancelVerticalSpace();
        }
        if (this->shapeDrawing) {
            const auto tool = this->currentToolState.activeTool;
            const bool canFinalizeMultiClickShape =
                    ((tool == QtToolType::DrawPolyline || tool == QtToolType::DrawSpline) &&
                     this->shapeClickPoints.size() >= 2U) ||
                    (tool == QtToolType::DrawArc && this->shapeClickPoints.size() >= 3U);
            if (isMultiClickShapeTool() && canFinalizeMultiClickShape) {
                finalizeShape();
                finalizedShape = true;
            } else {
                cancelShape();
            }
        }
        this->documentController->clearElementSelection();
        this->documentController->clearInteractiveGeometryState();
        updateDebugOverlay(finalizedShape ? QStringLiteral("shape finalized") : QStringLiteral("operation cancelled"));
        if (!this->spaceHeld && !this->panning) {
            refreshToolCursor();
        }
        update();
        Q_EMIT selectionStateChanged();
        event->accept();
        return;
    }
    QWidget::keyPressEvent(event);
}

void QtCanvas::keyReleaseEvent(QKeyEvent* event) {
    this->inputAdapter->handleKeyRelease(*event);
    if (!event->isAutoRepeat() && event->key() == Qt::Key_Space) {
        this->spaceHeld = false;
        if (!this->panning) {
            refreshToolCursor();
        }
    }
    QWidget::keyReleaseEvent(event);
}

bool QtCanvas::event(QEvent* event) {
    if (event && (event->type() == QEvent::TouchBegin || event->type() == QEvent::TouchUpdate ||
                  event->type() == QEvent::TouchEnd)) {
        auto* touchEvent = static_cast<QTouchEvent*>(event);
        if (handleTouchGesture(*touchEvent)) {
            event->accept();
            return true;
        }
        this->activeTouchAction = pointerActionForTouchDevice(touchEvent->device());
        this->inputAdapter->handleTouch(*touchEvent);
        this->activeTouchAction.reset();
    } else if (event && event->type() == QEvent::Leave) {
        clearGeometryHover();
        clearEraserPreview();
        this->cursorHighlightVisible = false;
        update();
    }
    return QWidget::event(event);
}

void QtCanvas::updateDebugOverlay(QString summary) {
    this->lastEventSummary = std::move(summary);
    update();
}

void QtCanvas::emitViewportUpdate(bool edited) {
    constrainScrollToDocumentBounds();
    update();
    Q_EMIT viewportStateChanged();
    Q_EMIT statusHintChanged(QStringLiteral("Zoom %1% | Scroll (%2, %3)")
                                     .arg(this->zoomFactor * 100.0, 0, 'f', 0)
                                     .arg(this->scrollX, 0, 'f', 1)
                                     .arg(this->scrollY, 0, 'f', 1));
    const bool appendedPage = maybeAppendEmptyLastPageOnScroll();
    if (edited || appendedPage) {
        Q_EMIT documentEdited();
    }
}

void QtCanvas::zoomAroundScreenPoint(double factor, const QPointF& screenPoint) {
    this->fitWidthModeEnabled = false;
    const double oldZoom = this->zoomFactor;
    const double newZoom = clampZoom(oldZoom * factor);
    if (newZoom == oldZoom) {
        return;
    }

    const double anchorX = this->scrollX + screenPoint.x() / oldZoom;
    const double anchorY = this->scrollY + screenPoint.y() / oldZoom;
    this->zoomFactor = newZoom;
    this->scrollX = anchorX - screenPoint.x() / newZoom;
    this->scrollY = anchorY - screenPoint.y() / newZoom;
    emitViewportUpdate();
}

auto QtCanvas::pageRects() const -> std::vector<QRectF> {
    const auto pages =
            this->documentController ? this->documentController->snapshotPages()
                                     : std::vector<vn::view::render::PageRenderSnapshot>{};
    return layoutQtCanvasPages(pages, {.span = this->layoutColumnsRowsValue,
                                       .pairOffset = this->pairOffsetValue,
                                       .vertical = this->verticalLayoutEnabled,
                                       .rightToLeft = this->rightToLeftLayoutEnabled,
                                       .bottomToTop = this->bottomToTopLayoutEnabled});
}

auto QtCanvas::documentSceneBounds() const -> QRectF {
    const auto rects = pageRects();
    QRectF bounds = rects.front();
    for (std::size_t i = 1; i < rects.size(); ++i) {
        bounds = bounds.united(rects[i]);
    }
    return bounds.adjusted(-80.0 - this->extraPageSpaceLeft, -80.0 - this->extraPageSpaceAbove,
                           80.0 + this->extraPageSpaceRight, 80.0 + this->extraPageSpaceBelow);
}

void QtCanvas::constrainScrollToDocumentBounds() {
    if (this->unlimitedScrolling || this->zoomFactor <= 0.0) {
        return;
    }

    const auto rects = pageRects();
    if (rects.empty()) {
        return;
    }

    QRectF bounds = rects.front();
    for (std::size_t i = 1; i < rects.size(); ++i) {
        bounds = bounds.united(rects[i]);
    }
    bounds = bounds.adjusted(-80.0 - this->extraPageSpaceLeft, -80.0 - this->extraPageSpaceAbove,
                             80.0 + this->extraPageSpaceRight, 80.0 + this->extraPageSpaceBelow);

    const double visibleWidth = width() / std::max(0.001, this->zoomFactor);
    const double visibleHeight = height() / std::max(0.001, this->zoomFactor);
    const auto clampAxis = [](double value, double minValue, double maxValue, double visibleSize) {
        if (visibleSize >= maxValue - minValue) {
            return minValue - (visibleSize - (maxValue - minValue)) / 2.0;
        }
        return std::clamp(value, minValue, maxValue - visibleSize);
    };

    this->scrollX = clampAxis(this->scrollX, bounds.left(), bounds.right(), visibleWidth);
    this->scrollY = clampAxis(this->scrollY, bounds.top(), bounds.bottom(), visibleHeight);
}

void QtCanvas::drawPageContents(QPainter& painter, const QRectF& rect,
                                const vn::view::render::PageRenderSnapshot& pageInfo, std::size_t pageIndex,
                                bool selected) const {
    if (this->pageContentRenderer) {
        vn::view::render::QtPainterRenderContext renderContext(&painter, this->zoomFactor);
        const vn::view::render::RenderRect renderRect{
                .x = rect.x(),
                .y = rect.y(),
                .width = rect.width(),
                .height = rect.height(),
        };
        this->pageContentRenderer->drawPage(pageInfo, renderRect, renderContext);
        drawGeometryInteractionOverlay(painter, rect, pageInfo, pageIndex);
    }

    // Selected page border (red, 2px) matching GTK style
    if (selected) {
        painter.setPen(QPen(QColor(0xff, 0x00, 0x00), 2.0 / this->zoomFactor));
        painter.setBrush(Qt::NoBrush);
        painter.drawRect(rect.adjusted(-1.0 / this->zoomFactor, -1.0 / this->zoomFactor,
                                        1.0 / this->zoomFactor, 1.0 / this->zoomFactor));
    }
}

void QtCanvas::drawOverlayHud(QPainter& painter) const {
    const auto pages =
            this->documentController ? this->documentController->snapshotPages()
                                     : std::vector<vn::view::render::PageRenderSnapshot>{};
    std::size_t geometryCount = 0;
    std::size_t drawableCount = 0;
    for (const auto& page: pages) {
        drawableCount += page.drawables.size();
        for (const auto& drawable: page.drawables) {
            if (std::holds_alternative<vn::view::render::GeometryRenderModel>(drawable)) {
                ++geometryCount;
            }
        }
    }

    const QStringList badges = {
            QStringLiteral("Qt shell"),
            QStringLiteral("pages %1").arg(static_cast<int>(pages.size())),
            QStringLiteral("drawables %1").arg(static_cast<int>(drawableCount)),
            QStringLiteral("geometry %1").arg(static_cast<int>(geometryCount)),
            QStringLiteral("g-snap %1").arg(this->geometrySnapEnabled ? QStringLiteral("on") : QStringLiteral("off")),
            QStringLiteral("grid %1").arg(this->gridSnapEnabled ? QStringLiteral("on") : QStringLiteral("off")),
    };

    constexpr int badgeHeight = 28;
    constexpr int badgeSpacing = 8;
    int right = width() - 20;
    const int top = 18;
    QFontMetrics metrics(painter.font());
    for (auto it = badges.crbegin(); it != badges.crend(); ++it) {
        const int badgeWidth = metrics.horizontalAdvance(*it) + 22;
        const QRect badgeRect(right - badgeWidth, top, badgeWidth, badgeHeight);
        painter.setPen(Qt::NoPen);
        painter.setBrush(QColor(23, 31, 44, 220));
        painter.drawRoundedRect(badgeRect, 8.0, 8.0);
        painter.setPen(QColor(232, 237, 243));
        painter.drawText(badgeRect.adjusted(10, 0, -10, 0), Qt::AlignCenter, *it);
        right = badgeRect.left() - badgeSpacing;
    }
}

auto QtCanvas::screenToScene(const QPointF& screenPoint) const -> QPointF {
    return QPointF(this->scrollX + screenPoint.x() / this->zoomFactor, this->scrollY + screenPoint.y() / this->zoomFactor);
}

auto QtCanvas::hasEdgePanDrag() const -> bool {
    return this->drawing || this->erasing || this->movingSelection || this->scalingSelection || this->rubberBanding ||
           this->pdfTextSelecting || this->shapeDrawing || this->geometryTransformInteraction ||
           (this->documentController && (this->documentController->isVerticalSpacing() ||
                                         this->documentController->activeGeometryDrag()));
}

auto QtCanvas::edgePanDeltaFor(const QPointF& screenPoint) const -> QPointF {
    if (this->edgePanSpeed <= 0.0 || width() <= 0 || height() <= 0) {
        return {};
    }

    constexpr double edgeMargin = 48.0;
    auto axisDelta = [this, edgeMargin](double position, double length) {
        if (position < 0.0 || position > length) {
            return 0.0;
        }
        if (position < edgeMargin) {
            const double t = (edgeMargin - position) / edgeMargin;
            return -this->edgePanSpeed * std::lerp(1.0, this->edgePanMaxMultiplier, t);
        }
        if (position > length - edgeMargin) {
            const double t = (position - (length - edgeMargin)) / edgeMargin;
            return this->edgePanSpeed * std::lerp(1.0, this->edgePanMaxMultiplier, t);
        }
        return 0.0;
    };

    return QPointF(axisDelta(screenPoint.x(), width()), axisDelta(screenPoint.y(), height()));
}

void QtCanvas::updateGeometryDragAtScreen(const QPointF& screenPoint) {
    if (!this->documentController || !this->documentController->activeGeometryDrag()) {
        return;
    }
    const auto& drag = *this->documentController->activeGeometryDrag();
    const auto rects = pageRects();
    if (drag.pageIndex >= rects.size()) {
        return;
    }

    const QPointF scenePoint = screenToScene(screenPoint);
    const auto& pageRect = rects[drag.pageIndex];
    const double pageX = scenePoint.x() - pageRect.x();
    const double pageY = scenePoint.y() - pageRect.y();
    static_cast<void>(this->documentController->updateGeometryVertexDrag(
            pageX, pageY, this->zoomFactor,
            {.geometryEnabled = this->geometrySnapEnabled,
             .gridEnabled = this->gridSnapEnabled,
             .gridSize = this->snapGridSize,
             .gridTolerance = this->snapGridTolerance,
             .screenTolerance = 22.0}));
    update();
}

void QtCanvas::updateEdgePanAtScreen(const QPointF& screenPoint) {
    this->edgePanScreenPoint = screenPoint;
    if (!hasEdgePanDrag() || edgePanDeltaFor(screenPoint).isNull()) {
        stopEdgePan();
        return;
    }
    if (this->edgePanTimer && !this->edgePanTimer->isActive()) {
        this->edgePanTimer->start();
    }
}

void QtCanvas::stopEdgePan() {
    if (this->edgePanTimer) {
        this->edgePanTimer->stop();
    }
}

void QtCanvas::applyEdgePanStep() {
    if (!hasEdgePanDrag()) {
        stopEdgePan();
        return;
    }
    const QPointF delta = edgePanDeltaFor(this->edgePanScreenPoint);
    if (delta.isNull()) {
        stopEdgePan();
        return;
    }

    this->scrollX += delta.x() / std::max(0.001, this->zoomFactor);
    this->scrollY += delta.y() / std::max(0.001, this->zoomFactor);

    if (this->drawing) {
        updateStrokeAtScreen(this->edgePanScreenPoint, 0.5);
    } else if (this->erasing) {
        eraseAtScreen(this->edgePanScreenPoint);
    } else if (this->movingSelection) {
        updateMoveSelectionAtScreen(this->edgePanScreenPoint);
    } else if (this->scalingSelection) {
        updateScaleSelectionAtScreen(this->edgePanScreenPoint);
    } else if (this->rubberBanding) {
        updateRubberBand(this->edgePanScreenPoint);
    } else if (this->pdfTextSelecting) {
        updatePdfTextSelectionAtScreen(this->edgePanScreenPoint);
    } else if (this->documentController && this->documentController->isVerticalSpacing()) {
        updateVerticalSpaceAtScreen(this->edgePanScreenPoint);
    } else if (this->shapeDrawing) {
        updateShapeAtScreen(this->edgePanScreenPoint);
    } else if (this->documentController && this->documentController->activeGeometryDrag()) {
        updateGeometryDragAtScreen(this->edgePanScreenPoint);
    }

    emitViewportUpdate();
}

auto QtCanvas::pageIndexAtScenePoint(const QPointF& scenePoint) const -> std::optional<std::size_t> {
    const auto rects = pageRects();
    for (std::size_t index = 0; index < rects.size(); ++index) {
        if (rects[index].contains(scenePoint)) {
            return index;
        }
    }
    return std::nullopt;
}

void QtCanvas::updateGeometryHover(const QPointF& screenPoint) {
    if (!this->documentController) {
        return;
    }
    if (this->currentToolState.activeTool == QtToolType::Eraser || this->temporaryRightButtonEraser) {
        this->documentController->setHoveredGeometry(std::nullopt);
        if (!this->spaceHeld && !this->panning) {
            setCursor(Qt::BlankCursor);
        }
        update();
        return;
    }

    const QPointF scenePoint = screenToScene(screenPoint);
    const auto pageIndex = pageIndexAtScenePoint(scenePoint);
    if (!pageIndex) {
        clearGeometryHover();
        if (!this->spaceHeld) {
            unsetCursor();
        }
        Q_EMIT statusHintChanged(QString());
        return;
    }

    const auto rects = pageRects();
    const auto& pageRect = rects[*pageIndex];
    const double pageX = scenePoint.x() - pageRect.x();
    const double pageY = scenePoint.y() - pageRect.y();
    const bool vertexMode = this->currentToolState.geometrySelectionMode == QtGeometrySelectionMode::Vertex;
    const bool edgeMode = this->currentToolState.geometrySelectionMode == QtGeometrySelectionMode::Edge;
    const bool faceMode = this->currentToolState.geometrySelectionMode == QtGeometrySelectionMode::Face;
    const bool objectMode = this->currentToolState.geometrySelectionMode == QtGeometrySelectionMode::Object;
    auto hit = this->documentController->hitTestGeometry(*pageIndex, pageX, pageY, this->zoomFactor,
                                                         GEOMETRY_HIT_RADIUS_PIXELS, vertexMode || objectMode,
                                                         edgeMode || objectMode, faceMode || objectMode);
    this->documentController->setHoveredGeometry(hit);
    if (hit) {
        if (!this->spaceHeld && !this->panning) {
            setCursor(vertexMode ? Qt::CrossCursor : Qt::PointingHandCursor);
        }
        const QString hitHint = geometryHitTypeHint(hit->hit.type);
        Q_EMIT statusHintChanged(hit->hit.snapKind
                                         ? QStringLiteral("Snap: %1 | Hover: %2").arg(snapHint(hit->hit.snapKind),
                                                                                      hitHint)
                                         : QStringLiteral("Hover: %1").arg(hitHint));
        updateDebugOverlay(QStringLiteral("%1 hover page=%2 object=%3")
                                   .arg(geometrySelectionModeName(this->currentToolState.geometrySelectionMode))
                                   .arg(static_cast<int>(hit->pageIndex + 1))
                                   .arg(static_cast<qulonglong>(hit->hit.objectId)));
    } else if (!this->spaceHeld && !this->panning) {
        unsetCursor();
        Q_EMIT statusHintChanged(QString());
    }
    update();
}

void QtCanvas::clearGeometryHover() {
    if (!this->documentController) {
        return;
    }
    this->documentController->setHoveredGeometry(std::nullopt);
    this->hoveredGeometryTransformHandle = GeometryTransformHandle::None;
    Q_EMIT statusHintChanged(QString());
    update();
}

void QtCanvas::selectHoveredGeometry(bool additive) {
    if (!this->documentController) {
        return;
    }

    if (this->currentToolState.geometrySelectionMode == QtGeometrySelectionMode::Object) {
        this->documentController->setSelectedGeometryObject(this->documentController->hoveredGeometry());
    } else {
        this->documentController->setSelectedGeometry(this->documentController->hoveredGeometry(), additive);
    }
    if (!this->documentController->selectedGeometry()) {
        updateDebugOverlay(QStringLiteral("%1 selection cleared")
                                   .arg(geometrySelectionModeName(this->currentToolState.geometrySelectionMode)));
    } else {
        const auto& hit = *this->documentController->selectedGeometry();
        updateDebugOverlay(QStringLiteral("%1 selected page=%2 object=%3 vertices=%4 edges=%5 faces=%6")
                                   .arg(geometrySelectionModeName(this->currentToolState.geometrySelectionMode))
                                   .arg(static_cast<int>(hit.pageIndex + 1))
                                   .arg(static_cast<qulonglong>(hit.hit.objectId))
                                   .arg(static_cast<int>(this->documentController->selectedVertexIds().size()))
                                   .arg(static_cast<int>(this->documentController->selectedEdgeIds().size()))
                                   .arg(static_cast<int>(this->documentController->selectedFaceIds().size())));
    }
    update();
    Q_EMIT selectionStateChanged();
}

auto QtCanvas::beginSelectedGeometryMoveAtScreen(const QPointF& screenPoint) -> bool {
    if (!this->documentController || !this->documentController->selectedGeometry()) {
        return false;
    }

    if (this->currentToolState.geometrySelectionMode == QtGeometrySelectionMode::Vertex &&
        this->documentController->selectedGeometry()->hit.type == vn::view::render::GeometryHitType::Vertex) {
        if (this->documentController->beginGeometryVertexDrag(*this->documentController->selectedGeometry())) {
            setCursor(Qt::ClosedHandCursor);
            return true;
        }
        return false;
    }

    if (beginGeometryTransformAtScreen(GeometryTransformHandle::TranslateXY, screenPoint)) {
        return true;
    }
    return false;
}

auto QtCanvas::selectedGeometrySceneBounds() const -> std::optional<GeometrySelectionSceneBounds> {
    if (!this->documentController || !this->documentController->selectedGeometry()) {
        return std::nullopt;
    }

    const auto& selected = *this->documentController->selectedGeometry();
    const auto rects = pageRects();
    const auto pages = this->documentController->snapshotPages();
    if (selected.pageIndex >= rects.size() || selected.pageIndex >= pages.size()) {
        return std::nullopt;
    }

    const auto& selectedVertexIds = this->documentController->selectedVertexIds();
    const auto& selectedEdgeIds = this->documentController->selectedEdgeIds();
    const auto& selectedFaceIds = this->documentController->selectedFaceIds();
    bool hasVertex = false;
    double minX = std::numeric_limits<double>::max();
    double minY = std::numeric_limits<double>::max();
    double maxX = std::numeric_limits<double>::lowest();
    double maxY = std::numeric_limits<double>::lowest();
    const auto includePoint = [&](const Point& point) {
        minX = std::min(minX, point.x);
        minY = std::min(minY, point.y);
        maxX = std::max(maxX, point.x);
        maxY = std::max(maxY, point.y);
        hasVertex = true;
    };

    for (const auto& drawable: pages[selected.pageIndex].drawables) {
        const auto* geometry = std::get_if<vn::view::render::GeometryRenderModel>(&drawable);
        if (!geometry || geometry->objectId != selected.hit.objectId) {
            continue;
        }
        if (!selectedVertexIds.empty()) {
            for (const auto& vertex: geometry->vertices) {
                if (std::find(selectedVertexIds.begin(), selectedVertexIds.end(), vertex.id) !=
                    selectedVertexIds.end()) {
                    includePoint(vertex.position);
                }
            }
        } else if (!selectedEdgeIds.empty()) {
            for (const auto& edge: geometry->edges) {
                if (std::find(selectedEdgeIds.begin(), selectedEdgeIds.end(), edge.id) == selectedEdgeIds.end()) {
                    continue;
                }
                includePoint(edge.start);
                includePoint(edge.end);
                for (const auto& control: edge.controls) {
                    includePoint(control);
                }
            }
        } else if (!selectedFaceIds.empty()) {
            for (const auto& face: geometry->faces) {
                if (std::find(selectedFaceIds.begin(), selectedFaceIds.end(), face.id) == selectedFaceIds.end()) {
                    continue;
                }
                for (const auto& point: face.vertices) {
                    includePoint(point);
                }
            }
        } else {
            for (const auto& vertex: geometry->vertices) {
                includePoint(vertex.position);
            }
        }
        break;
    }

    if (!hasVertex) {
        return std::nullopt;
    }

    const QRectF& pageRect = rects[selected.pageIndex];
    QRectF bounds(pageRect.x() + minX, pageRect.y() + minY, std::max(0.0, maxX - minX), std::max(0.0, maxY - minY));
    const double minVisualSize = 18.0 / std::max(this->zoomFactor, 0.001);
    if (bounds.width() < minVisualSize) {
        const double delta = minVisualSize - bounds.width();
        bounds.adjust(-delta / 2.0, 0.0, delta / 2.0, 0.0);
    }
    if (bounds.height() < minVisualSize) {
        const double delta = minVisualSize - bounds.height();
        bounds.adjust(0.0, -delta / 2.0, 0.0, delta / 2.0);
    }

    return GeometrySelectionSceneBounds{.pageIndex = selected.pageIndex, .bounds = bounds, .center = bounds.center()};
}

auto QtCanvas::geometryTransformSupportsAdvancedHandles() const -> bool {
    if (!this->documentController || !this->documentController->selectedGeometry()) {
        return false;
    }

    if (this->currentToolState.geometrySelectionMode == QtGeometrySelectionMode::Object ||
        !this->documentController->selectedEdgeIds().empty() ||
        !this->documentController->selectedFaceIds().empty()) {
        return true;
    }

    if (this->documentController->selectedVertexIds().size() > 1U) {
        return true;
    }

    const auto& selected = *this->documentController->selectedGeometry();
    return selected.hit.type == vn::view::render::GeometryHitType::Edge ||
           selected.hit.type == vn::view::render::GeometryHitType::Face;
}

auto QtCanvas::geometryTransformHandleAtScreen(const QPointF& screenPoint) const -> GeometryTransformHandle {
    const auto bounds = selectedGeometrySceneBounds();
    if (!bounds) {
        return GeometryTransformHandle::None;
    }

    const QPointF scenePoint = screenToScene(screenPoint);
    const double zoomScale = std::max(this->zoomFactor, 0.001);
    const double hitRadius = 16.0 / zoomScale;
    const double axisLength = 78.0 / zoomScale;
    const double rotationRadius = 62.0 / zoomScale;
    const QPointF xEnd(bounds->center.x() + axisLength, bounds->center.y());
    const QPointF yEnd(bounds->center.x(), bounds->center.y() - axisLength);
    const QPointF scaleHandle = bounds->bounds.bottomRight() + QPointF(18.0 / zoomScale, 18.0 / zoomScale);
    const QPointF zHandle(bounds->center.x() + rotationRadius * std::cos(-M_PI / 4.0),
                          bounds->center.y() + rotationRadius * std::sin(-M_PI / 4.0));

    const auto withinCircle = [](const QPointF& point, const QPointF& center, double radius) {
        const double dx = point.x() - center.x();
        const double dy = point.y() - center.y();
        return dx * dx + dy * dy <= radius * radius;
    };
    const auto distanceToSegment = [](const QPointF& point, const QPointF& a, const QPointF& b) {
        const QPointF ab = b - a;
        const double len2 = QPointF::dotProduct(ab, ab);
        if (len2 <= 1e-9) {
            const QPointF delta = point - a;
            return std::sqrt(QPointF::dotProduct(delta, delta));
        }
        const double t = std::clamp(QPointF::dotProduct(point - a, ab) / len2, 0.0, 1.0);
        const QPointF projection = a + ab * t;
        const QPointF delta = point - projection;
        return std::sqrt(QPointF::dotProduct(delta, delta));
    };

    const QPointF radial = scenePoint - bounds->center;
    const double radialDistance = std::sqrt(QPointF::dotProduct(radial, radial));
    const bool advancedHandles = geometryTransformSupportsAdvancedHandles();
    if (advancedHandles && withinCircle(scenePoint, scaleHandle, hitRadius * 1.55)) {
        return GeometryTransformHandle::ScaleUniform;
    }
    if (withinCircle(scenePoint, bounds->center, 18.0 / zoomScale)) {
        return GeometryTransformHandle::TranslateXY;
    }
    if (withinCircle(scenePoint, xEnd, hitRadius * 1.35) ||
        distanceToSegment(scenePoint, bounds->center, xEnd) <= hitRadius * 1.05) {
        return GeometryTransformHandle::TranslateX;
    }
    if (withinCircle(scenePoint, yEnd, hitRadius * 1.35) ||
        distanceToSegment(scenePoint, bounds->center, yEnd) <= hitRadius * 1.05) {
        return GeometryTransformHandle::TranslateY;
    }
    if (advancedHandles && (withinCircle(scenePoint, zHandle, hitRadius * 1.55) ||
                            std::abs(radialDistance - rotationRadius) <= hitRadius * 1.1)) {
        return GeometryTransformHandle::RotateZ;
    }
    return GeometryTransformHandle::None;
}

void QtCanvas::setGeometryTransformCursor(GeometryTransformHandle handle) {
    switch (handle) {
        case GeometryTransformHandle::TranslateXY:
            setCursor(Qt::SizeAllCursor);
            break;
        case GeometryTransformHandle::TranslateX:
            setCursor(Qt::SizeHorCursor);
            break;
        case GeometryTransformHandle::TranslateY:
            setCursor(Qt::SizeVerCursor);
            break;
        case GeometryTransformHandle::RotateZ:
            setCursor(Qt::SizeBDiagCursor);
            break;
        case GeometryTransformHandle::ScaleUniform:
            setCursor(Qt::SizeFDiagCursor);
            break;
        case GeometryTransformHandle::None:
            refreshToolCursor();
            break;
    }
}

auto QtCanvas::beginGeometryTransformAtScreen(GeometryTransformHandle handle, const QPointF& screenPoint) -> bool {
    if (!this->documentController || handle == GeometryTransformHandle::None) {
        return false;
    }

    const auto bounds = selectedGeometrySceneBounds();
    if (!bounds || !this->documentController->beginSelectedGeometryTransform()) {
        return false;
    }

    const QPointF scenePoint = screenToScene(screenPoint);
    const auto rects = pageRects();
    if (bounds->pageIndex >= rects.size()) {
        this->documentController->cancelSelectedGeometryTransform();
        return false;
    }

    const QPointF pagePoint(scenePoint.x() - rects[bounds->pageIndex].x(), scenePoint.y() - rects[bounds->pageIndex].y());
    const QPointF centerPagePoint(bounds->center.x() - rects[bounds->pageIndex].x(),
                                  bounds->center.y() - rects[bounds->pageIndex].y());
    this->geometryTransformInteraction = GeometryTransformInteraction{
            .handle = handle,
            .pageIndex = bounds->pageIndex,
            .startPagePoint = pagePoint,
            .centerPagePoint = centerPagePoint,
            .startAngle = std::atan2(pagePoint.y() - centerPagePoint.y(), pagePoint.x() - centerPagePoint.x()),
            .startRadius = std::max(1e-6, std::hypot(pagePoint.x() - centerPagePoint.x(),
                                                     pagePoint.y() - centerPagePoint.y())),
    };
    this->hoveredGeometryTransformHandle = handle;
    setGeometryTransformCursor(handle);
    return true;
}

void QtCanvas::updateGeometryTransformAtScreen(const QPointF& screenPoint) {
    if (!this->documentController || !this->geometryTransformInteraction) {
        return;
    }

    const auto rects = pageRects();
    auto& interaction = *this->geometryTransformInteraction;
    if (interaction.pageIndex >= rects.size()) {
        return;
    }

    const QPointF scenePoint = screenToScene(screenPoint);
    const QPointF pagePoint(scenePoint.x() - rects[interaction.pageIndex].x(),
                            scenePoint.y() - rects[interaction.pageIndex].y());

    double dx = 0.0;
    double dy = 0.0;
    double degrees = 0.0;
    double scale = 1.0;
    if (interaction.handle == GeometryTransformHandle::TranslateXY ||
        interaction.handle == GeometryTransformHandle::TranslateX ||
        interaction.handle == GeometryTransformHandle::TranslateY) {
        dx = pagePoint.x() - interaction.startPagePoint.x();
        dy = pagePoint.y() - interaction.startPagePoint.y();
        if (interaction.handle == GeometryTransformHandle::TranslateX) {
            dy = 0.0;
        } else if (interaction.handle == GeometryTransformHandle::TranslateY) {
            dx = 0.0;
        }
        if (this->gridSnapEnabled) {
            dx = std::round(dx / std::max(this->snapGridSize, 0.001)) * this->snapGridSize;
            dy = std::round(dy / std::max(this->snapGridSize, 0.001)) * this->snapGridSize;
        }
    } else if (interaction.handle == GeometryTransformHandle::RotateZ) {
        const double angle = std::atan2(pagePoint.y() - interaction.centerPagePoint.y(),
                                        pagePoint.x() - interaction.centerPagePoint.x());
        double radians = angle - interaction.startAngle;
        if (this->rotationSnapEnabled && this->rotationSnapTolerance > 0.0) {
            radians = std::round(radians / ROTATION_SNAP_STEP_RADIANS) * ROTATION_SNAP_STEP_RADIANS;
        }
        degrees = radians * 180.0 / M_PI;
    } else if (interaction.handle == GeometryTransformHandle::ScaleUniform) {
        const double radius = std::hypot(pagePoint.x() - interaction.centerPagePoint.x(),
                                         pagePoint.y() - interaction.centerPagePoint.y());
        scale = std::clamp(radius / interaction.startRadius, 0.05, 20.0);
    }

    static_cast<void>(this->documentController->updateSelectedGeometryTransform(dx, dy, degrees, scale, scale));
    update();
}

void QtCanvas::finalizeGeometryTransform() {
    if (!this->documentController) {
        this->geometryTransformInteraction.reset();
        return;
    }

    const bool changed = this->documentController->endSelectedGeometryTransform();
    this->geometryTransformInteraction.reset();
    this->hoveredGeometryTransformHandle = GeometryTransformHandle::None;
    setCursor(Qt::ArrowCursor);
    update();
    if (changed) {
        Q_EMIT documentEdited();
    }
}

void QtCanvas::cancelGeometryTransform() {
    if (this->documentController) {
        this->documentController->cancelSelectedGeometryTransform();
    }
    this->geometryTransformInteraction.reset();
    this->hoveredGeometryTransformHandle = GeometryTransformHandle::None;
    setCursor(Qt::ArrowCursor);
    update();
}

void QtCanvas::drawGeometryTransformGizmo(QPainter& painter) const {
    const auto bounds = selectedGeometrySceneBounds();
    if (!bounds) {
        return;
    }

    painter.save();
    const double zoomScale = std::max(this->zoomFactor, 0.001);
    const QColor xColor(225, 62, 62);
    const QColor yColor(45, 175, 78);
    const QColor zColor(58, 126, 245);
    const QColor orange(245, 130, 32);
    const QColor scaleColor(132, 85, 214);
    const double axisLength = 78.0 / zoomScale;
    const double arrowSize = 9.5 / zoomScale;
    const double rotationRadius = 62.0 / zoomScale;
    const QPointF center = bounds->center;
    const QPointF xEnd(center.x() + axisLength, center.y());
    const QPointF yEnd(center.x(), center.y() - axisLength);
    const bool advancedHandles = geometryTransformSupportsAdvancedHandles();
    const auto activeHandle = this->geometryTransformInteraction ? this->geometryTransformInteraction->handle
                                                                 : this->hoveredGeometryTransformHandle;
    const auto isActive = [activeHandle](GeometryTransformHandle handle) {
        return activeHandle == handle;
    };
    const double scaleHandleSize = (isActive(GeometryTransformHandle::ScaleUniform) ? 17.0 : 14.0) / zoomScale;
    const QPointF scaleHandle = bounds->bounds.bottomRight() + QPointF(18.0 / zoomScale, 18.0 / zoomScale);

    QPen boundsPen(QColor(58, 126, 245, 90), 1.0 / zoomScale);
    boundsPen.setStyle(Qt::DashLine);
    painter.setPen(boundsPen);
    painter.setBrush(Qt::NoBrush);
    painter.drawRect(bounds->bounds.adjusted(-4.0 / zoomScale, -4.0 / zoomScale, 4.0 / zoomScale, 4.0 / zoomScale));

    if (advancedHandles) {
        QPen ringPen(zColor, (isActive(GeometryTransformHandle::RotateZ) ? 3.0 : 1.7) / zoomScale);
        ringPen.setCosmetic(false);
        painter.setPen(ringPen);
        painter.setBrush(isActive(GeometryTransformHandle::RotateZ) ? QBrush(QColor(58, 126, 245, 24)) : Qt::NoBrush);
        painter.drawEllipse(center, rotationRadius, rotationRadius);
    }

    auto drawAxis = [&](const QPointF& end, const QColor& color, bool horizontal) {
        const auto handle = horizontal ? GeometryTransformHandle::TranslateX : GeometryTransformHandle::TranslateY;
        QPen haloPen(QColor(color.red(), color.green(), color.blue(), 46),
                     (isActive(handle) ? 15.0 : 10.5) / zoomScale);
        haloPen.setCapStyle(Qt::RoundCap);
        painter.setPen(haloPen);
        painter.drawLine(center, end);

        QPen pen(color, (isActive(handle) ? 5.0 : 3.4) / zoomScale);
        pen.setCapStyle(Qt::RoundCap);
        painter.setPen(pen);
        painter.drawLine(center, end);
        QPainterPath arrow;
        if (horizontal) {
            arrow.moveTo(end);
            arrow.lineTo(QPointF(end.x() - arrowSize, end.y() - arrowSize * 0.65));
            arrow.lineTo(QPointF(end.x() - arrowSize, end.y() + arrowSize * 0.65));
        } else {
            arrow.moveTo(end);
            arrow.lineTo(QPointF(end.x() - arrowSize * 0.65, end.y() + arrowSize));
            arrow.lineTo(QPointF(end.x() + arrowSize * 0.65, end.y() + arrowSize));
        }
        arrow.closeSubpath();
        painter.setPen(Qt::NoPen);
        painter.setBrush(color);
        painter.drawPath(arrow);

        painter.setBrush(QColor(255, 255, 255, 245));
        painter.setPen(QPen(color, (isActive(handle) ? 2.2 : 1.5) / zoomScale));
        painter.drawEllipse(end, (isActive(handle) ? 7.0 : 5.5) / zoomScale, (isActive(handle) ? 7.0 : 5.5) / zoomScale);
    };

    drawAxis(xEnd, xColor, true);
    drawAxis(yEnd, yColor, false);

    const double centerHandleSize = (isActive(GeometryTransformHandle::TranslateXY) ? 21.0 : 17.0) / zoomScale;
    painter.setPen(QPen(orange, (isActive(GeometryTransformHandle::TranslateXY) ? 2.2 : 1.5) / zoomScale));
    painter.setBrush(isActive(GeometryTransformHandle::TranslateXY) ? QColor(255, 184, 92, 245)
                                                                    : QColor(255, 255, 255, 245));
    painter.drawRoundedRect(QRectF(center.x() - centerHandleSize / 2.0, center.y() - centerHandleSize / 2.0,
                                   centerHandleSize, centerHandleSize),
                            3.0 / zoomScale, 3.0 / zoomScale);
    painter.setPen(QPen(orange, 1.6 / zoomScale));
    painter.drawLine(QPointF(center.x() - 5.0 / zoomScale, center.y()), QPointF(center.x() + 5.0 / zoomScale, center.y()));
    painter.drawLine(QPointF(center.x(), center.y() - 5.0 / zoomScale), QPointF(center.x(), center.y() + 5.0 / zoomScale));

    const QPointF zHandle(center.x() + rotationRadius * std::cos(-M_PI / 4.0),
                          center.y() + rotationRadius * std::sin(-M_PI / 4.0));
    if (advancedHandles) {
        painter.setPen(QPen(zColor, (isActive(GeometryTransformHandle::RotateZ) ? 2.2 : 1.5) / zoomScale));
        painter.setBrush(isActive(GeometryTransformHandle::RotateZ) ? QColor(198, 218, 255, 245)
                                                                    : QColor(255, 255, 255, 245));
        painter.drawEllipse(zHandle, (isActive(GeometryTransformHandle::RotateZ) ? 10.5 : 8.5) / zoomScale,
                            (isActive(GeometryTransformHandle::RotateZ) ? 10.5 : 8.5) / zoomScale);
        painter.drawArc(QRectF(zHandle.x() - 5.2 / zoomScale, zHandle.y() - 5.2 / zoomScale, 10.4 / zoomScale,
                               10.4 / zoomScale),
                        30 * 16, 250 * 16);

        painter.setPen(QPen(scaleColor, (isActive(GeometryTransformHandle::ScaleUniform) ? 2.2 : 1.5) / zoomScale));
        painter.setBrush(isActive(GeometryTransformHandle::ScaleUniform) ? QColor(220, 205, 255, 245)
                                                                         : QColor(255, 255, 255, 245));
        painter.drawRect(QRectF(scaleHandle.x() - scaleHandleSize / 2.0, scaleHandle.y() - scaleHandleSize / 2.0,
                                scaleHandleSize, scaleHandleSize));
        painter.drawLine(QPointF(scaleHandle.x() - 3.2 / zoomScale, scaleHandle.y() + 2.7 / zoomScale),
                         QPointF(scaleHandle.x() + 2.7 / zoomScale, scaleHandle.y() - 3.2 / zoomScale));
        painter.setPen(QPen(QColor(scaleColor.red(), scaleColor.green(), scaleColor.blue(), 150), 1.0 / zoomScale));
        painter.drawLine(center, scaleHandle);
    }

    painter.restore();
}

void QtCanvas::drawGeometryInteractionOverlay(QPainter& painter, const QRectF& rect,
                                              const vn::view::render::PageRenderSnapshot& pageInfo,
                                              std::size_t pageIndex) const {
    if (!this->documentController || !this->geometryRenderer) {
        return;
    }

    vn::view::render::QtPainterRenderContext renderContext(&painter, this->zoomFactor);
    const auto& hovered = this->documentController->hoveredGeometry();
    const auto& selected = this->documentController->selectedGeometry();
    const auto& selectedVertexIds = this->documentController->selectedVertexIds();
    const auto& selectedEdgeIds = this->documentController->selectedEdgeIds();
    const auto& selectedFaceIds = this->documentController->selectedFaceIds();
    const auto& drag = this->documentController->activeGeometryDrag();
    const auto selectionMode = this->currentToolState.geometrySelectionMode;
    const QColor modeAccent = geometrySelectionModeAccent(selectionMode);
    const QColor selectedEdgeColor = geometrySelectionModeAccent(QtGeometrySelectionMode::Edge);
    const QColor objectColor = geometrySelectionModeAccent(QtGeometrySelectionMode::Object);
    const QColor hoverEdgeColor(255, 140, 20);
    const QColor hoverFaceColor(18, 154, 112);
    const QColor hoverVertexColor(0, 115, 255);

    const auto drawGeometryEdgesOverlay = [&](vn::geom::ObjectId objectId, const std::vector<vn::geom::EdgeId>* edgeIds,
                                              const QColor& color, double extraWidth) {
        if (edgeIds && edgeIds->empty()) {
            return;
        }

        for (const auto& drawable: pageInfo.drawables) {
            const auto* geometry = std::get_if<vn::view::render::GeometryRenderModel>(&drawable);
            if (!geometry || geometry->objectId != objectId) {
                continue;
            }

            std::vector<vn::view::render::GeometryEdgeRenderModel> overlayEdges;
            if (edgeIds) {
                for (const auto& edge: geometry->edges) {
                    if (std::find(edgeIds->begin(), edgeIds->end(), edge.id) != edgeIds->end()) {
                        overlayEdges.push_back(edge);
                    }
                }
            } else {
                overlayEdges = geometry->edges;
            }
            if (overlayEdges.empty()) {
                break;
            }

            vn::view::render::GeometryRenderModel overlay;
            overlay.objectId = geometry->objectId;
            overlay.vertices = geometry->vertices;
            overlay.edges = std::move(overlayEdges);
            overlay.color = Color(static_cast<uint8_t>(color.red()), static_cast<uint8_t>(color.green()),
                                  static_cast<uint8_t>(color.blue()));
            overlay.strokeWidth = geometry->strokeWidth + extraWidth;
            painter.save();
            painter.translate(rect.x(), rect.y());
            this->geometryRenderer->draw(overlay, renderContext);
            painter.restore();
            break;
        }
    };

    const auto drawSingleEdgeOverlay = [&](const QtGeometryHit& geometryHit, const QColor& color, double extraWidth) {
        if (geometryHit.pageIndex != pageIndex || geometryHit.hit.type != vn::view::render::GeometryHitType::Edge ||
            geometryHit.hit.edgeId == vn::geom::InvalidEdgeId) {
            return;
        }
        const std::vector<vn::geom::EdgeId> edgeIds{geometryHit.hit.edgeId};
        drawGeometryEdgesOverlay(geometryHit.hit.objectId, &edgeIds, color, extraWidth);
    };

    const auto drawGeometryFacesOverlay = [&](vn::geom::ObjectId objectId, const std::vector<vn::geom::FaceId>* faceIds,
                                              const QColor& color) {
        if (faceIds && faceIds->empty()) {
            return;
        }
        for (const auto& drawable: pageInfo.drawables) {
            const auto* geometry = std::get_if<vn::view::render::GeometryRenderModel>(&drawable);
            if (!geometry || geometry->objectId != objectId) {
                continue;
            }
            painter.save();
            painter.translate(rect.x(), rect.y());
            painter.setPen(QPen(color, 1.6 / std::max(this->zoomFactor, 0.001)));
            painter.setBrush(QColor(color.red(), color.green(), color.blue(), 54));
            for (const auto& face: geometry->faces) {
                if (faceIds && std::find(faceIds->begin(), faceIds->end(), face.id) == faceIds->end()) {
                    continue;
                }
                if (face.vertices.size() < 3U) {
                    continue;
                }
                QPainterPath path;
                path.moveTo(face.vertices.front().x, face.vertices.front().y);
                for (std::size_t index = 1U; index < face.vertices.size(); ++index) {
                    path.lineTo(face.vertices[index].x, face.vertices[index].y);
                }
                path.closeSubpath();
                painter.drawPath(path);
            }
            painter.restore();
            break;
        }
    };

    const auto drawGeometryLoopPreview = [&](vn::geom::ObjectId objectId, const std::vector<vn::geom::VertexId>& loop,
                                             const QColor& color) {
        if (loop.size() < 3U) {
            return;
        }
        for (const auto& drawable: pageInfo.drawables) {
            const auto* geometry = std::get_if<vn::view::render::GeometryRenderModel>(&drawable);
            if (!geometry || geometry->objectId != objectId) {
                continue;
            }

            QPainterPath path;
            bool valid = true;
            for (std::size_t index = 0U; index < loop.size(); ++index) {
                const auto vertexIt = std::find_if(geometry->vertices.begin(), geometry->vertices.end(),
                                                   [&](const auto& vertex) { return vertex.id == loop[index]; });
                if (vertexIt == geometry->vertices.end()) {
                    valid = false;
                    break;
                }
                const QPointF point(rect.x() + vertexIt->position.x, rect.y() + vertexIt->position.y);
                if (index == 0U) {
                    path.moveTo(point);
                } else {
                    path.lineTo(point);
                }
            }
            if (!valid) {
                break;
            }
            path.closeSubpath();

            painter.save();
            QPen previewPen(color, 1.6 / std::max(this->zoomFactor, 0.001));
            previewPen.setStyle(Qt::DashLine);
            painter.setPen(previewPen);
            painter.setBrush(QColor(color.red(), color.green(), color.blue(), 34));
            painter.drawPath(path);
            painter.restore();
            break;
        }
    };

    const auto drawGeometryDiagonalPreview = [&](vn::geom::ObjectId objectId,
                                                 const QtGeometryFaceDiagonal& diagonal, const QColor& color) {
        for (const auto& drawable: pageInfo.drawables) {
            const auto* geometry = std::get_if<vn::view::render::GeometryRenderModel>(&drawable);
            if (!geometry || geometry->objectId != objectId) {
                continue;
            }

            const auto findVertex = [&](vn::geom::VertexId vertexId) {
                return std::find_if(geometry->vertices.begin(), geometry->vertices.end(),
                                    [&](const auto& vertex) { return vertex.id == vertexId; });
            };
            const auto lhs = findVertex(diagonal.lhs);
            const auto rhs = findVertex(diagonal.rhs);
            if (lhs == geometry->vertices.end() || rhs == geometry->vertices.end()) {
                break;
            }

            const double zoomScale = std::max(this->zoomFactor, 0.001);
            const QPointF lhsPoint(rect.x() + lhs->position.x, rect.y() + lhs->position.y);
            const QPointF rhsPoint(rect.x() + rhs->position.x, rect.y() + rhs->position.y);
            QPen previewPen(color, 2.2 / zoomScale);
            previewPen.setStyle(Qt::DashLine);

            painter.save();
            painter.setRenderHint(QPainter::Antialiasing, true);
            painter.setPen(previewPen);
            painter.drawLine(lhsPoint, rhsPoint);
            painter.setPen(QPen(color, 1.8 / zoomScale));
            painter.setBrush(QColor(255, 255, 255, 235));
            const double handleSize = 8.0 / zoomScale;
            painter.drawRect(QRectF(lhsPoint.x() - handleSize / 2.0, lhsPoint.y() - handleSize / 2.0,
                                    handleSize, handleSize));
            painter.drawRect(QRectF(rhsPoint.x() - handleSize / 2.0, rhsPoint.y() - handleSize / 2.0,
                                    handleSize, handleSize));
            painter.restore();
            break;
        }
    };

    if (selected && selected->pageIndex == pageIndex) {
        const auto loopStatus = this->documentController->selectedGeometryFaceLoopStatus();
        if (loopStatus.kind == QtGeometryFaceLoopStatusKind::Ready) {
            drawGeometryLoopPreview(selected->hit.objectId, loopStatus.loop, QColor(18, 154, 112));
        }
        if (selectionMode == QtGeometrySelectionMode::Object) {
            drawGeometryFacesOverlay(selected->hit.objectId, nullptr, objectColor);
            drawGeometryEdgesOverlay(selected->hit.objectId, nullptr, objectColor, 2.4);
        } else if (!selectedFaceIds.empty()) {
            drawGeometryFacesOverlay(selected->hit.objectId, &selectedFaceIds, modeAccent);
        } else if (!selectedEdgeIds.empty()) {
            drawGeometryEdgesOverlay(selected->hit.objectId, &selectedEdgeIds, selectedEdgeColor, 2.4);
        } else {
            drawSingleEdgeOverlay(*selected, selectedEdgeColor, 2.2);
        }
        if (this->geometryFaceSplitPreview) {
            drawGeometryDiagonalPreview(selected->hit.objectId, *this->geometryFaceSplitPreview,
                                        QColor(255, 140, 20));
        }
    }
    if (hovered) {
        if (hovered->hit.type == vn::view::render::GeometryHitType::Face &&
            hovered->hit.faceId != vn::geom::InvalidFaceId) {
            const std::vector<vn::geom::FaceId> faceIds{hovered->hit.faceId};
            drawGeometryFacesOverlay(hovered->hit.objectId, &faceIds, hoverFaceColor);
        }
        drawSingleEdgeOverlay(*hovered, hoverEdgeColor, 1.0);
    }

    std::optional<vn::geom::ObjectId> focusObject;
    if (selected && selected->pageIndex == pageIndex) {
        focusObject = selected->hit.objectId;
    } else if (hovered && hovered->pageIndex == pageIndex) {
        focusObject = hovered->hit.objectId;
    }
    const bool drawGlobalVertexOverlay = this->geometryVertexOverlayEnabled;
    if (!focusObject && !drawGlobalVertexOverlay) {
        return;
    }

    const auto hasCoincidentPeer = [&](const vn::view::render::GeometryRenderModel& owner,
                                       const vn::view::render::GeometryVertexRenderModel& vertex) {
        for (const auto& drawable: pageInfo.drawables) {
            const auto* geometry = std::get_if<vn::view::render::GeometryRenderModel>(&drawable);
            if (!geometry) {
                continue;
            }
            for (const auto& candidate: geometry->vertices) {
                if (geometry->objectId == owner.objectId && candidate.id == vertex.id) {
                    continue;
                }
                if (coincidentRenderPoint(candidate.position, vertex.position)) {
                    return true;
                }
            }
        }
        return false;
    };
    const auto incidentEndpointCount = [](const vn::view::render::GeometryRenderModel& geometry, const Point& point) {
        int count = 0;
        for (const auto& edge: geometry.edges) {
            if (coincidentRenderPoint(edge.start, point)) {
                ++count;
            }
            if (coincidentRenderPoint(edge.end, point)) {
                ++count;
            }
        }
        return count;
    };

    painter.save();
    for (const auto& drawable: pageInfo.drawables) {
        const auto* geometry = std::get_if<vn::view::render::GeometryRenderModel>(&drawable);
        if (!geometry) {
            continue;
        }
        const bool isFocusObject = focusObject && geometry->objectId == *focusObject;
        if (!drawGlobalVertexOverlay && !isFocusObject) {
            continue;
        }

        const bool isObjectSelected = selected && selected->pageIndex == pageIndex &&
                                      selectionMode == QtGeometrySelectionMode::Object &&
                                      selected->hit.objectId == geometry->objectId;
        if (isObjectSelected) {
            const auto bounds = selectedGeometrySceneBounds();
            if (bounds && bounds->pageIndex == pageIndex) {
                QPen boundsPen(objectColor, 1.2 / std::max(this->zoomFactor, 0.001));
                boundsPen.setStyle(Qt::DashLine);
                painter.setPen(boundsPen);
                painter.setBrush(QColor(objectColor.red(), objectColor.green(), objectColor.blue(), 18));
                painter.drawRect(bounds->bounds.adjusted(-3.0 / std::max(this->zoomFactor, 0.001),
                                                         -3.0 / std::max(this->zoomFactor, 0.001),
                                                         3.0 / std::max(this->zoomFactor, 0.001),
                                                         3.0 / std::max(this->zoomFactor, 0.001)));
            }
        }

        for (const auto& vertex: geometry->vertices) {
            const bool isSelected = selected && selected->pageIndex == pageIndex && selected->hit.objectId == geometry->objectId &&
                                    std::find(selectedVertexIds.begin(), selectedVertexIds.end(), vertex.id) !=
                                            selectedVertexIds.end();
            const bool isHovered = hovered && hovered->pageIndex == pageIndex &&
                                   hovered->hit.type == vn::view::render::GeometryHitType::Vertex &&
                                   hovered->hit.objectId == geometry->objectId && hovered->hit.vertexId == vertex.id;

            const double zoomScale = std::max(this->zoomFactor, 0.001);
            const bool passiveVertex = drawGlobalVertexOverlay && !isFocusObject && !isSelected && !isHovered;
            const double size = (isHovered ? 9.0 : isSelected ? 8.0 : passiveVertex ? 5.2 : 6.5) / zoomScale;
            const QRectF handle(rect.x() + vertex.position.x - size / 2.0, rect.y() + vertex.position.y - size / 2.0,
                                size, size);
            const QPointF center(rect.x() + vertex.position.x, rect.y() + vertex.position.y);
            const bool isLinked = hasCoincidentPeer(*geometry, vertex) ||
                                  incidentEndpointCount(*geometry, vertex.position) > 1;
            const QColor selectedVertexColor(245, 130, 32);
            const QColor passiveVertexColor(0, 153, 191, 165);
            const QColor markerColor = isSelected              ? selectedVertexColor
                                       : isHovered             ? hoverVertexColor
                                       : isObjectSelected      ? objectColor
                                       : passiveVertex         ? passiveVertexColor
                                                               : modeAccent;
            if (this->geometryLinkedVertexOverlayEnabled && isLinked) {
                const QColor linkedColor(0, 153, 191, 220);
                const double ringRadius = (isHovered || isSelected ? 7.4 : passiveVertex ? 5.4 : 6.4) / zoomScale;
                painter.setPen(QPen(linkedColor, 1.5 / zoomScale));
                painter.setBrush(Qt::NoBrush);
                painter.drawEllipse(center, ringRadius, ringRadius);
                painter.drawLine(QPointF(center.x() - ringRadius * 0.56, center.y() - ringRadius * 0.56),
                                 QPointF(center.x() + ringRadius * 0.56, center.y() + ringRadius * 0.56));
            }
            painter.setPen(QPen(markerColor, (isHovered ? 2.1 : isSelected ? 1.9 : 1.4) / zoomScale));
            painter.setBrush(isSelected ? QBrush(QColor(255, 168, 68, 230))
                                        : isHovered        ? QBrush(QColor(0, 115, 255, 36))
                                        : isObjectSelected ? QBrush(QColor(132, 85, 214, 46))
                                        : passiveVertex    ? QBrush(QColor(255, 255, 255, 170))
                                                           : QBrush(QColor(255, 255, 255, 240)));
            painter.drawRect(handle);
            if (isSelected || isHovered) {
                painter.setPen(Qt::NoPen);
                painter.setBrush(isSelected && !isHovered ? QColor(255, 255, 255) : markerColor);
                painter.drawEllipse(center, 1.8 / zoomScale, 1.8 / zoomScale);
            }
        }
        if (!drawGlobalVertexOverlay) {
            break;
        }
    }

    const QPointF indicatorCenter = [&]() -> QPointF {
        if (drag && drag->pageIndex == pageIndex) {
            const auto& point = drag->snapKind ? drag->snapPoint : drag->currentPosition;
            return QPointF(rect.x() + point.x, rect.y() + point.y);
        }
        if (hovered && hovered->pageIndex == pageIndex) {
            return QPointF(rect.x() + hovered->hit.point.x, rect.y() + hovered->hit.point.y);
        }
        return selected && selected->pageIndex == pageIndex
                       ? QPointF(rect.x() + selected->hit.point.x, rect.y() + selected->hit.point.y)
                       : QPointF();
    }();
    const auto indicatorKind = [&]() -> std::optional<vn::snap::SnapKind> {
        if (drag && drag->pageIndex == pageIndex) {
            return drag->snapKind;
        }
        if (hovered && hovered->pageIndex == pageIndex) {
            return hovered->hit.snapKind;
        }
        if (selected && selected->pageIndex == pageIndex) {
            return selected->hit.snapKind;
        }
        return std::nullopt;
    }();
    const bool hasIndicator =
            (drag && drag->pageIndex == pageIndex && drag->snapKind) ||
            (hovered && hovered->pageIndex == pageIndex && shouldDrawPassiveSnapMarker(hovered->hit.snapKind)) ||
            (selected && selected->pageIndex == pageIndex && shouldDrawPassiveSnapMarker(selected->hit.snapKind));
    if (hasIndicator) {
        const QPointF center = indicatorCenter;
        drawSnapMarker(painter, center, indicatorKind, this->vertexSnapMarkerSize);
    }
    painter.restore();
}

void QtCanvas::beginPan(const QPointF& position) {
    this->panning = true;
    this->lastPanScreenPosition = position;
    setCursor(Qt::ClosedHandCursor);
}

void QtCanvas::endPan() {
    this->panning = false;
    if (this->spaceHeld) {
        setCursor(Qt::OpenHandCursor);
    } else {
        refreshToolCursor();
    }
}

auto QtCanvas::pointerButtonMatrixForDevice(const QInputDevice* device) const -> const QtPointerButtonMatrix& {
    const QString key = qtInputDeviceKey(device);
    if (key.isEmpty()) {
        return this->buttonMatrix;
    }

    for (const auto& profile: this->inputDeviceButtonProfiles) {
        if (profile.customButtonMatrix && QString::fromStdString(profile.key) == key) {
            return profile.buttonMatrix;
        }
    }
    return this->buttonMatrix;
}

auto QtCanvas::pointerActionForMouseButton(Qt::MouseButton button, const QInputDevice* device) const
        -> QtPointerButtonAction {
    const auto& matrix = pointerButtonMatrixForDevice(device);
    switch (button) {
        case Qt::LeftButton:
            return matrix.mouseLeftAction;
        case Qt::MiddleButton:
            return matrix.mouseMiddleAction;
        case Qt::RightButton:
            return matrix.mouseRightAction;
        case Qt::BackButton:
            return matrix.mouseBackAction;
        case Qt::ForwardButton:
            return matrix.mouseForwardAction;
        default:
            return QtPointerButtonAction::None;
    }
}

auto QtCanvas::pointerActionForTabletEvent(const QTabletEvent& event) const -> QtPointerButtonAction {
    const auto& matrix = pointerButtonMatrixForDevice(event.device());
    const auto* pointingDevice = event.pointingDevice();
    if (pointingDevice && pointingDevice->pointerType() == QPointingDevice::PointerType::Eraser) {
        return matrix.eraserTipAction;
    }
    if (event.button() == Qt::RightButton || event.buttons().testFlag(Qt::RightButton)) {
        return matrix.stylusButton1Action;
    }
    if (event.button() == Qt::MiddleButton || event.buttons().testFlag(Qt::MiddleButton)) {
        return matrix.stylusButton2Action;
    }
    return QtPointerButtonAction::None;
}

auto QtCanvas::pointerActionForTouchDevice(const QInputDevice* device) const -> QtPointerButtonAction {
    return pointerButtonMatrixForDevice(device).touchAction;
}

auto QtCanvas::beginPointerAction(QtPointerButtonAction action, const QPointF& screenPoint, double pressure) -> bool {
    if (this->spaceHeld || action == QtPointerButtonAction::None) {
        return false;
    }
    if (action == QtPointerButtonAction::Pan) {
        beginPan(screenPoint);
        return true;
    }
    if (action == QtPointerButtonAction::Eraser) {
        this->temporaryRightButtonEraser = this->currentToolState.activeTool != QtToolType::Eraser;
        if (this->eraserCursorHidden || this->temporaryRightButtonEraser) {
            setCursor(Qt::BlankCursor);
        }
        Q_EMIT toolStateChanged();
        beginEraseAtScreen(screenPoint);
        if (!this->erasing) {
            this->temporaryRightButtonEraser = false;
            refreshToolCursor();
            Q_EMIT toolStateChanged();
        }
        (void) pressure;
        return true;
    }
    return false;
}

auto QtCanvas::releasePointerAction(QtPointerButtonAction action) -> bool {
    if (action != QtPointerButtonAction::Eraser || !this->erasing || !this->temporaryRightButtonEraser) {
        return false;
    }
    finalizeErase();
    this->temporaryRightButtonEraser = false;
    clearEraserPreview();
    refreshToolCursor();
    Q_EMIT toolStateChanged();
    return true;
}

// ---------------------------------------------------------------------------
// Tool state
// ---------------------------------------------------------------------------

void QtCanvas::setCursorForTool(QtToolType tool) {
    switch (tool) {
        case QtToolType::Pen:
        case QtToolType::LaserPointerPen:
        case QtToolType::LaserPointerHighlighter:
        case QtToolType::Setsquare:
        case QtToolType::Compass:
        case QtToolType::Highlighter:
        case QtToolType::DrawLine:
        case QtToolType::DrawRectangle:
        case QtToolType::DrawCircle:
        case QtToolType::DrawEllipse:
        case QtToolType::DrawArrow:
        case QtToolType::DrawDoubleArrow:
        case QtToolType::DrawCoordinateSystem:
        case QtToolType::DrawSpline:
        case QtToolType::ShapeRecognizer:
        case QtToolType::DrawArc:
        case QtToolType::DrawEdge:
        case QtToolType::DrawPolyline:
        case QtToolType::DrawConstructionLine:
        case QtToolType::DrawConstructionCircle:
            setCursor(Qt::CrossCursor);
            break;
        case QtToolType::Eraser:
            setCursor(this->eraserCursorHidden ? Qt::BlankCursor : Qt::CrossCursor);
            break;
        case QtToolType::Hand:
            setCursor(Qt::OpenHandCursor);
            break;
        case QtToolType::Text:
        case QtToolType::PdfTextLinear:
        case QtToolType::PdfTextRect:
            setCursor(Qt::IBeamCursor);
            break;
        case QtToolType::SelectRect:
        case QtToolType::SelectRegion:
        case QtToolType::SelectMultiLayerRect:
        case QtToolType::SelectMultiLayerRegion:
        case QtToolType::SelectObject:
        case QtToolType::VerticalSpace:
            setCursor(Qt::ArrowCursor);
            break;
    }
}

void QtCanvas::refreshToolCursor() {
    if (this->spaceHeld) {
        setCursor(Qt::OpenHandCursor);
        return;
    }
    if (this->temporaryRightButtonEraser) {
        setCursor(Qt::BlankCursor);
        return;
    }
    setCursorForTool(this->currentToolState.activeTool);
}

void QtCanvas::setActiveTool(QtToolType tool) {
    if (this->drawing) {
        cancelActiveStroke();
    }
    if (this->pdfTextSelecting) {
        cancelPdfTextSelection();
    }
    if (this->activeInstrumentStroke) {
        cancelInstrumentTool();
    }
    if (this->documentController && this->documentController->isVerticalSpacing()) {
        cancelVerticalSpace();
    }
    this->movingInstrumentOverlay = false;
    this->currentToolState.activeTool = tool;
    refreshToolCursor();

    if ((tool == QtToolType::Setsquare || tool == QtToolType::Compass) && !this->instrumentOverlay.visible) {
        const auto rects = pageRects();
        if (!rects.empty()) {
            const QPointF sceneCenter = screenToScene(rect().center());
            const auto pageIdx = pageIndexAtScenePoint(sceneCenter).value_or(0U);
            const QRectF& pageRect = rects[std::min(pageIdx, rects.size() - 1U)];
            ensureInstrumentOverlay(std::min(pageIdx, rects.size() - 1U),
                                    QPointF(pageRect.width() * 0.5, pageRect.height() * 0.35));
        }
    }
    Q_EMIT toolStateChanged();
    update();
}

auto QtCanvas::activeTool() const -> QtToolType {
    return this->temporaryRightButtonEraser ? QtToolType::Eraser : this->currentToolState.activeTool;
}

auto QtCanvas::toolState() -> QtToolState& { return this->currentToolState; }

auto QtCanvas::toolState() const -> const QtToolState& { return this->currentToolState; }

auto QtCanvas::canUndo() const -> bool { return this->documentController && this->documentController->canUndo(); }

auto QtCanvas::canRedo() const -> bool { return this->documentController && this->documentController->canRedo(); }

auto QtCanvas::performUndo() -> bool {
    if (!this->documentController) {
        return false;
    }
    const bool changed = this->documentController->undo();
    if (changed) {
        update();
        Q_EMIT documentEdited();
    }
    return changed;
}

auto QtCanvas::performRedo() -> bool {
    if (!this->documentController) {
        return false;
    }
    const bool changed = this->documentController->redo();
    if (changed) {
        update();
        Q_EMIT documentEdited();
    }
    return changed;
}

auto QtCanvas::contentRenderer() const -> vn::view::render::PageContentRenderer* {
    return this->pageContentRenderer.get();
}
