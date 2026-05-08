#pragma once

#include <gtk/gtk.h>

namespace xoj::popup {

class UpdateDialog {
public:
    static void show(GtkWindow* parent);
};

}  // namespace xoj::popup
