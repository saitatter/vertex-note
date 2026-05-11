/*
 * VertexNote
 *
 * Helper functions to iterate over devices
 *
 * @author VertexNote Team
 * https://github.com/saitatter/vertex-note
 *
 * @license GNU GPLv2 or later
 */

#pragma once

#include <vector>  // for vector

#include "control/InputDevice.h"          // for InputDevice
#include "gui/inputdevices/InputEvents.h"  // for InputDeviceClass

class Settings;

namespace DeviceListHelper {
std::vector<InputDevice> getDeviceList(Settings* settings, bool ignoreTouchDevices = false);
InputDeviceClass getSourceMapping(GdkInputSource source, Settings* settings);
}  // namespace DeviceListHelper
