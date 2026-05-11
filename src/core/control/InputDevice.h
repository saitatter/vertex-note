/*
 * VertexNote
 *
 * Stored input device metadata.
 *
 * @author VertexNote Team
 * https://github.com/saitatter/vertex-note
 *
 * @license GNU GPLv2 or later
 */

#pragma once

#include <string>

enum class InputDeviceSource {
    Mouse = 0,
    Pen = 1,
    Eraser = 2,
    Cursor = 3,
    Keyboard = 4,
    Touchscreen = 5,
    Touchpad = 6,
    Trackpoint = 7,
    TabletPad = 8,
};

class InputDevice {
public:
    InputDevice();
    explicit InputDevice(std::string name, InputDeviceSource source);
    ~InputDevice() = default;

public:
    std::string getType() const;
    std::string getName() const;
    InputDeviceSource getSource() const;
    void updateType(InputDeviceSource newSource);

    bool operator==(const InputDevice& inputDevice) const;

private:
    std::string name;
    InputDeviceSource source{InputDeviceSource::Mouse};
};
