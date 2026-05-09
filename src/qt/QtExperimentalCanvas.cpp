/*
 * VertexNote
 *
 * Experimental Qt canvas bootstrap.
 */

#include "QtExperimentalCanvas.h"

#include <QCursor>
#include <QPaintEvent>
#include <QPainter>
#include <QPalette>
#include <QPen>
#include <QRect>

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
    auto palette = this->palette();
    palette.setColor(QPalette::Window, QColor(250, 250, 248));
    setPalette(palette);
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

void QtExperimentalCanvas::paintEvent(QPaintEvent* event) {
    QWidget::paintEvent(event);

    QPainter painter(this);
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

    if (event) {
        event->accept();
    }
}
