/*
 * VertexNote
 *
 * The about dialog
 *
 * @author VertexNote Team
 * https://github.com/saitatter/vertex-note
 *
 * @license GNU GPLv2 or later
 */

#pragma once

#include "util/raii/GtkWindowUPtr.h"

class GladeSearchpath;

namespace xoj::popup {
class AboutDialog {
public:
    AboutDialog(GladeSearchpath* gladeSearchPath);
    ~AboutDialog();

    inline GtkWindow* getWindow() const { return window.get(); }

private:
    xoj::util::GtkWindowUPtr window;
};
};  // namespace xoj::popup
