/*
 * VertexNote
 *
 * A submenu
 *
 * @author VertexNote Team
 * https://github.com/saitatter/vertex-note
 *
 * @license GNU GPLv2 or later
 */

#pragma once

#include "util/raii/GObjectSPtr.h"

class Menubar;

class Submenu {
protected:
    Submenu() = default;
    ~Submenu() noexcept = default;

public:
    virtual void setDisabled(bool disabled) = 0;
    virtual void addToMenubar(Menubar& menubar) = 0;
};
