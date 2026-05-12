/*
 * VertexNote
 *
 * Qt input device identity helpers.
 */

#pragma once

#include <QString>

#include <QInputDevice>
#include <QPointingDevice>

inline auto qtInputDeviceTypeName(const QInputDevice* device) -> QString {
    if (!device) {
        return QStringLiteral("unknown");
    }

    if (const auto* pointing = dynamic_cast<const QPointingDevice*>(device)) {
        switch (pointing->pointerType()) {
            case QPointingDevice::PointerType::Generic:
                return QStringLiteral("pointer");
            case QPointingDevice::PointerType::Finger:
                return QStringLiteral("touch");
            case QPointingDevice::PointerType::Pen:
                return QStringLiteral("pen");
            case QPointingDevice::PointerType::Eraser:
                return QStringLiteral("eraser");
            case QPointingDevice::PointerType::Cursor:
                return QStringLiteral("cursor");
            default:
                break;
        }
    }

    switch (device->type()) {
        case QInputDevice::DeviceType::Mouse:
            return QStringLiteral("mouse");
        case QInputDevice::DeviceType::TouchScreen:
            return QStringLiteral("touchscreen");
        case QInputDevice::DeviceType::TouchPad:
            return QStringLiteral("touchpad");
        case QInputDevice::DeviceType::Puck:
            return QStringLiteral("puck");
        case QInputDevice::DeviceType::Stylus:
            return QStringLiteral("stylus");
        case QInputDevice::DeviceType::Airbrush:
            return QStringLiteral("airbrush");
        case QInputDevice::DeviceType::Keyboard:
            return QStringLiteral("keyboard");
        default:
            return QStringLiteral("device");
    }
}

inline auto qtInputDeviceKey(const QInputDevice* device) -> QString {
    if (!device) {
        return QString();
    }

    int pointerType = -1;
    if (const auto* pointing = dynamic_cast<const QPointingDevice*>(device)) {
        pointerType = static_cast<int>(pointing->pointerType());
    }

    return QStringLiteral("%1|%2|%3|%4")
            .arg(QString::number(device->systemId()), QString::number(static_cast<int>(device->type())),
                 QString::number(pointerType), device->name());
}
