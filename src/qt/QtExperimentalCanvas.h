/*
 * VertexNote
 *
 * Experimental Qt canvas bootstrap.
 */

#pragma once

#include <memory>

#include <QWidget>

#include "ui/common/ICanvasHost.h"
#include "ui/input/UiInputEvents.h"

class QtInputAdapter;

class QtExperimentalCanvas: public QWidget, public vn::ui::common::ICanvasHost, public vn::ui::input::IInputEventSink {
public:
    explicit QtExperimentalCanvas(QWidget* parent = nullptr);

public:
    void invalidateCanvas() override;
    void invalidateRect(double x, double y, double width, double height) override;
    void setCanvasCursor(vn::ui::common::CanvasCursor cursor) override;
    [[nodiscard]] auto viewport() const -> vn::ui::common::CanvasViewport override;
    void handlePointerEvent(const vn::ui::input::PointerEvent& event) override;
    void handleKeyboardEvent(const vn::ui::input::KeyboardEvent& event) override;
    void handleTouchEvent(const vn::ui::input::TouchEvent& event) override;

protected:
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void wheelEvent(QWheelEvent* event) override;
    void tabletEvent(QTabletEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;
    void keyReleaseEvent(QKeyEvent* event) override;
    bool event(QEvent* event) override;

private:
    void updateDebugOverlay(QString summary);

private:
    std::unique_ptr<QtInputAdapter> inputAdapter;
    QString lastEventSummary;
};
