/*
 * VertexNote
 *
 * Platform-neutral UI contracts for the long-term shell migration.
 */

#pragma once

#include <filesystem>
#include <functional>
#include <string>
#include <vector>

namespace vn::ui::common {

enum class CanvasCursor {
    Arrow,
    Crosshair,
    Hand,
    IBeam,
    Wait,
    Hidden,
};

struct CommandDescriptor {
    std::string id;
    std::string text;
    std::string tooltip;
    std::string shortcut;
    std::string menu;
    bool checkable = false;
    bool enabled = true;
    bool checked = false;
};

struct CanvasViewport {
    double zoom = 1.0;
    double scrollX = 0.0;
    double scrollY = 0.0;
    double width = 0.0;
    double height = 0.0;
    double devicePixelRatio = 1.0;
};

struct FileDialogFilter {
    std::string label;
    std::vector<std::string> patterns;
};

struct UpdateReleaseSummary {
    std::string version;
    std::string title;
    std::string notes;
    std::string downloadUrl;
};

struct PluginUiActionDescriptor {
    std::string id;
    std::string label;
    std::string tooltip;
    std::function<void()> callback;
};

}  // namespace vn::ui::common
