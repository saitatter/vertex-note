/*
 * VertexNote
 *
 * Experimental Qt input adapter for the shell migration.
 */

#include "QtInputAdapter.h"

#include <QDateTime>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QTabletEvent>
#include <QTouchEvent>
#include <QWheelEvent>

namespace {

auto pointerDeviceForTablet(QPointingDevice::PointerType pointerType) -> vn::ui::input::PointerDeviceKind {
    using vn::ui::input::PointerDeviceKind;

    switch (pointerType) {
        case QPointingDevice::PointerType::Pen:
            return PointerDeviceKind::Pen;
        case QPointingDevice::PointerType::Eraser:
            return PointerDeviceKind::Eraser;
        case QPointingDevice::PointerType::Finger:
            return PointerDeviceKind::Touch;
        default:
            return PointerDeviceKind::Unknown;
    }
}

}  // namespace

void QtInputAdapter::handleMousePress(const QMouseEvent& event) const {
    if (!this->sink) {
        return;
    }
    this->sink->handlePointerEvent({.type = vn::ui::input::PointerEventType::Press,
                                    .device = vn::ui::input::PointerDeviceKind::Mouse,
                                    .button = translateButton(event.button()),
                                    .modifiers = translateModifiers(event.modifiers()),
                                    .x = event.position().x(),
                                    .y = event.position().y(),
                                    .timestampMs = nowTimestampMs()});
}

void QtInputAdapter::handleMouseRelease(const QMouseEvent& event) const {
    if (!this->sink) {
        return;
    }
    this->sink->handlePointerEvent({.type = vn::ui::input::PointerEventType::Release,
                                    .device = vn::ui::input::PointerDeviceKind::Mouse,
                                    .button = translateButton(event.button()),
                                    .modifiers = translateModifiers(event.modifiers()),
                                    .x = event.position().x(),
                                    .y = event.position().y(),
                                    .timestampMs = nowTimestampMs()});
}

void QtInputAdapter::handleMouseMove(const QMouseEvent& event) const {
    if (!this->sink) {
        return;
    }
    this->sink->handlePointerEvent({.type = vn::ui::input::PointerEventType::Move,
                                    .device = vn::ui::input::PointerDeviceKind::Mouse,
                                    .button = translateButton(event.button()),
                                    .modifiers = translateModifiers(event.modifiers()),
                                    .x = event.position().x(),
                                    .y = event.position().y(),
                                    .timestampMs = nowTimestampMs()});
}

void QtInputAdapter::handleWheel(const QWheelEvent& event) const {
    if (!this->sink) {
        return;
    }
    this->sink->handlePointerEvent({.type = vn::ui::input::PointerEventType::Scroll,
                                    .device = vn::ui::input::PointerDeviceKind::Mouse,
                                    .modifiers = translateModifiers(event.modifiers()),
                                    .x = event.position().x(),
                                    .y = event.position().y(),
                                    .deltaX = static_cast<double>(event.angleDelta().x()),
                                    .deltaY = static_cast<double>(event.angleDelta().y()),
                                    .timestampMs = nowTimestampMs()});
}

void QtInputAdapter::handleTablet(const QTabletEvent& event) const {
    if (!this->sink) {
        return;
    }
    this->sink->handlePointerEvent({.type = event.type() == QEvent::TabletPress ? vn::ui::input::PointerEventType::Press
                                                                                : event.type() == QEvent::TabletRelease
                                                                                          ? vn::ui::input::PointerEventType::Release
                                                                                          : vn::ui::input::PointerEventType::Move,
                                    .device = pointerDeviceForTablet(event.pointingDevice()->pointerType()),
                                    .button = translateButton(event.button()),
                                    .modifiers = translateModifiers(event.modifiers()),
                                    .x = event.position().x(),
                                    .y = event.position().y(),
                                    .pressure = event.pressure(),
                                    .timestampMs = nowTimestampMs()});
}

void QtInputAdapter::handleKeyPress(const QKeyEvent& event) const {
    if (!this->sink) {
        return;
    }
    this->sink->handleKeyboardEvent({.type = vn::ui::input::KeyboardEventType::Press,
                                     .modifiers = translateModifiers(event.modifiers()),
                                     .key = static_cast<std::uint32_t>(event.key()),
                                     .text = event.text().toStdString(),
                                     .autoRepeat = event.isAutoRepeat(),
                                     .timestampMs = nowTimestampMs()});
}

void QtInputAdapter::handleKeyRelease(const QKeyEvent& event) const {
    if (!this->sink) {
        return;
    }
    this->sink->handleKeyboardEvent({.type = vn::ui::input::KeyboardEventType::Release,
                                     .modifiers = translateModifiers(event.modifiers()),
                                     .key = static_cast<std::uint32_t>(event.key()),
                                     .text = event.text().toStdString(),
                                     .autoRepeat = event.isAutoRepeat(),
                                     .timestampMs = nowTimestampMs()});
}

void QtInputAdapter::handleTouch(const QTouchEvent& event) const {
    if (!this->sink) {
        return;
    }
    vn::ui::input::TouchEvent translated;
    translated.modifiers = translateModifiers(event.modifiers());
    translated.timestampMs = nowTimestampMs();
    translated.points.reserve(event.points().size());
    for (const auto& point: event.points()) {
        translated.points.push_back({.id = point.id(),
                                     .x = point.position().x(),
                                     .y = point.position().y(),
                                     .pressure = point.pressure()});
    }
    this->sink->handleTouchEvent(translated);
}

auto QtInputAdapter::translateModifiers(Qt::KeyboardModifiers modifiers) const -> vn::ui::input::Modifiers {
    return {.shift = modifiers.testFlag(Qt::ShiftModifier),
            .control = modifiers.testFlag(Qt::ControlModifier),
            .alt = modifiers.testFlag(Qt::AltModifier),
            .meta = modifiers.testFlag(Qt::MetaModifier)};
}

auto QtInputAdapter::translateButton(Qt::MouseButton button) const -> vn::ui::input::MouseButton {
    using vn::ui::input::MouseButton;
    switch (button) {
        case Qt::LeftButton:
            return MouseButton::Left;
        case Qt::RightButton:
            return MouseButton::Right;
        case Qt::MiddleButton:
            return MouseButton::Middle;
        case Qt::BackButton:
            return MouseButton::Back;
        case Qt::ForwardButton:
            return MouseButton::Forward;
        case Qt::NoButton:
            return MouseButton::None;
        default:
            return MouseButton::Other;
    }
}

auto QtInputAdapter::nowTimestampMs() const -> std::uint64_t {
    return static_cast<std::uint64_t>(QDateTime::currentMSecsSinceEpoch());
}
