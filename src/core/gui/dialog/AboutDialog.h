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

namespace vn::popup {
class AboutDialog {
public:
    AboutDialog(GladeSearchpath* gladeSearchPath);
    ~AboutDialog();

    inline GtkWindow* getWindow() const { return window.get(); }

private:
    vn::util::GtkWindowUPtr window;
};
};  // namespace vn::popup
