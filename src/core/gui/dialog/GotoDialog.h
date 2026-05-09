/*
 * VertexNote
 *
 * Goto-Page dialog
 *
 * @author VertexNote Team
 * https://github.com/saitatter/vertex-note
 *
 * @license GNU GPLv2 or later
 */

#pragma once

#include <functional>

#include <gtk/gtk.h>

#include "util/raii/GtkWindowUPtr.h"

class GladeSearchpath;

namespace vn::popup {
class GotoDialog {
public:
    GotoDialog(GladeSearchpath* gladeSearchPath, size_t initialPage, size_t maxPage,
               std::function<void(size_t)> callback);
    ~GotoDialog();

public:
    inline GtkWindow* getWindow() const { return window.get(); }

private:
    vn::util::GtkWindowUPtr window;
    GtkSpinButton* spinButton = nullptr;

    std::function<void(size_t)> callback;
};
};  // namespace vn::popup
