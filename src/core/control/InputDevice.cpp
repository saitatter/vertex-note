#include "control/InputDevice.h"

#include <utility>

#include "util/i18n.h"

InputDevice::InputDevice() = default;

InputDevice::InputDevice(GdkDevice* device): name(gdk_device_get_name(device)), source(gdk_device_get_source(device)) {}

InputDevice::InputDevice(std::string name, GdkInputSource source): name(std::move(name)), source(source) {}

auto InputDevice::getName() const -> std::string { return this->name; }

auto InputDevice::getSource() const -> GdkInputSource { return this->source; }

void InputDevice::updateType(GdkInputSource newSource) { this->source = newSource; }

auto InputDevice::getType() const -> std::string {
    switch (source) {
        case GDK_SOURCE_MOUSE:
            return _("mouse");
        case GDK_SOURCE_PEN:
            return _("pen");
        case GDK_SOURCE_ERASER:
            return _("eraser");
        case GDK_SOURCE_CURSOR:
            return _("cursor");
        case GDK_SOURCE_KEYBOARD:
            return _("keyboard");
        case GDK_SOURCE_TOUCHSCREEN:
            return _("touchscreen");
        case GDK_SOURCE_TOUCHPAD:
            return _("touchpad");
#if (GDK_MAJOR_VERSION >= 3 && GDK_MINOR_VERSION >= 22)
        case GDK_SOURCE_TRACKPOINT:
            return _("trackpoint");
        case GDK_SOURCE_TABLET_PAD:
            return _("tablet pad");
#endif
    }

    return "";
}

auto InputDevice::operator==(const InputDevice& inputDevice) const -> bool {
    return this->getName() == inputDevice.getName();
}
