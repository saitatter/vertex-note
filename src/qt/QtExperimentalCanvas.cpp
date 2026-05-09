/*
 * VertexNote
 *
 * Experimental Qt canvas bootstrap.
 */

#include "QtExperimentalCanvas.h"

#include <QEvent>
#include <QCursor>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QPaintEvent>
#include <QPainter>
#include <QPalette>
#include <QPen>
#include <QRect>
#include <QTabletEvent>
#include <QTouchEvent>
#include <QWheelEvent>

#include "QtInputAdapter.h"
#include "view/render/QtPainterRenderContext.h"

namespace {

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

}  // namespace

QtExperimentalCanvas::QtExperimentalCanvas(QWidget* parent): QWidget(parent) {
    setObjectName("qtExperimentalCanvas");
    setMinimumSize(960, 640);
    setAutoFillBackground(true);
    setMouseTracking(true);
    setFocusPolicy(Qt::StrongFocus);
    setAttribute(Qt::WA_AcceptTouchEvents, true);
    auto palette = this->palette();
    palette.setColor(QPalette::Window, QColor(250, 250, 248));
    setPalette(palette);
    this->inputAdapter = std::make_unique<QtInputAdapter>(this);
}

void QtExperimentalCanvas::invalidateCanvas() { update(); }

void QtExperimentalCanvas::invalidateRect(double x, double y, double width, double height) {
    update(QRect(static_cast<int>(x), static_cast<int>(y), static_cast<int>(width), static_cast<int>(height)));
}

void QtExperimentalCanvas::setCanvasCursor(vn::ui::common::CanvasCursor cursor) {
    setCursor(QCursor(toQtCursor(cursor)));
}

auto QtExperimentalCanvas::viewport() const -> vn::ui::common::CanvasViewport {
    return {.zoom = 1.0,
            .scrollX = 0.0,
            .scrollY = 0.0,
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

void QtExperimentalCanvas::paintEvent(QPaintEvent* event) {
    QWidget::paintEvent(event);

    QPainter painter(this);
    vn::view::render::QtPainterRenderContext renderContext(&painter, devicePixelRatioF());
    painter.fillRect(rect(), palette().window());
    painter.setRenderHint(QPainter::Antialiasing, false);

    const QColor minorGrid(224, 231, 239);
    const QColor majorGrid(198, 210, 224);
    const int minorStep = 24;
    const int majorEvery = 5;

    for (int x = 0, column = 0; x <= width(); x += minorStep, ++column) {
        painter.setPen((column % majorEvery) == 0 ? majorGrid : minorGrid);
        painter.drawLine(x, 0, x, height());
    }
    for (int y = 0, row = 0; y <= height(); y += minorStep, ++row) {
        painter.setPen((row % majorEvery) == 0 ? majorGrid : minorGrid);
        painter.drawLine(0, y, width(), y);
    }

    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setPen(QColor(52, 64, 84));
    painter.drawText(QRect(24, 24, width() - 48, 96), Qt::AlignLeft | Qt::AlignTop,
                     QStringLiteral("VertexNote Qt Experimental Shell\nCanvas bootstrap active"));
    painter.setPen(QColor(102, 112, 133));
    painter.drawText(QRect(24, 88, width() - 48, 64), Qt::AlignLeft | Qt::AlignTop,
                     QStringLiteral("render backend=QtPainter scale=%1").arg(renderContext.scaleFactor(), 0, 'f', 2));
    painter.drawText(QRect(24, 120, width() - 48, 64), Qt::AlignLeft | Qt::AlignTop, this->lastEventSummary);

    if (event) {
        event->accept();
    }
}

void QtExperimentalCanvas::mousePressEvent(QMouseEvent* event) {
    this->inputAdapter->handleMousePress(*event);
    QWidget::mousePressEvent(event);
}

void QtExperimentalCanvas::mouseReleaseEvent(QMouseEvent* event) {
    this->inputAdapter->handleMouseRelease(*event);
    QWidget::mouseReleaseEvent(event);
}

void QtExperimentalCanvas::mouseMoveEvent(QMouseEvent* event) {
    this->inputAdapter->handleMouseMove(*event);
    QWidget::mouseMoveEvent(event);
}

void QtExperimentalCanvas::wheelEvent(QWheelEvent* event) {
    this->inputAdapter->handleWheel(*event);
    QWidget::wheelEvent(event);
}

void QtExperimentalCanvas::tabletEvent(QTabletEvent* event) {
    this->inputAdapter->handleTablet(*event);
    QWidget::tabletEvent(event);
}

void QtExperimentalCanvas::keyPressEvent(QKeyEvent* event) {
    this->inputAdapter->handleKeyPress(*event);
    QWidget::keyPressEvent(event);
}

void QtExperimentalCanvas::keyReleaseEvent(QKeyEvent* event) {
    this->inputAdapter->handleKeyRelease(*event);
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
