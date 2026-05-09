/*
 * VertexNote
 *
 * Part of the customizable toolbars
 *
 * @author VertexNote Team
 * https://github.com/saitatter/vertex-note
 *
 * @license GNU GPLv2 or later
 */
#pragma once

#include <string>

#include "ComboToolButton.h"

class IconNameHelper;
class ActionDatabase;

class ToolSelectCombocontrol: public ComboToolButton {
public:
    ToolSelectCombocontrol(std::string id, IconNameHelper& icons, const ActionDatabase& db, bool hideAudio);
    ~ToolSelectCombocontrol() override = default;
};
