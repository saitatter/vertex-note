/*
 * VertexNote
 *
 * Simple dialog for editing a fixed-length geometry constraint.
 */

#pragma once

#include <functional>

#include <gtk/gtk.h>

namespace vn::popup {

class FixedLengthConstraintDialog {
public:
    static void show(GtkWindow* parent, double initialValue, std::function<void(double)> callback);
};

}  // namespace vn::popup
