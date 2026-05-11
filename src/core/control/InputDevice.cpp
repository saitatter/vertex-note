#include "control/InputDevice.h"

#include <utility>

#include "util/i18n.h"

InputDevice::InputDevice() = default;

InputDevice::InputDevice(std::string name, InputDeviceSource source): name(std::move(name)), source(source) {}

auto InputDevice::getName() const -> std::string { return this->name; }

auto InputDevice::getSource() const -> InputDeviceSource { return this->source; }

void InputDevice::updateType(InputDeviceSource newSource) { this->source = newSource; }

auto InputDevice::getType() const -> std::string {
    switch (source) {
        case InputDeviceSource::Mouse:
            return _("mouse");
        case InputDeviceSource::Pen:
            return _("pen");
        case InputDeviceSource::Eraser:
            return _("eraser");
        case InputDeviceSource::Cursor:
            return _("cursor");
        case InputDeviceSource::Keyboard:
            return _("keyboard");
        case InputDeviceSource::Touchscreen:
            return _("touchscreen");
        case InputDeviceSource::Touchpad:
            return _("touchpad");
        case InputDeviceSource::Trackpoint:
            return _("trackpoint");
        case InputDeviceSource::TabletPad:
            return _("tablet pad");
    }

    return "";
}

auto InputDevice::operator==(const InputDevice& inputDevice) const -> bool {
    return this->getName() == inputDevice.getName();
}
