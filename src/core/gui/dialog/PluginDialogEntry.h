/*
 * VertexNote
 *
 * Configuration of a single plugin
 *
 * @author VertexNote Team
 * https://github.com/saitatter/vertex-note
 *
 * @license GNU GPLv2 or later
 */

#pragma once

#include "config-features.h"

#ifdef ENABLE_PLUGINS
#include <string>  // for string

#include <gtk/gtk.h>

class Plugin;
class GladeSearchpath;

class PluginDialogEntry {
public:
    PluginDialogEntry(Plugin* plugin, GladeSearchpath* gladeSearchPath, GtkBox* box);

public:
    void saveSettings(std::string& pluginEnabled, std::string& pluginDisabled);


private:
    /**
     * Plugin instance
     */
    Plugin* plugin;

    GtkCheckButton* stateButton;
};
#endif
