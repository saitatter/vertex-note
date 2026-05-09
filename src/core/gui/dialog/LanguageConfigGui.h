/*
 * VertexNote
 *
 * The configuration for a language in the Settings Dialog
 *
 * @author VertexNote Team
 * https://github.com/saitatter/vertex-note
 *
 * @license GNU GPLv2 or later
 */

#pragma once

#include <string>  // for string, basic_string
#include <vector>  // for vector

#include <gtk/gtk.h>  // for GtkWidget, GtkBox

#include "util/raii/GObjectSPtr.h"

class Settings;
class GladeSearchpath;

class LanguageConfigGui {
public:
    LanguageConfigGui(GtkBox* parent, Settings* settings);

public:
    void saveSettings();

private:
    std::vector<std::string> availableLocales;

    vn::util::WidgetSPtr comboBox;

    Settings* settings;
};
