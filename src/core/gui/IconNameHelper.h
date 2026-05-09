/*
 * VertexNote
 *
 * Helper which allows to switch between VertexNote icons and stock icons
 *
 * @author VertexNote Team
 * https://github.com/saitatter/vertex-note
 *
 * @license GNU GPLv2 or later
 */
#pragma once

#include <string>

class Settings;

class IconNameHelper final {

public:
    IconNameHelper(Settings* settings);

protected:
    const Settings* settings;

public:
    std::string iconName(const char* icon) const;
};
