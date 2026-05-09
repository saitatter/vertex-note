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

#include <functional>

#include "control/ToolEnums.h"
#include "util/raii/GtkWindowUPtr.h"

class GladeSearchpath;

namespace vn::popup {
class SelectOpacityDialog {
public:
    SelectOpacityDialog(GladeSearchpath* gladeSearchPath, int alpha, OpacityFeature type,
                        std::function<void(int, OpacityFeature)> callback);
    ~SelectOpacityDialog();

    inline GtkWindow* getWindow() const { return window.get(); }

private:
    void setPreviewImage(int alpha);

private:
    xoj::util::GtkWindowUPtr window;
    GtkImage* previewImage;
    GtkRange* alphaRange;

    OpacityFeature opacityFeature;
    std::function<void(int, OpacityFeature)> callback;
};
};  // namespace vn::popup
