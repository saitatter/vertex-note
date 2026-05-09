/*
 * VertexNote
 *
 * Experimental Qt canvas bootstrap.
 */

#include "QtExperimentalCanvas.h"

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
#include <QPalette>
#include <QPen>
#include <QRect>
#include <QTabletEvent>
#include <QTouchEvent>
#include <QTransform>
#include <QStringList>
#include <QWheelEvent>

#include "QtPreviewBackgroundRenderer.h"
#include "QtPreviewGeometryRenderer.h"
#include "QtPreviewImageRenderer.h"
#include "QtPreviewStrokeRenderer.h"
#include "QtPreviewTextRenderer.h"
#include "view/render/QtPainterRenderContext.h"

namespace {

constexpr double MIN_ZOOM = 0.1;
constexpr double MAX_ZOOM = 8.0;
constexpr double ZOOM_STEP = 1.15;
constexpr double PAGE_STACK_X = 120.0;
constexpr double PAGE_STACK_Y = 100.0;
constexpr double PAGE_STACK_GAP = 56.0;

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

}  // namespace

QtExperimentalCanvas::QtExperimentalCanvas(QWidget* parent): QWidget(parent) {
    setObjectName("qtExperimentalCanvas");
    setMinimumSize(960, 640);
    setAutoFillBackground(true);
    setMouseTracking(true);
    setFocusPolicy(Qt::StrongFocus);
    setAttribute(Qt::WA_AcceptTouchEvents, true);
    auto palette = this->palette();
    palette.setColor(QPalette::Window, QColor(236, 241, 247));
    setPalette(palette);
    this->inputAdapter = std::make_unique<QtInputAdapter>(this);
    this->backgroundRenderer = std::make_unique<vn::view::render::QtPreviewBackgroundRenderer>();
    this->geometryRenderer = std::make_unique<vn::view::render::QtPreviewGeometryRenderer>();
    this->imageRenderer = std::make_unique<vn::view::render::QtPreviewImageRenderer>();
    this->strokeRenderer = std::make_unique<vn::view::render::QtPreviewStrokeRenderer>();
    this->textRenderer = std::make_unique<vn::view::render::QtPreviewTextRenderer>();
    newBlankDocument();
}

void QtExperimentalCanvas::invalidateCanvas() { update(); }

void QtExperimentalCanvas::invalidateRect(double x, double y, double width, double height) {
    update(QRect(static_cast<int>(x), static_cast<int>(y), static_cast<int>(width), static_cast<int>(height)));
}

void QtExperimentalCanvas::setCanvasCursor(vn::ui::common::CanvasCursor cursor) {
    setCursor(QCursor(toQtCursor(cursor)));
}

auto QtExperimentalCanvas::viewport() const -> vn::ui::common::CanvasViewport {
    return {.zoom = this->zoomFactor,
            .scrollX = this->scrollX,
            .scrollY = this->scrollY,
            .width = static_cast<double>(width()),
            .height = static_cast<double>(height()),
            .devicePixelRatio = devicePixelRatioF()};
}

void QtExperimentalCanvas::handlePointerEvent(const vn::ui::input::PointerEvent& event) {
    updateDebugOverlay(QStringLiteral("pointer x=%1 y=%2 pressure=%3")
                               .arg(event.x, 0, 'f', 1)
                               .arg(event.y, 0, 'f', 1)
                               .arg(event.pressure, 0, 'f', 2));
}

void QtExperimentalCanvas::handleKeyboardEvent(const vn::ui::input::KeyboardEvent& event) {
    updateDebugOverlay(QStringLiteral("key code=%1 text=%2").arg(event.key).arg(QString::fromStdString(event.text)));
}

void QtExperimentalCanvas::handleTouchEvent(const vn::ui::input::TouchEvent& event) {
    updateDebugOverlay(QStringLiteral("touch points=%1").arg(static_cast<int>(event.points.size())));
}

void QtExperimentalCanvas::setDocumentController(const QtExperimentalDocumentController* documentController) {
    this->documentController = documentController;
    fitPage(false);
}

