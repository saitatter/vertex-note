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

#include <gdk/gdk.h>

class InputDevice {
public:
    InputDevice();
    explicit InputDevice(GdkDevice* device);
    explicit InputDevice(std::string name, GdkInputSource source);
    ~InputDevice() = default;

public:
    std::string getType() const;
    std::string getName() const;
    GdkInputSource getSource() const;
    void updateType(GdkInputSource newSource);

    bool operator==(const InputDevice& inputDevice) const;

private:
    std::string name;
    GdkInputSource source{GDK_SOURCE_MOUSE};
};
