/*
 * VertexNote
 *
 * Handles page selection menu
 *
 * @author VertexNote Team
 * https://github.com/saitatter/vertex-note
 *
 * @license GNU GPLv2 or later
 */

#pragma once

#include <gio/gio.h>  // for GMenu, GSimpleAction

#include "gui/menus/PageTypeSelectionMenuBase.h"
#include "util/raii/GObjectSPtr.h"

#include "AbstractSubmenu.h"

class PageTypeHandler;
class Settings;
class PageBackgroundChangeController;

class PageTypeSubmenu final: public Submenu, public PageTypeSelectionMenuBase {
public:
    PageTypeSubmenu(PageTypeHandler* typesHandler, PageBackgroundChangeController* controller, const Settings* settings,
                    GtkApplicationWindow* win);
    ~PageTypeSubmenu() = default;

    void setDisabled(bool disabled) override;
    void addToMenubar(Menubar& menubar) override;

private:
    void entrySelected(const PageTypeInfo*) override;

    PageBackgroundChangeController* controller;

    vn::util::GObjectSPtr<GMenu> generatedPageTypesSection;
    vn::util::GObjectSPtr<GMenu> specialPageTypesSection;
    vn::util::GObjectSPtr<GMenu> applyToAllPagesSection;
    vn::util::GObjectSPtr<GSimpleAction> applyToAllPagesAction;
};