void QtExperimentalCanvas::newBlankDocument() {
    this->zoomFactor = 1.0;
    this->scrollX = 0.0;
    this->scrollY = 0.0;
    fitPage(false);
    updateDebugOverlay(QStringLiteral("new experimental document"));
}

void QtExperimentalCanvas::setViewportState(double zoom, double scrollX, double scrollY) {
    this->zoomFactor = clampZoom(zoom);
    this->scrollX = scrollX;
    this->scrollY = scrollY;
    emitViewportUpdate(false);
}

auto QtExperimentalCanvas::sessionViewportState() const -> QtExperimentalViewportState {
    return {.zoom = this->zoomFactor, .scrollX = this->scrollX, .scrollY = this->scrollY};
}

void QtExperimentalCanvas::zoomIn() { zoomAroundScreenPoint(ZOOM_STEP, rect().center()); }

void QtExperimentalCanvas::zoomOut() { zoomAroundScreenPoint(1.0 / ZOOM_STEP, rect().center()); }

void QtExperimentalCanvas::resetViewport() {
    fitPage();
    updateDebugOverlay(QStringLiteral("viewport reset"));
}

void QtExperimentalCanvas::fitPage(bool edited) {
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

void QtExperimentalCanvas::panBy(double dx, double dy) {
    this->scrollX += dx;
    this->scrollY += dy;
    emitViewportUpdate();
}

void QtExperimentalCanvas::paintEvent(QPaintEvent* event) {
    QWidget::paintEvent(event);

    QPainter painter(this);
    vn::view::render::QtPainterRenderContext renderContext(&painter, devicePixelRatioF());
    painter.fillRect(rect(), palette().window());

    QTransform viewTransform;
    viewTransform.translate(-this->scrollX * this->zoomFactor, -this->scrollY * this->zoomFactor);
    viewTransform.scale(this->zoomFactor, this->zoomFactor);
    painter.setTransform(viewTransform);

    painter.setRenderHint(QPainter::Antialiasing, false);
    const QColor sceneGridMinor(214, 223, 235);
    const QColor sceneGridMajor(188, 200, 216);
    constexpr int sceneStep = 48;
    const QRectF visibleScene(this->scrollX, this->scrollY, width() / this->zoomFactor, height() / this->zoomFactor);
    const int startX = static_cast<int>(std::floor(visibleScene.left() / sceneStep)) * sceneStep;
    const int endX = static_cast<int>(std::ceil(visibleScene.right() / sceneStep)) * sceneStep;
    const int startY = static_cast<int>(std::floor(visibleScene.top() / sceneStep)) * sceneStep;
    const int endY = static_cast<int>(std::ceil(visibleScene.bottom() / sceneStep)) * sceneStep;
    for (int x = startX, index = 0; x <= endX; x += sceneStep, ++index) {
        painter.setPen((index % 4) == 0 ? sceneGridMajor : sceneGridMinor);
        painter.drawLine(QPointF(x, visibleScene.top()), QPointF(x, visibleScene.bottom()));
    }
    for (int y = startY, index = 0; y <= endY; y += sceneStep, ++index) {
        painter.setPen((index % 4) == 0 ? sceneGridMajor : sceneGridMinor);
        painter.drawLine(QPointF(visibleScene.left(), y), QPointF(visibleScene.right(), y));
    }

    painter.setRenderHint(QPainter::Antialiasing, true);
    const auto pages = this->documentController ? this->documentController->snapshotPages() : std::vector<QtExperimentalPageInfo>{};
    const auto rects = pageRects();
    for (std::size_t index = 0; index < rects.size(); ++index) {
        drawPageContents(painter, rects[index], index < pages.size() ? pages[index] : QtExperimentalPageInfo{}, index);
    }

    painter.resetTransform();
    painter.setPen(QColor(52, 64, 84));
    painter.drawText(QRect(20, 18, width() - 40, 72), Qt::AlignLeft | Qt::AlignTop,
                     QStringLiteral("VertexNote Qt Experimental Shell"));
    painter.setPen(QColor(102, 112, 133));
    const auto pageCount = this->documentController ? this->documentController->pageCount() : rects.size();
    painter.drawText(QRect(20, 52, width() - 40, 72), Qt::AlignLeft | Qt::AlignTop,
                     QStringLiteral("render backend=QtPainter scale=%1  zoom=%2%  scroll=(%3, %4)  pages=%5")
                             .arg(renderContext.scaleFactor(), 0, 'f', 2)
                             .arg(this->zoomFactor * 100.0, 0, 'f', 0)
                             .arg(this->scrollX, 0, 'f', 1)
                             .arg(this->scrollY, 0, 'f', 1)
                             .arg(static_cast<int>(pageCount)));
    painter.drawText(QRect(20, 78, width() - 40, 40), Qt::AlignLeft | Qt::AlignTop, this->lastEventSummary);
    drawOverlayHud(painter);

    if (event) {
        event->accept();
    }
}

void QtExperimentalCanvas::mousePressEvent(QMouseEvent* event) {
    this->inputAdapter->handleMousePress(*event);
    if (event->button() == Qt::MiddleButton || (event->button() == Qt::LeftButton && this->spaceHeld)) {
        beginPan(event->position());
        event->accept();
        return;
    }
    QWidget::mousePressEvent(event);
}

void QtExperimentalCanvas::mouseReleaseEvent(QMouseEvent* event) {
    this->inputAdapter->handleMouseRelease(*event);
    if (this->panning && (event->button() == Qt::MiddleButton || event->button() == Qt::LeftButton)) {
        endPan();
        event->accept();
        return;
    }
    QWidget::mouseReleaseEvent(event);
}

void QtExperimentalCanvas::mouseMoveEvent(QMouseEvent* event) {
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
    QWidget::mouseMoveEvent(event);
}

void QtExperimentalCanvas::wheelEvent(QWheelEvent* event) {
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

void QtExperimentalCanvas::tabletEvent(QTabletEvent* event) {
    this->inputAdapter->handleTablet(*event);
    QWidget::tabletEvent(event);
}

void QtExperimentalCanvas::keyPressEvent(QKeyEvent* event) {
    this->inputAdapter->handleKeyPress(*event);
    if (!event->isAutoRepeat() && event->key() == Qt::Key_Space) {
        this->spaceHeld = true;
        setCursor(Qt::OpenHandCursor);
    }
    QWidget::keyPressEvent(event);
}

void QtExperimentalCanvas::keyReleaseEvent(QKeyEvent* event) {
    this->inputAdapter->handleKeyRelease(*event);
    if (!event->isAutoRepeat() && event->key() == Qt::Key_Space) {
        this->spaceHeld = false;
        if (!this->panning) {
            unsetCursor();
        }
    }
    QWidget::keyReleaseEvent(event);
}

bool QtExperimentalCanvas::event(QEvent* event) {
    if (event && (event->type() == QEvent::TouchBegin || event->type() == QEvent::TouchUpdate ||
                  event->type() == QEvent::TouchEnd)) {
        this->inputAdapter->handleTouch(*static_cast<QTouchEvent*>(event));
    }
    return QWidget::event(event);
}

void QtExperimentalCanvas::updateDebugOverlay(QString summary) {
    this->lastEventSummary = std::move(summary);
    update();
}

void QtExperimentalCanvas::emitViewportUpdate(bool edited) {
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

void QtExperimentalCanvas::zoomAroundScreenPoint(double factor, const QPointF& screenPoint) {
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

auto QtExperimentalCanvas::pageRects() const -> std::vector<QRectF> {
    std::vector<QRectF> rects;
    const auto pages = this->documentController ? this->documentController->snapshotPages() : std::vector<QtExperimentalPageInfo>{};
    if (pages.empty()) {
        rects.emplace_back(PAGE_STACK_X, PAGE_STACK_Y, 1100.0, 1500.0);
        return rects;
    }

    rects.reserve(pages.size());
    double currentY = PAGE_STACK_Y;
    for (const auto& page: pages) {
        rects.emplace_back(PAGE_STACK_X, currentY, std::max(page.width, 1.0), std::max(page.height, 1.0));
        currentY += std::max(page.height, 1.0) + PAGE_STACK_GAP;
    }
    return rects;
}

auto QtExperimentalCanvas::documentSceneBounds() const -> QRectF {
    const auto rects = pageRects();
    QRectF bounds = rects.front();
    for (std::size_t i = 1; i < rects.size(); ++i) {
        bounds = bounds.united(rects[i]);
    }
    return bounds.adjusted(-80.0, -80.0, 80.0, 80.0);
}

void QtExperimentalCanvas::drawPageContents(QPainter& painter, const QRectF& rect, const QtExperimentalPageInfo& pageInfo,
                                            std::size_t pageIndex) const {
    if (this->backgroundRenderer) {
        vn::view::render::QtPainterRenderContext renderContext(&painter, this->zoomFactor);
        const vn::view::render::RenderRect renderRect{
                .x = rect.x(),
                .y = rect.y(),
                .width = rect.width(),
                .height = rect.height(),
        };
        this->backgroundRenderer->draw(pageInfo.background, renderRect, renderContext);
        for (const auto& drawable: pageInfo.drawables) {
            std::visit(
                    [this, &renderContext](const auto& model) {
                        using Model = std::decay_t<decltype(model)>;
                        if constexpr (std::is_same_v<Model, vn::view::render::StrokeRenderModel>) {
                            if (this->strokeRenderer) {
                                this->strokeRenderer->draw(model, renderContext);
                            }
                        } else if constexpr (std::is_same_v<Model, vn::view::render::TextRenderModel>) {
                            if (this->textRenderer) {
                                this->textRenderer->draw(model, renderContext);
                            }
                        } else if constexpr (std::is_same_v<Model, vn::view::render::ImageRenderModel>) {
                            if (this->imageRenderer) {
                                this->imageRenderer->draw(model, renderContext);
                            }
                        } else if constexpr (std::is_same_v<Model, vn::view::render::GeometryRenderModel>) {
                            if (this->geometryRenderer) {
                                this->geometryRenderer->draw(model, renderContext);
                            }
                        }
                    },
                    drawable);
        }
    }

    painter.setPen(QColor(61, 74, 89));
    painter.drawText(QRectF(rect.left() + 26.0, rect.top() + 18.0, rect.width() - 52.0, 30.0), Qt::AlignLeft | Qt::AlignVCenter,
                     QStringLiteral("Page %1").arg(static_cast<int>(pageIndex + 1)));

    if (pageInfo.background.hasBackgroundName) {
        painter.setPen(QColor(120, 131, 146));
        painter.drawText(QRectF(rect.left() + 26.0, rect.top() + rect.height() - 48.0, rect.width() - 52.0, 24.0),
                         Qt::AlignLeft | Qt::AlignVCenter,
                         QString::fromStdString(pageInfo.background.backgroundName));
    }

    painter.setPen(QColor(102, 112, 133));
    painter.drawText(QRectF(rect.left() + 26.0, rect.top() + rect.height() - 74.0, rect.width() - 52.0, 24.0),
                     Qt::AlignLeft | Qt::AlignVCenter,
                     QStringLiteral("Layers %1  |  Annotated %2")
                             .arg(static_cast<int>(pageInfo.background.layerCount))
                             .arg(pageInfo.background.annotated ? QStringLiteral("yes") : QStringLiteral("no")));
}

void QtExperimentalCanvas::drawOverlayHud(QPainter& painter) const {
    const auto pages = this->documentController ? this->documentController->snapshotPages() : std::vector<QtExperimentalPageInfo>{};
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
            QStringLiteral("Qt experimental"),
            QStringLiteral("pages %1").arg(static_cast<int>(pages.size())),
            QStringLiteral("drawables %1").arg(static_cast<int>(drawableCount)),
            QStringLiteral("geometry %1").arg(static_cast<int>(geometryCount)),
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

void QtExperimentalCanvas::beginPan(const QPointF& position) {
    this->panning = true;
    this->lastPanScreenPosition = position;
    setCursor(Qt::ClosedHandCursor);
}

void QtExperimentalCanvas::endPan() {
    this->panning = false;
    if (this->spaceHeld) {
        setCursor(Qt::OpenHandCursor);
    } else {
        unsetCursor();
    }
}
