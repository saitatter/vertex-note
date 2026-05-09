#pragma once

#include <gtk/gtk.h>

class Settings;

namespace vn::popup {

class UpdateDialog {
public:
    static void show(GtkWindow* parent, Settings* settings);
};

}  // namespace vn::popup
