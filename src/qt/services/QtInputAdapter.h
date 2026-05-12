/*
 * VertexNote
 *
 * Experimental Qt input adapter for the shell migration.
 */

#pragma once

#include <Qt>

#include "ui/input/UiInputEvents.h"

class QKeyEvent;
class QMouseEvent;
class QTabletEvent;
class QTouchEvent;
class QWheelEvent;

class QtInputAdapter {
public:
    explicit QtInputAdapter(vn::ui::input::IInputEventSink* sink): sink(sink) {}

public:
    void handleMousePress(const QMouseEvent& event) const;
    void handleMouseRelease(const QMouseEvent& event) const;
    void handleMouseMove(const QMouseEvent& event) const;
    void handleWheel(const QWheelEvent& event) const;
    void handleTablet(const QTabletEvent& event) const;
    void handleKeyPress(const QKeyEvent& event) const;
    void handleKeyRelease(const QKeyEvent& event) const;
    void handleTouch(const QTouchEvent& event) const;

private:
    [[nodiscard]] auto translateModifiers(Qt::KeyboardModifiers modifiers) const -> vn::ui::input::Modifiers;
    [[nodiscard]] auto translateButton(Qt::MouseButton button) const -> vn::ui::input::MouseButton;
    [[nodiscard]] auto nowTimestampMs() const -> std::uint64_t;

private:
    vn::ui::input::IInputEventSink* sink = nullptr;
};
