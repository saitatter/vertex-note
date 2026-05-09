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

#include <string>  // for string, allocator

#include <gtk/gtk.h>  // for GtkWidget, GtkToolItem

#include "AbstractToolItem.h"  // for AbstractToolItem

struct ToolbarButtonEntry;

class PluginToolButton: public AbstractToolItem {
public:
    PluginToolButton(ToolbarButtonEntry* t);

    ~PluginToolButton() override;
    std::string getToolDisplayName() const override;

protected:
    vn::util::WidgetSPtr createItem(bool horizontal) override;
    GtkWidget* getNewToolIcon() const override;

private:
    ToolbarButtonEntry* t;
};
