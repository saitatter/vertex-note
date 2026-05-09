/*
 * VertexNote
 *
 * Platform-neutral UI input events for shell migration.
 */

#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace vn::ui::input {

enum class PointerEventType { Press, Release, Move, Enter, Leave, Scroll };

enum class PointerDeviceKind { Mouse, Pen, Eraser, Touch, Unknown };

enum class MouseButton { None, Left, Right, Middle, Back, Forward, Other };

enum class KeyboardEventType { Press, Release };

struct Modifiers {
    bool shift = false;
    bool control = false;
    bool alt = false;
    bool meta = false;
};

struct PointerEvent {
    PointerEventType type = PointerEventType::Move;
    PointerDeviceKind device = PointerDeviceKind::Unknown;
    MouseButton button = MouseButton::None;
    Modifiers modifiers{};
    double x = 0.0;
    double y = 0.0;
    double deltaX = 0.0;
    double deltaY = 0.0;
    double pressure = 0.0;
    std::uint64_t timestampMs = 0;
};

struct KeyboardEvent {
    KeyboardEventType type = KeyboardEventType::Press;
    Modifiers modifiers{};
    std::uint32_t key = 0;
    std::string text;
    bool autoRepeat = false;
    std::uint64_t timestampMs = 0;
};

struct TouchPoint {
    std::int64_t id = 0;
    double x = 0.0;
    double y = 0.0;
    double pressure = 0.0;
};

struct TouchEvent {
    std::vector<TouchPoint> points;
    Modifiers modifiers{};
    std::uint64_t timestampMs = 0;
};

class IInputEventSink {
public:
    virtual ~IInputEventSink() = default;

    virtual void handlePointerEvent(const PointerEvent& event) = 0;
    virtual void handleKeyboardEvent(const KeyboardEvent& event) = 0;
    virtual void handleTouchEvent(const TouchEvent& event) = 0;
};

}  // namespace vn::ui::input
