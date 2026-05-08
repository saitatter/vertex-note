#pragma once

#include <gtk/gtk.h>

class Settings;

namespace xoj::popup {

class UpdateDialog {
public:
    static void show(GtkWindow* parent, Settings* settings);
};

}  // namespace xoj::popup
