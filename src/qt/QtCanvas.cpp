/*
 * VertexNote
 *
 * Qt canvas bootstrap.
 */

#include "QtCanvas.h"

#include <algorithm>
#include <cmath>
#include <type_traits>

#include <QCursor>
#include <QEvent>
#include <QFontMetrics>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QPaintEvent>
#include <QPainter>
#include <QPainterPath>
#include <QPalette>
#include <QPen>
#include <QRect>
#include <QTabletEvent>
#include <QTouchEvent>
#include <QTransform>
#include <QResizeEvent>
#include <QShowEvent>
#include <QStringList>
#include <QWheelEvent>

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
constexpr double ZOOM_STEP = 1.15;
constexpr double PAGE_STACK_X = 120.0;
constexpr double PAGE_STACK_Y = 100.0;
constexpr double PAGE_STACK_GAP = 56.0;
constexpr double GEOMETRY_HIT_RADIUS_PIXELS = 10.0;
constexpr double ROTATION_SNAP_STEP_RADIANS = M_PI / 12.0;

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

auto snapLabel(std::optional<vn::snap::SnapKind> kind) -> QString {
    if (!kind) {
        return QStringLiteral("HIT");
    }

    switch (*kind) {
        case vn::snap::SnapKind::Grid:
            return QStringLiteral("GRID");
        case vn::snap::SnapKind::ExplicitVertex:
        case vn::snap::SnapKind::EdgeEndpoint:
            return QStringLiteral("VERTEX");
        case vn::snap::SnapKind::Midpoint:
            return QStringLiteral("MID");
        case vn::snap::SnapKind::EdgeProjection:
            return QStringLiteral("PROJ");
        case vn::snap::SnapKind::Intersection:
            return QStringLiteral("INT");
        case vn::snap::SnapKind::ConstraintGuide:
            return QStringLiteral("CONST");
    }

    return QStringLiteral("HIT");
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

auto cubicSplinePath(const std::vector<QPointF>& points) -> QPainterPath {
    QPainterPath path;
    if (points.empty()) {
        return path;
    }

    path.moveTo(points.front());
    if (points.size() == 1U) {
        return path;
    }
    if (points.size() == 2U) {
        path.lineTo(points.back());
        return path;
    }

    for (std::size_t index = 0; index + 1 < points.size(); ++index) {
        const QPointF& p0 = index == 0 ? points[index] : points[index - 1];
        const QPointF& p1 = points[index];
        const QPointF& p2 = points[index + 1];
        const QPointF& p3 = (index + 2 < points.size()) ? points[index + 2] : points[index + 1];
        const QPointF c1(p1.x() + (p2.x() - p0.x()) / 6.0, p1.y() + (p2.y() - p0.y()) / 6.0);
        const QPointF c2(p2.x() - (p3.x() - p1.x()) / 6.0, p2.y() - (p3.y() - p1.y()) / 6.0);
        path.cubicTo(c1, c2, p2);
    }

    return path;
}

auto buildArrowPreviewPoints(const QPointF& start, const QPointF& end, double thickness, bool doubleEnded)
        -> std::vector<QPointF> {
    const double lineLength = std::hypot(end.x() - start.x(), end.y() - start.y());
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

    const double angle = std::atan2(end.y() - start.y(), end.x() - start.x());
    std::vector<QPointF> shape;
    shape.reserve(doubleEnded ? 10 : 6);
    shape.emplace_back(start);
    if (doubleEnded) {
        shape.emplace_back(start.x() + arrowDist * std::cos(angle + delta),
                           start.y() + arrowDist * std::sin(angle + delta));
        shape.emplace_back(start);
        shape.emplace_back(start.x() + arrowDist * std::cos(angle - delta),
                           start.y() + arrowDist * std::sin(angle - delta));
        shape.emplace_back(start);
    }
    shape.emplace_back(end);
    shape.emplace_back(end.x() - arrowDist * std::cos(angle + delta), end.y() - arrowDist * std::sin(angle + delta));
    shape.emplace_back(end);
    shape.emplace_back(end.x() - arrowDist * std::cos(angle - delta), end.y() - arrowDist * std::sin(angle - delta));
    shape.emplace_back(end);
    return shape;
}

auto buildCoordinateSystemPreviewPoints(const QPointF& start, const QPointF& current) -> std::vector<QPointF> {
    return {start, QPointF(start.x(), current.y()), QPointF(current.x(), current.y())};
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
    fitPage(false);
}

void QtCanvas::newBlankDocument() {
    this->zoomFactor = 1.0;
    this->scrollX = 0.0;
    this->scrollY = 0.0;
    fitPage(false);
    updateDebugOverlay(QStringLiteral("new document"));
}

void QtCanvas::setViewportState(double zoom, double scrollX, double scrollY) {
    this->zoomFactor = clampZoom(zoom);
    this->scrollX = scrollX;
    this->scrollY = scrollY;
    emitViewportUpdate(false);
}

auto QtCanvas::sessionViewportState() const -> QtViewportState {
    return {.zoom = this->zoomFactor, .scrollX = this->scrollX, .scrollY = this->scrollY};
}

void QtCanvas::zoomIn() { zoomAroundScreenPoint(ZOOM_STEP, rect().center()); }

void QtCanvas::zoomOut() { zoomAroundScreenPoint(1.0 / ZOOM_STEP, rect().center()); }

void QtCanvas::resetViewport() {
    fitPage();
    updateDebugOverlay(QStringLiteral("viewport reset"));
}

void QtCanvas::fitPage(bool edited) {
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
    if (this->pairedPagesEnabled == enabled) {
        return;
    }
    this->pairedPagesEnabled = enabled;
    fitPage();
}

auto QtCanvas::isPairedPagesEnabled() const -> bool { return this->pairedPagesEnabled; }

void QtCanvas::setVerticalLayout(bool enabled) {
    if (this->verticalLayoutEnabled == enabled) {
        return;
    }
    this->verticalLayoutEnabled = enabled;
    fitPage();
}

auto QtCanvas::isVerticalLayout() const -> bool { return this->verticalLayoutEnabled; }

void QtCanvas::setRightToLeftLayout(bool enabled) {
    if (this->rightToLeftLayoutEnabled == enabled) {
        return;
    }
    this->rightToLeftLayoutEnabled = enabled;
    fitPage();
}

auto QtCanvas::isRightToLeftLayout() const -> bool { return this->rightToLeftLayoutEnabled; }

void QtCanvas::setBottomToTopLayout(bool enabled) {
    if (this->bottomToTopLayoutEnabled == enabled) {
        return;
    }
    this->bottomToTopLayoutEnabled = enabled;
    fitPage();
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
    const auto rects = pageRects();
    if (rects.empty()) {
        return;
    }
    if (!isVisible()) {
        this->deferredFitWidthPending = true;
    }
    // Find the widest page
    double maxWidth = 0.0;
    for (const auto& r: rects) {
        maxWidth = std::max(maxWidth, r.width());
    }
    const double padding = 32.0;
    const double availableWidth = std::max(1.0, width() - 2.0 * padding);
    this->zoomFactor = clampZoom(availableWidth / std::max(maxWidth, 1.0));
    const double visibleWorldWidth = width() / this->zoomFactor;
    this->scrollX = rects[0].left() - (visibleWorldWidth - maxWidth) / 2.0;
    this->scrollY = std::max(0.0, rects[0].top() - 12.0 / this->zoomFactor);
    this->deferredFitWidthPending = false;
    emitViewportUpdate();
}

void QtCanvas::zoomToActualSize() {
    this->zoomFactor = 1.0;
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

void QtCanvas::setRotationSnapEnabled(bool enabled) {
    this->rotationSnapEnabled = enabled;
    update();
}

void QtCanvas::setTouchDrawingEnabled(bool enabled) {
    if (!enabled && this->drawing) {
        finalizeActiveStroke();
    }
    this->touchDrawingEnabled = enabled;
}

auto QtCanvas::isRotationSnapEnabled() const -> bool { return this->rotationSnapEnabled; }

auto QtCanvas::isTouchDrawingEnabled() const -> bool { return this->touchDrawingEnabled; }

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
    painter.fillRect(rect(), QColor(0xdc, 0xda, 0xd5));

    QTransform viewTransform;
    viewTransform.translate(-this->scrollX * this->zoomFactor, -this->scrollY * this->zoomFactor);
    viewTransform.scale(this->zoomFactor, this->zoomFactor);
    painter.setTransform(viewTransform);

    painter.setRenderHint(QPainter::Antialiasing, true);
    const auto pages =
            this->documentController ? this->documentController->snapshotPages()
                                     : std::vector<vn::view::render::PageRenderSnapshot>{};
    const auto rects = pageRects();
    const auto currentPage = currentPageIndex();
    for (std::size_t index = 0; index < rects.size(); ++index) {
        drawPageContents(painter, rects[index],
                         index < pages.size() ? pages[index] : vn::view::render::PageRenderSnapshot{}, index,
                         index == currentPage);
    }

    drawActiveStroke(painter);
    drawSelectionOverlay(painter);
    drawRubberBand(painter);
    drawShapePreview(painter);

    painter.resetTransform();

    if (event) {
        event->accept();
    }
}

void QtCanvas::resizeEvent(QResizeEvent* event) {
    QWidget::resizeEvent(event);
    if (this->deferredFitWidthPending && width() > 0 && height() > 0) {
        fitWidth();
    }
}

void QtCanvas::showEvent(QShowEvent* event) {
    QWidget::showEvent(event);
    if (this->deferredFitWidthPending && width() > 0 && height() > 0) {
        fitWidth();
    }
}

void QtCanvas::mousePressEvent(QMouseEvent* event) {
    this->inputAdapter->handleMousePress(*event);
    if (event->button() == Qt::MiddleButton || (event->button() == Qt::LeftButton && this->spaceHeld)) {
        beginPan(event->position());
        event->accept();
        return;
    }
    if (event->button() == Qt::LeftButton) {
        const auto tool = this->currentToolState.activeTool;
        if (tool == QtToolType::Pen || tool == QtToolType::Highlighter) {
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
        if (tool == QtToolType::SelectRect) {
            // Check if clicking on an already-selected element to start a move
            if (this->documentController && this->documentController->elementSelection()) {
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
        if (this->documentController && this->documentController->selectedGeometry() &&
            this->documentController->selectedGeometry()->hit.type == vn::view::render::GeometryHitType::Vertex) {
            if (this->documentController->beginGeometryVertexDrag(*this->documentController->selectedGeometry())) {
                setCursor(Qt::ClosedHandCursor);
            }
        }
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
            finalizeShape();
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
    if (this->panning && (event->button() == Qt::MiddleButton || event->button() == Qt::LeftButton)) {
        endPan();
        event->accept();
        return;
    }
    if (event->button() == Qt::LeftButton && this->drawing) {
        finalizeActiveStroke();
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
    if (event->button() == Qt::LeftButton && this->rubberBanding) {
        finalizeRubberBand();
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
    this->inputAdapter->handleMouseMove(*event);
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
        updateStrokeAtScreen(event->position(), 0.5);
        event->accept();
        return;
    }
    if (this->erasing) {
        eraseAtScreen(event->position());
        event->accept();
        return;
    }
    if (this->movingSelection) {
        updateMoveSelectionAtScreen(event->position());
        event->accept();
        return;
    }
    if (this->rubberBanding) {
        updateRubberBand(event->position());
        event->accept();
        return;
    }
    if (this->shapeDrawing) {
        updateShapeAtScreen(event->position());
        event->accept();
        return;
    }
    if (this->documentController && this->documentController->activeGeometryDrag()) {
        const auto& drag = *this->documentController->activeGeometryDrag();
        const auto rects = pageRects();
        if (drag.pageIndex < rects.size()) {
            const QPointF scenePoint = screenToScene(event->position());
            const auto& pageRect = rects[drag.pageIndex];
            const double pageX = scenePoint.x() - pageRect.x();
            const double pageY = scenePoint.y() - pageRect.y();
            static_cast<void>(this->documentController->updateGeometryVertexDrag(
                    pageX, pageY, this->zoomFactor,
                    {.geometryEnabled = this->geometrySnapEnabled, .gridEnabled = this->gridSnapEnabled}));
            update();
            event->accept();
            return;
        }
    }
    updateGeometryHover(event->position());
    QWidget::mouseMoveEvent(event);
}

void QtCanvas::wheelEvent(QWheelEvent* event) {
    this->inputAdapter->handleWheel(*event);
    if (event->modifiers().testFlag(Qt::ControlModifier)) {
        const double factor = event->angleDelta().y() >= 0 ? ZOOM_STEP : 1.0 / ZOOM_STEP;
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
    this->inputAdapter->handleTablet(*event);
    const auto tool = this->currentToolState.activeTool;
    const bool isDrawTool = tool == QtToolType::Pen || tool == QtToolType::Highlighter;
    const bool isEraserTool = tool == QtToolType::Eraser;
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
        if (this->rubberBanding) {
            cancelRubberBand();
        }
        if (this->movingSelection) {
            cancelMoveSelection();
        }
        this->documentController->clearElementSelection();
        this->documentController->clearInteractiveGeometryState();
        updateDebugOverlay(QStringLiteral("selection cleared"));
        if (!this->spaceHeld && !this->panning) {
            unsetCursor();
        }
        update();
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
            unsetCursor();
        }
    }
    QWidget::keyReleaseEvent(event);
}

bool QtCanvas::event(QEvent* event) {
    if (event && (event->type() == QEvent::TouchBegin || event->type() == QEvent::TouchUpdate ||
                  event->type() == QEvent::TouchEnd)) {
        this->inputAdapter->handleTouch(*static_cast<QTouchEvent*>(event));
    } else if (event && event->type() == QEvent::Leave) {
        clearGeometryHover();
    }
    return QWidget::event(event);
}

void QtCanvas::updateDebugOverlay(QString summary) {
    this->lastEventSummary = std::move(summary);
    update();
}

void QtCanvas::emitViewportUpdate(bool edited) {
    update();
    Q_EMIT viewportStateChanged();
    Q_EMIT statusHintChanged(QStringLiteral("Zoom %1% | Scroll (%2, %3)")
                                     .arg(this->zoomFactor * 100.0, 0, 'f', 0)
                                     .arg(this->scrollX, 0, 'f', 1)
                                     .arg(this->scrollY, 0, 'f', 1));
    if (edited) {
        Q_EMIT documentEdited();
    }
}

void QtCanvas::zoomAroundScreenPoint(double factor, const QPointF& screenPoint) {
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
    std::vector<QRectF> rects;
    const auto pages =
            this->documentController ? this->documentController->snapshotPages()
                                     : std::vector<vn::view::render::PageRenderSnapshot>{};
    if (pages.empty()) {
        rects.emplace_back(PAGE_STACK_X, PAGE_STACK_Y, 1100.0, 1500.0);
        return rects;
    }

    rects.reserve(pages.size());

    const std::size_t columnCount = this->pairedPagesEnabled ? 2U : 1U;
    double primary = PAGE_STACK_Y;
    double secondaryBase = PAGE_STACK_X;
    double maxRowExtent = 0.0;

    if (!this->verticalLayoutEnabled) {
        primary = PAGE_STACK_X;
        secondaryBase = PAGE_STACK_Y;
    }

    for (std::size_t index = 0; index < pages.size(); ++index) {
        const auto& page = pages[index];
        const double pageWidth = std::max(page.width, 1.0);
        const double pageHeight = std::max(page.height, 1.0);
        const std::size_t column = index % columnCount;

        double x = PAGE_STACK_X;
        double y = PAGE_STACK_Y;
        if (this->verticalLayoutEnabled) {
            x = secondaryBase + static_cast<double>(column) * (pageWidth + PAGE_STACK_GAP);
            y = primary;
        } else {
            x = primary;
            y = secondaryBase + static_cast<double>(column) * (pageHeight + PAGE_STACK_GAP);
        }

        rects.emplace_back(x, y, pageWidth, pageHeight);
        maxRowExtent = std::max(maxRowExtent, this->verticalLayoutEnabled ? pageHeight : pageWidth);

        const bool rowFinished = (column + 1U) == columnCount || index + 1U == pages.size();
        if (rowFinished) {
            primary += maxRowExtent + PAGE_STACK_GAP;
            maxRowExtent = 0.0;
        }
    }

    if (this->rightToLeftLayoutEnabled || this->bottomToTopLayoutEnabled) {
        QRectF bounds = rects.front();
        for (std::size_t index = 1; index < rects.size(); ++index) {
            bounds = bounds.united(rects[index]);
        }
        for (auto& rect: rects) {
            if (this->rightToLeftLayoutEnabled) {
                rect.moveLeft(bounds.right() - (rect.left() - bounds.left()) - rect.width());
            }
            if (this->bottomToTopLayoutEnabled) {
                rect.moveTop(bounds.bottom() - (rect.top() - bounds.top()) - rect.height());
            }
        }
    }
    return rects;
}

auto QtCanvas::documentSceneBounds() const -> QRectF {
    const auto rects = pageRects();
    QRectF bounds = rects.front();
    for (std::size_t i = 1; i < rects.size(); ++i) {
        bounds = bounds.united(rects[i]);
    }
    return bounds.adjusted(-80.0, -80.0, 80.0, 80.0);
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

    const QPointF scenePoint = screenToScene(screenPoint);
    const auto pageIndex = pageIndexAtScenePoint(scenePoint);
    if (!pageIndex) {
        clearGeometryHover();
        if (!this->spaceHeld) {
            unsetCursor();
        }
        return;
    }

    const auto rects = pageRects();
    const auto& pageRect = rects[*pageIndex];
    const double pageX = scenePoint.x() - pageRect.x();
    const double pageY = scenePoint.y() - pageRect.y();
    auto hit = this->documentController->hitTestGeometry(*pageIndex, pageX, pageY, this->zoomFactor, GEOMETRY_HIT_RADIUS_PIXELS);
    this->documentController->setHoveredGeometry(hit);
    if (hit) {
        if (!this->spaceHeld && !this->panning) {
            setCursor(hit->hit.type == vn::view::render::GeometryHitType::Vertex ? Qt::CrossCursor : Qt::PointingHandCursor);
        }
        updateDebugOverlay(QStringLiteral("geometry hover page=%1 object=%2")
                                   .arg(static_cast<int>(hit->pageIndex + 1))
                                   .arg(static_cast<qulonglong>(hit->hit.objectId)));
    } else if (!this->spaceHeld && !this->panning) {
        unsetCursor();
    }
    update();
}

void QtCanvas::clearGeometryHover() {
    if (!this->documentController) {
        return;
    }
    this->documentController->setHoveredGeometry(std::nullopt);
    update();
}

void QtCanvas::selectHoveredGeometry(bool additive) {
    if (!this->documentController) {
        return;
    }

    this->documentController->setSelectedGeometry(this->documentController->hoveredGeometry(), additive);
    if (!this->documentController->selectedGeometry()) {
        updateDebugOverlay(QStringLiteral("selection cleared"));
    } else {
        const auto& hit = *this->documentController->selectedGeometry();
        updateDebugOverlay(QStringLiteral("selected page=%1 object=%2 vertices=%3")
                                   .arg(static_cast<int>(hit.pageIndex + 1))
                                   .arg(static_cast<qulonglong>(hit.hit.objectId))
                                   .arg(static_cast<int>(this->documentController->selectedVertexIds().size())));
    }
    update();
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
    const auto& drag = this->documentController->activeGeometryDrag();

    const auto drawEdgeOverlay = [&](const QtGeometryHit& geometryHit, const QColor& color, double extraWidth) {
        if (geometryHit.pageIndex != pageIndex || geometryHit.hit.type != vn::view::render::GeometryHitType::Edge) {
            return;
        }

        for (const auto& drawable: pageInfo.drawables) {
            const auto* geometry = std::get_if<vn::view::render::GeometryRenderModel>(&drawable);
            if (!geometry || geometry->objectId != geometryHit.hit.objectId) {
                continue;
            }

            auto edgeIt = std::find_if(geometry->edges.begin(), geometry->edges.end(), [&](const auto& edge) {
                return edge.id == geometryHit.hit.edgeId;
            });
            if (edgeIt == geometry->edges.end()) {
                continue;
            }

            vn::view::render::GeometryRenderModel overlay;
            overlay.objectId = geometry->objectId;
            overlay.vertices = geometry->vertices;
            overlay.edges = {*edgeIt};
            overlay.color = Color(static_cast<uint8_t>(color.red()), static_cast<uint8_t>(color.green()),
                                  static_cast<uint8_t>(color.blue()));
            overlay.strokeWidth = geometry->strokeWidth + extraWidth;
            this->geometryRenderer->draw(overlay, renderContext);
            break;
        }
    };

    if (selected) {
        drawEdgeOverlay(*selected, QColor(0, 102, 255, 215), 2.2);
    }
    if (hovered) {
        drawEdgeOverlay(*hovered, QColor(0, 171, 255, 190), 1.2);
    }

    std::optional<vn::geom::ObjectId> focusObject;
    if (selected && selected->pageIndex == pageIndex) {
        focusObject = selected->hit.objectId;
    } else if (hovered && hovered->pageIndex == pageIndex) {
        focusObject = hovered->hit.objectId;
    }
    if (!focusObject) {
        return;
    }

    painter.save();
    for (const auto& drawable: pageInfo.drawables) {
        const auto* geometry = std::get_if<vn::view::render::GeometryRenderModel>(&drawable);
        if (!geometry || geometry->objectId != *focusObject) {
            continue;
        }

        for (const auto& vertex: geometry->vertices) {
            const bool isSelected = selected && selected->pageIndex == pageIndex && selected->hit.objectId == geometry->objectId &&
                                    std::find(selectedVertexIds.begin(), selectedVertexIds.end(), vertex.id) !=
                                            selectedVertexIds.end();
            const bool isHovered = hovered && hovered->pageIndex == pageIndex &&
                                   hovered->hit.type == vn::view::render::GeometryHitType::Vertex &&
                                   hovered->hit.objectId == geometry->objectId && hovered->hit.vertexId == vertex.id;

            const double zoomScale = std::max(this->zoomFactor, 0.001);
            const double size = (isHovered ? 9.0 : isSelected ? 8.0 : 6.5) / zoomScale;
            const QRectF handle(rect.x() + vertex.position.x - size / 2.0, rect.y() + vertex.position.y - size / 2.0,
                                size, size);
            painter.setPen(QPen(QColor(0, 102, 255), (isHovered ? 2.1 : isSelected ? 1.9 : 1.4) / zoomScale));
            painter.setBrush(isSelected ? QBrush(QColor(0, 102, 255)) : QBrush(QColor(255, 255, 255, 240)));
            painter.drawRect(handle);
            if (isSelected || isHovered) {
                painter.setPen(Qt::NoPen);
                painter.setBrush(isSelected && !isHovered ? QColor(255, 255, 255) : QColor(0, 102, 255));
                painter.drawEllipse(QPointF(rect.x() + vertex.position.x, rect.y() + vertex.position.y),
                                    1.8 / zoomScale, 1.8 / zoomScale);
            }
        }
        break;
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
    const bool hasIndicator = (drag && drag->pageIndex == pageIndex) || (hovered && hovered->pageIndex == pageIndex) ||
                              (selected && selected->pageIndex == pageIndex);
    if (hasIndicator) {
        const QPointF center = indicatorCenter;
        const QColor color = snapColor(indicatorKind);
        painter.setPen(QPen(QColor(255, 255, 255, 235), 3.2));
        painter.setBrush(QColor(color.red(), color.green(), color.blue(), 40));
        painter.drawEllipse(center, 4.8, 4.8);
        painter.setPen(QPen(color, 1.4));
        painter.drawEllipse(center, 4.8, 4.8);
        painter.drawLine(QPointF(center.x() - 4.8, center.y()), QPointF(center.x() + 4.8, center.y()));
        painter.drawLine(QPointF(center.x(), center.y() - 4.8), QPointF(center.x(), center.y() + 4.8));

        const QString label = drag && drag->pageIndex == pageIndex && drag->snapKind ? snapLabel(drag->snapKind)
                                                                                      : snapLabel(indicatorKind);
        QFontMetrics metrics(painter.font());
        const int labelWidth = metrics.horizontalAdvance(label) + 10;
        const QRectF badge(center.x() + 10.0, center.y() - 22.0, labelWidth, 18.0);
        painter.setPen(Qt::NoPen);
        painter.setBrush(QColor(255, 255, 255, 225));
        painter.drawRoundedRect(badge, 6.0, 6.0);
        painter.setPen(color);
        painter.drawText(badge, Qt::AlignCenter, label);
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
        unsetCursor();
    }
}

// ---------------------------------------------------------------------------
// Tool state
// ---------------------------------------------------------------------------

void QtCanvas::setActiveTool(QtToolType tool) {
    if (this->drawing) {
        cancelActiveStroke();
    }
    this->currentToolState.activeTool = tool;
    switch (tool) {
        case QtToolType::Pen:
        case QtToolType::Highlighter:
        case QtToolType::Eraser:
        case QtToolType::DrawLine:
        case QtToolType::DrawRectangle:
        case QtToolType::DrawCircle:
        case QtToolType::DrawEllipse:
        case QtToolType::DrawArrow:
        case QtToolType::DrawDoubleArrow:
        case QtToolType::DrawCoordinateSystem:
        case QtToolType::DrawSpline:
        case QtToolType::DrawArc:
        case QtToolType::DrawPolyline:
        case QtToolType::DrawConstructionLine:
        case QtToolType::DrawConstructionCircle:
            setCursor(Qt::CrossCursor);
            break;
        case QtToolType::Hand:
            setCursor(Qt::OpenHandCursor);
            break;
        case QtToolType::Text:
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
    update();
}

auto QtCanvas::activeTool() const -> QtToolType { return this->currentToolState.activeTool; }

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

// ---------------------------------------------------------------------------
// Stroke input
// ---------------------------------------------------------------------------

void QtCanvas::beginStrokeAtScreen(const QPointF& screenPoint, double pressure) {
    if (!this->documentController) {
        return;
    }

    const QPointF scenePoint = screenToScene(screenPoint);
    const auto pageIdx = pageIndexAtScenePoint(scenePoint);
    if (!pageIdx) {
        return;
    }

    const auto rects = pageRects();
    if (*pageIdx >= rects.size()) {
        return;
    }

    const QRectF& pageRect = rects[*pageIdx];
    const double pageX = scenePoint.x() - pageRect.x();
    const double pageY = scenePoint.y() - pageRect.y();

    Color color;
    double width;
    StrokeTool::Value toolType;
    const auto tool = this->currentToolState.activeTool;
    if (tool == QtToolType::Pen) {
        color = this->currentToolState.penColor;
        width = this->currentToolState.penWidth;
        toolType = StrokeTool::PEN;
    } else if (tool == QtToolType::Highlighter) {
        color = this->currentToolState.highlighterColor;
        width = this->currentToolState.highlighterWidth;
        toolType = StrokeTool::HIGHLIGHTER;
    } else {
        return;
    }

    if (this->documentController->beginStroke(*pageIdx, pageX, pageY, pressure, color, width, toolType,
                                               this->currentToolState.pressureSensitive,
                                               this->currentToolState.penLineStyle,
                                               this->currentToolState.fillEnabled
                                                       ? this->currentToolState.fillOpacity
                                                       : -1)) {
        this->drawing = true;
        update();
    }
}

void QtCanvas::updateStrokeAtScreen(const QPointF& screenPoint, double pressure) {
    if (!this->documentController || !this->drawing) {
        return;
    }

    const auto* active = this->documentController->activeStroke();
    if (!active) {
        return;
    }

    const auto rects = pageRects();
    if (active->pageIndex >= rects.size()) {
        return;
    }

    const QPointF scenePoint = screenToScene(screenPoint);
    const QRectF& pageRect = rects[active->pageIndex];
    const double pageX = scenePoint.x() - pageRect.x();
    const double pageY = scenePoint.y() - pageRect.y();

    if (this->documentController->updateStroke(pageX, pageY, pressure)) {
        update();
    }
}

void QtCanvas::finalizeActiveStroke() {
    if (!this->documentController || !this->drawing) {
        return;
    }

    const bool added = this->documentController->finalizeStroke();
    this->drawing = false;
    this->activeTouchPointId = -1;
    update();
    if (added) {
        Q_EMIT documentEdited();
    }
}

void QtCanvas::cancelActiveStroke() {
    if (this->documentController) {
        this->documentController->cancelStroke();
    }
    this->drawing = false;
    this->activeTouchPointId = -1;
    update();
}

void QtCanvas::drawActiveStroke(QPainter& painter) const {
    const auto* active = this->documentController ? this->documentController->activeStroke() : nullptr;
    if (!active || !active->stroke || !this->pageContentRenderer) {
        return;
    }

    const auto rects = pageRects();
    if (active->pageIndex >= rects.size()) {
        return;
    }

    const QRectF& pageRect = rects[active->pageIndex];
    painter.save();
    painter.setClipRect(pageRect);

    // Translate so the stroke renderer draws at page position
    painter.translate(pageRect.x(), pageRect.y());

    auto model = vn::view::render::StrokeRenderModelFactory::fromStroke(*active->stroke);
    vn::view::render::QtPainterRenderContext renderContext(&painter, this->zoomFactor);
    this->pageContentRenderer->drawStroke(model, renderContext);

    painter.restore();
}

// ---------------------------------------------------------------------------
// Eraser input
// ---------------------------------------------------------------------------

void QtCanvas::beginEraseAtScreen(const QPointF& screenPoint) {
    if (!this->documentController) {
        return;
    }

    const QPointF scenePoint = screenToScene(screenPoint);
    const auto pageIdx = pageIndexAtScenePoint(scenePoint);
    if (!pageIdx) {
        return;
    }

    const auto rects = pageRects();
    if (*pageIdx >= rects.size()) {
        return;
    }

    this->documentController->beginErase(*pageIdx);
    this->erasing = true;

    // Immediately erase at the press point
    const QRectF& pageRect = rects[*pageIdx];
    const double pageX = scenePoint.x() - pageRect.x();
    const double pageY = scenePoint.y() - pageRect.y();
    const double halfSize = this->currentToolState.eraserWidth / 2.0;

    const bool segment = this->currentToolState.eraserMode == QtEraserMode::Segment;
    const int erased = segment ? this->documentController->eraseSegmentAt(*pageIdx, pageX, pageY, halfSize)
                               : this->documentController->eraseAt(*pageIdx, pageX, pageY, halfSize);
    if (erased > 0) {
        update();
        Q_EMIT documentEdited();
    }
}

void QtCanvas::eraseAtScreen(const QPointF& screenPoint) {
    if (!this->documentController || !this->erasing) {
        return;
    }

    const QPointF scenePoint = screenToScene(screenPoint);
    const auto pageIdx = pageIndexAtScenePoint(scenePoint);
    if (!pageIdx) {
        return;
    }

    const auto rects = pageRects();
    if (*pageIdx >= rects.size()) {
        return;
    }

    const QRectF& pageRect = rects[*pageIdx];
    const double pageX = scenePoint.x() - pageRect.x();
    const double pageY = scenePoint.y() - pageRect.y();
    const double halfSize = this->currentToolState.eraserWidth / 2.0;

    const bool segment = this->currentToolState.eraserMode == QtEraserMode::Segment;
    const int erased = segment ? this->documentController->eraseSegmentAt(*pageIdx, pageX, pageY, halfSize)
                               : this->documentController->eraseAt(*pageIdx, pageX, pageY, halfSize);
    if (erased > 0) {
        update();
        Q_EMIT documentEdited();
    }
}

void QtCanvas::finalizeErase() {
    if (!this->documentController) {
        this->erasing = false;
        return;
    }
    this->documentController->finalizeErase();
    this->erasing = false;
    Q_EMIT documentEdited();
}

void QtCanvas::cancelErase() {
    if (this->documentController) {
        this->documentController->cancelErase();
    }
    this->erasing = false;
}

// ---------------------------------------------------------------------------
// Text editing
// ---------------------------------------------------------------------------

void QtCanvas::beginTextEditAtScreen(const QPointF& screenPoint) {
    if (!this->documentController) {
        return;
    }

    // Commit any existing text edit first
    commitTextEdit();

    const QPointF scenePoint = screenToScene(screenPoint);
    const auto pageIdx = pageIndexAtScenePoint(scenePoint);
    if (!pageIdx) {
        return;
    }

    const auto rects = pageRects();
    if (*pageIdx >= rects.size()) {
        return;
    }

    const QRectF& pageRect = rects[*pageIdx];
    const double pageX = scenePoint.x() - pageRect.x();
    const double pageY = scenePoint.y() - pageRect.y();

    // Create text editor if needed
    if (!this->textEditor) {
        this->textEditor = new QtTextEditor(this);
        connect(this->textEditor, &QtTextEditor::editingFinished, this, [this](bool committed) {
            if (committed && this->textEditor->isNewText()) {
                auto textElem = this->textEditor->newTextElement();
                if (textElem) {
                    this->documentController->insertTextElement(this->textEditor->editedPageIndex(),
                                                                std::move(textElem));
                    update();
                    Q_EMIT documentEdited();
                }
            } else if (committed) {
                update();
                Q_EMIT documentEdited();
            }
        });
    }

    // Check if clicking on an existing text element
    const double hitRadius = 20.0 / this->zoomFactor;
    auto* existingText = this->documentController->hitTestTextElement(*pageIdx, pageX, pageY, hitRadius);

    if (existingText) {
        this->textEditor->beginEditing(existingText, pageRect, this->zoomFactor);
    } else {
        this->textEditor->beginNewText(*pageIdx, pageX, pageY, pageRect, this->zoomFactor,
                                       this->currentToolState.penColor, "Sans", 12.0);
    }
}

void QtCanvas::commitTextEdit() {
    if (this->textEditor && this->textEditor->isEditing()) {
        this->textEditor->commit();
    }
}

void QtCanvas::cancelTextEdit() {
    if (this->textEditor && this->textEditor->isEditing()) {
        this->textEditor->cancel();
    }
}

// ---------------------------------------------------------------------------
// Element selection (SelectRect tool)
// ---------------------------------------------------------------------------

void QtCanvas::selectElementAtScreen(const QPointF& screenPoint, bool additive) {
    if (!this->documentController) {
        return;
    }

    const QPointF scenePoint = screenToScene(screenPoint);
    const auto pageIdx = pageIndexAtScenePoint(scenePoint);
    if (!pageIdx) {
        if (!additive) {
            this->documentController->clearElementSelection();
        }
        update();
        return;
    }

    const auto rects = pageRects();
    const double pageX = scenePoint.x() - rects[*pageIdx].x();
    const double pageY = scenePoint.y() - rects[*pageIdx].y();
    const double hitRadius = 10.0 / this->zoomFactor;

    this->documentController->selectElementAt(*pageIdx, pageX, pageY, hitRadius, additive);
    const auto& sel = this->documentController->elementSelection();
    if (sel) {
        updateDebugOverlay(QStringLiteral("selected %1 element(s)").arg(static_cast<int>(sel->elements.size())));
    } else {
        updateDebugOverlay(QStringLiteral("selection cleared"));
    }
    update();
}

void QtCanvas::beginRubberBand(const QPointF& screenPoint) {
    this->rubberBanding = true;
    this->rubberBandOrigin = screenPoint;
    this->rubberBandCurrent = screenPoint;
}

void QtCanvas::updateRubberBand(const QPointF& screenPoint) {
    this->rubberBandCurrent = screenPoint;
    update();
}

void QtCanvas::finalizeRubberBand() {
    if (!this->rubberBanding) {
        return;
    }

    const QPointF delta = this->rubberBandCurrent - this->rubberBandOrigin;
    const bool isClick = std::abs(delta.x()) < 4.0 && std::abs(delta.y()) < 4.0;

    if (isClick) {
        // Single-click select
        selectElementAtScreen(this->rubberBandOrigin, false);
    } else if (this->documentController) {
        // Rubber-band rectangle select
        const QPointF sceneOrigin = screenToScene(this->rubberBandOrigin);
        const QPointF sceneCurrent = screenToScene(this->rubberBandCurrent);
        const double x = std::min(sceneOrigin.x(), sceneCurrent.x());
        const double y = std::min(sceneOrigin.y(), sceneCurrent.y());
        const double w = std::abs(sceneCurrent.x() - sceneOrigin.x());
        const double h = std::abs(sceneCurrent.y() - sceneOrigin.y());

        // Find which page the rubber band falls on
        const auto rects = pageRects();
        const QRectF bandRect(x, y, w, h);
        for (std::size_t i = 0; i < rects.size(); ++i) {
            if (rects[i].intersects(bandRect)) {
                const double pageX = x - rects[i].x();
                const double pageY = y - rects[i].y();
                const double pageW = w;
                const double pageH = h;
                this->documentController->selectElementsInRect(i, pageX, pageY, pageW, pageH);
                break;
            }
        }

        const auto& sel = this->documentController->elementSelection();
        if (sel) {
            updateDebugOverlay(
                    QStringLiteral("rect-selected %1 element(s)").arg(static_cast<int>(sel->elements.size())));
        } else {
            updateDebugOverlay(QStringLiteral("selection cleared"));
        }
    }

    this->rubberBanding = false;
    update();
}

void QtCanvas::cancelRubberBand() {
    this->rubberBanding = false;
    update();
}

void QtCanvas::beginMoveSelectionAtScreen(const QPointF& screenPoint) {
    if (!this->documentController) {
        return;
    }

    const QPointF scenePoint = screenToScene(screenPoint);
    const auto& sel = this->documentController->elementSelection();
    if (!sel) {
        return;
    }

    const auto rects = pageRects();
    if (sel->pageIndex >= rects.size()) {
        return;
    }

    const double pageX = scenePoint.x() - rects[sel->pageIndex].x();
    const double pageY = scenePoint.y() - rects[sel->pageIndex].y();

    if (this->documentController->beginMoveSelection(pageX, pageY)) {
        this->movingSelection = true;
        setCursor(Qt::ClosedHandCursor);
    }
}

void QtCanvas::updateMoveSelectionAtScreen(const QPointF& screenPoint) {
    if (!this->documentController || !this->movingSelection) {
        return;
    }

    const auto& sel = this->documentController->elementSelection();
    if (!sel) {
        return;
    }

    const auto rects = pageRects();
    if (sel->pageIndex >= rects.size()) {
        return;
    }

    const QPointF scenePoint = screenToScene(screenPoint);
    const double pageX = scenePoint.x() - rects[sel->pageIndex].x();
    const double pageY = scenePoint.y() - rects[sel->pageIndex].y();

    if (this->documentController->updateMoveSelection(pageX, pageY)) {
        update();
    }
}

void QtCanvas::finalizeMoveSelection() {
    if (!this->documentController) {
        this->movingSelection = false;
        return;
    }

    const bool changed = this->documentController->endMoveSelection();
    this->movingSelection = false;
    setCursor(Qt::ArrowCursor);
    update();
    if (changed) {
        Q_EMIT documentEdited();
    }
}

void QtCanvas::cancelMoveSelection() {
    if (this->documentController) {
        this->documentController->cancelMoveSelection();
    }
    this->movingSelection = false;
    setCursor(Qt::ArrowCursor);
    update();
}

void QtCanvas::drawSelectionOverlay(QPainter& painter) const {
    if (!this->documentController) {
        return;
    }

    const auto& sel = this->documentController->elementSelection();
    if (!sel || sel->elements.empty()) {
        return;
    }

    const auto rects = pageRects();
    if (sel->pageIndex >= rects.size()) {
        return;
    }

    const QRectF& pageRect = rects[sel->pageIndex];
    painter.save();

    // Compute union bounding box of all selected elements
    double minX = 1e18, minY = 1e18, maxX = -1e18, maxY = -1e18;
    for (const auto* elem: sel->elements) {
        if (!elem) {
            continue;
        }
        auto bounds = elem->boundingRect();
        minX = std::min(minX, bounds.x);
        minY = std::min(minY, bounds.y);
        maxX = std::max(maxX, bounds.x + bounds.width);
        maxY = std::max(maxY, bounds.y + bounds.height);
    }

    const QRectF selRect(pageRect.x() + minX, pageRect.y() + minY, maxX - minX, maxY - minY);
    const double handleSize = 6.0 / this->zoomFactor;
    const double margin = 4.0 / this->zoomFactor;
    const QRectF outerRect = selRect.adjusted(-margin, -margin, margin, margin);

    // Draw selection rectangle
    QPen dashPen(QColor(0, 120, 255, 200), 1.2 / this->zoomFactor);
    dashPen.setStyle(Qt::DashLine);
    dashPen.setCosmetic(false);
    painter.setPen(dashPen);
    painter.setBrush(QColor(0, 120, 255, 20));
    painter.drawRect(outerRect);

    // Draw corner handles
    painter.setPen(QPen(QColor(0, 120, 255), 1.0 / this->zoomFactor));
    painter.setBrush(QColor(255, 255, 255, 230));
    const QPointF corners[] = {outerRect.topLeft(), outerRect.topRight(), outerRect.bottomLeft(),
                               outerRect.bottomRight()};
    for (const auto& corner: corners) {
        painter.drawRect(QRectF(corner.x() - handleSize / 2.0, corner.y() - handleSize / 2.0, handleSize, handleSize));
    }

    painter.restore();
}

void QtCanvas::drawRubberBand(QPainter& painter) const {
    if (!this->rubberBanding) {
        return;
    }

    // Draw in screen coordinates
    painter.save();
    painter.resetTransform();

    const QRectF bandRect = QRectF(this->rubberBandOrigin, this->rubberBandCurrent).normalized();
    QPen pen(QColor(0, 120, 255, 180), 1.0);
    pen.setStyle(Qt::DashLine);
    painter.setPen(pen);
    painter.setBrush(QColor(0, 120, 255, 30));
    painter.drawRect(bandRect);

    painter.restore();
}

// ---------------------------------------------------------------------------
// Shape drawing
// ---------------------------------------------------------------------------

void QtCanvas::beginShapeAtScreen(const QPointF& screenPoint) {
    const QPointF scenePoint = screenToScene(screenPoint);
    const auto pageIdx = pageIndexAtScenePoint(scenePoint);
    if (!pageIdx) {
        return;
    }

    const auto rects = pageRects();
    this->shapePageIndex = *pageIdx;
    this->shapeStartScene = QPointF(scenePoint.x() - rects[*pageIdx].x(), scenePoint.y() - rects[*pageIdx].y());
    this->shapeCurrentScene = this->shapeStartScene;
    this->shapeClickPoints.clear();
    this->shapeClickPoints.push_back(this->shapeStartScene);
    this->shapeDrawing = true;
    setCursor(Qt::CrossCursor);
    update();
}

void QtCanvas::updateShapeAtScreen(const QPointF& screenPoint) {
    const QPointF scenePoint = screenToScene(screenPoint);
    const auto rects = pageRects();
    if (this->shapePageIndex < rects.size()) {
        QPointF pagePoint(scenePoint.x() - rects[this->shapePageIndex].x(),
                          scenePoint.y() - rects[this->shapePageIndex].y());
        if (this->rotationSnapEnabled) {
            const QPointF origin =
                    this->shapeClickPoints.empty() ? this->shapeStartScene : this->shapeClickPoints.back();
            pagePoint = applyRotationSnap(origin, pagePoint);
        }
        this->shapeCurrentScene = pagePoint;
    }
    update();
}

void QtCanvas::addShapeClickAtScreen(const QPointF& screenPoint) {
    const QPointF scenePoint = screenToScene(screenPoint);
    const auto rects = pageRects();
    if (this->shapePageIndex < rects.size()) {
        QPointF pagePoint(scenePoint.x() - rects[this->shapePageIndex].x(),
                          scenePoint.y() - rects[this->shapePageIndex].y());
        if (this->rotationSnapEnabled && !this->shapeClickPoints.empty()) {
            pagePoint = applyRotationSnap(this->shapeClickPoints.back(), pagePoint);
        }
        this->shapeClickPoints.push_back(pagePoint);
        this->shapeCurrentScene = pagePoint;
    }

    // For arc: finalize after 3 clicks (center, start, end)
    if (this->currentToolState.activeTool == QtToolType::DrawArc && this->shapeClickPoints.size() >= 3U) {
        finalizeShape();
        return;
    }
    update();
}

void QtCanvas::finalizeShape() {
    if (!this->documentController || !this->shapeDrawing) {
        cancelShape();
        return;
    }

    const Color color = this->currentToolState.penColor;
    const double width = this->currentToolState.penWidth;
    const std::string& lineStyle = this->currentToolState.penLineStyle;
    const int fill = this->currentToolState.fillEnabled ? this->currentToolState.fillOpacity : -1;
    const Element* created = nullptr;

    switch (this->currentToolState.activeTool) {
        case QtToolType::DrawLine:
            created = this->documentController->createLine(this->shapePageIndex, this->shapeStartScene.x(),
                                                           this->shapeStartScene.y(), this->shapeCurrentScene.x(),
                                                           this->shapeCurrentScene.y(), color, width);
            break;
        case QtToolType::DrawRectangle:
            created = this->documentController->createRectangle(this->shapePageIndex, this->shapeStartScene.x(),
                                                                this->shapeStartScene.y(), this->shapeCurrentScene.x(),
                                                                this->shapeCurrentScene.y(), color, width);
            break;
        case QtToolType::DrawCircle:
            created = this->documentController->createCircle(this->shapePageIndex, this->shapeStartScene.x(),
                                                             this->shapeStartScene.y(), this->shapeCurrentScene.x(),
                                                             this->shapeCurrentScene.y(), color, width);
            break;
        case QtToolType::DrawEllipse:
            created = this->documentController->createEllipse(
                    this->shapePageIndex, this->shapeStartScene.x(), this->shapeStartScene.y(),
                    this->shapeCurrentScene.x(), this->shapeCurrentScene.y(), color, width, lineStyle, fill);
            break;
        case QtToolType::DrawArrow:
            created = this->documentController->createArrow(
                    this->shapePageIndex, this->shapeStartScene.x(), this->shapeStartScene.y(),
                    this->shapeCurrentScene.x(), this->shapeCurrentScene.y(), color, width, lineStyle, false);
            break;
        case QtToolType::DrawDoubleArrow:
            created = this->documentController->createArrow(
                    this->shapePageIndex, this->shapeStartScene.x(), this->shapeStartScene.y(),
                    this->shapeCurrentScene.x(), this->shapeCurrentScene.y(), color, width, lineStyle, true);
            break;
        case QtToolType::DrawCoordinateSystem:
            created = this->documentController->createCoordinateSystem(
                    this->shapePageIndex, this->shapeStartScene.x(), this->shapeStartScene.y(),
                    this->shapeCurrentScene.x(), this->shapeCurrentScene.y(), color, width, lineStyle);
            break;
        case QtToolType::DrawArc:
            if (this->shapeClickPoints.size() >= 3U) {
                created = this->documentController->createArc(
                        this->shapePageIndex, this->shapeClickPoints[0].x(), this->shapeClickPoints[0].y(),
                        this->shapeClickPoints[1].x(), this->shapeClickPoints[1].y(),
                        this->shapeClickPoints[2].x(), this->shapeClickPoints[2].y(), color, width);
            }
            break;
        case QtToolType::DrawPolyline: {
            std::vector<std::pair<double, double>> points;
            points.reserve(this->shapeClickPoints.size());
            for (const auto& pt: this->shapeClickPoints) {
                points.emplace_back(pt.x(), pt.y());
            }
            created = this->documentController->createPolyline(this->shapePageIndex, points, color, width);
            break;
        }
        case QtToolType::DrawSpline: {
            std::vector<std::pair<double, double>> points;
            points.reserve(this->shapeClickPoints.size());
            for (const auto& pt: this->shapeClickPoints) {
                points.emplace_back(pt.x(), pt.y());
            }
            created = this->documentController->createSpline(this->shapePageIndex, points, color, width, lineStyle);
            break;
        }
        case QtToolType::DrawConstructionLine:
            created = this->documentController->createConstructionLine(
                    this->shapePageIndex, this->shapeStartScene.x(), this->shapeStartScene.y(),
                    this->shapeCurrentScene.x(), this->shapeCurrentScene.y(), color, width);
            break;
        case QtToolType::DrawConstructionCircle:
            created = this->documentController->createConstructionCircle(
                    this->shapePageIndex, this->shapeStartScene.x(), this->shapeStartScene.y(),
                    this->shapeCurrentScene.x(), this->shapeCurrentScene.y(), color, width);
            break;
        default:
            break;
    }

    this->shapeDrawing = false;
    this->shapeClickPoints.clear();
    setCursor(Qt::ArrowCursor);

    if (created) {
        Q_EMIT documentEdited();
    }
    update();
}

void QtCanvas::cancelShape() {
    this->shapeDrawing = false;
    this->shapeClickPoints.clear();
    setCursor(Qt::ArrowCursor);
    update();
}

void QtCanvas::drawShapePreview(QPainter& painter) const {
    if (!this->shapeDrawing) {
        return;
    }

    const auto rects = pageRects();
    if (this->shapePageIndex >= rects.size()) {
        return;
    }

    painter.save();
    const auto& pageRect = rects[this->shapePageIndex];
    painter.translate(pageRect.x(), pageRect.y());

    QPen pen(QColor(this->currentToolState.penColor.red, this->currentToolState.penColor.green,
                    this->currentToolState.penColor.blue, 180),
             this->currentToolState.penWidth);
    pen.setStyle(Qt::DashLine);
    painter.setPen(pen);
    painter.setBrush(Qt::NoBrush);

    const QPointF start = this->shapeStartScene;
    const QPointF current = this->shapeCurrentScene;

    switch (this->currentToolState.activeTool) {
        case QtToolType::DrawLine:
        case QtToolType::DrawConstructionLine:
            painter.drawLine(start, current);
            break;
        case QtToolType::DrawRectangle:
            painter.drawRect(QRectF(start, current).normalized());
            break;
        case QtToolType::DrawCircle:
        case QtToolType::DrawConstructionCircle: {
            const double radius = std::hypot(current.x() - start.x(), current.y() - start.y());
            painter.drawEllipse(start, radius, radius);
            break;
        }
        case QtToolType::DrawEllipse:
            painter.drawEllipse(QRectF(start, current).normalized());
            break;
        case QtToolType::DrawArrow:
        case QtToolType::DrawDoubleArrow: {
            const auto points =
                    buildArrowPreviewPoints(start, current, this->currentToolState.penWidth,
                                            this->currentToolState.activeTool == QtToolType::DrawDoubleArrow);
            for (std::size_t i = 1; i < points.size(); ++i) {
                painter.drawLine(points[i - 1], points[i]);
            }
            break;
        }
        case QtToolType::DrawCoordinateSystem: {
            const auto points = buildCoordinateSystemPreviewPoints(start, current);
            for (std::size_t i = 1; i < points.size(); ++i) {
                painter.drawLine(points[i - 1], points[i]);
            }
            break;
        }
        case QtToolType::DrawArc:
            if (this->shapeClickPoints.size() == 1U) {
                painter.drawLine(this->shapeClickPoints[0], current);
            } else if (this->shapeClickPoints.size() >= 2U) {
                const QPointF& center = this->shapeClickPoints[0];
                const QPointF& arcStart = this->shapeClickPoints[1];
                const double radius = std::hypot(arcStart.x() - center.x(), arcStart.y() - center.y());
                painter.drawEllipse(center, radius, radius);
                painter.drawLine(center, current);
            }
            break;
        case QtToolType::DrawPolyline:
            for (std::size_t i = 1; i < this->shapeClickPoints.size(); ++i) {
                painter.drawLine(this->shapeClickPoints[i - 1], this->shapeClickPoints[i]);
            }
            if (!this->shapeClickPoints.empty()) {
                painter.drawLine(this->shapeClickPoints.back(), current);
            }
            break;
        case QtToolType::DrawSpline: {
            std::vector<QPointF> points = this->shapeClickPoints;
            if (points.empty() || points.back() != current) {
                points.push_back(current);
            }
            painter.drawPath(cubicSplinePath(points));
            break;
        }
        default:
            break;
    }

    // Draw vertex handles
    painter.setPen(Qt::NoPen);
    painter.setBrush(QColor(0, 120, 255, 200));
    const double handleRadius = 3.0 / painter.transform().m11();
    for (const auto& pt: this->shapeClickPoints) {
        painter.drawEllipse(pt, handleRadius, handleRadius);
    }

    painter.restore();
}

auto QtCanvas::isMultiClickShapeTool() const -> bool {
    return this->currentToolState.activeTool == QtToolType::DrawPolyline ||
           this->currentToolState.activeTool == QtToolType::DrawArc ||
           this->currentToolState.activeTool == QtToolType::DrawSpline;
}

auto QtCanvas::applyRotationSnap(const QPointF& origin, const QPointF& point) const -> QPointF {
    const QPointF delta = point - origin;
    const double length = std::hypot(delta.x(), delta.y());
    if (length <= 0.0001) {
        return point;
    }

    const double angle = std::atan2(delta.y(), delta.x());
    const double snappedAngle = std::round(angle / ROTATION_SNAP_STEP_RADIANS) * ROTATION_SNAP_STEP_RADIANS;
    return QPointF(origin.x() + std::cos(snappedAngle) * length, origin.y() + std::sin(snappedAngle) * length);
}

void QtCanvas::processTouchDrawing(const vn::ui::input::TouchEvent& event) {
    if (!this->touchDrawingEnabled) {
        return;
    }

    const bool drawTool = this->currentToolState.activeTool == QtToolType::Pen ||
                          this->currentToolState.activeTool == QtToolType::Highlighter;
    if (!drawTool) {
        return;
    }

    if (event.points.empty()) {
        if (this->drawing && this->activeTouchPointId >= 0) {
            finalizeActiveStroke();
            this->activeTouchPointId = -1;
        }
        return;
    }

    const auto* touchPoint = [&]() -> const vn::ui::input::TouchPoint* {
        if (this->activeTouchPointId >= 0) {
            for (const auto& point: event.points) {
                if (point.id == this->activeTouchPointId) {
                    return &point;
                }
            }
        }
        return &event.points.front();
    }();

    if (!touchPoint) {
        return;
    }

    const QPointF screenPoint(touchPoint->x, touchPoint->y);
    const double pressure = touchPoint->pressure > 0.0 ? touchPoint->pressure : 0.5;

    if (!this->drawing) {
        this->activeTouchPointId = touchPoint->id;
        beginStrokeAtScreen(screenPoint, pressure);
        return;
    }

    if (this->activeTouchPointId == touchPoint->id) {
        updateStrokeAtScreen(screenPoint, pressure);
    }
}
