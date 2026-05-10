/*
 * VertexNote
 *
 * Minimal Lua plugin runtime for the Qt shell.
 */

#pragma once

#include <filesystem>
#include <memory>
#include <string>
#include <vector>

#include "ui/common/IPluginUiBridge.h"

class QWidget;
namespace vn::ui::common {
class ICommandHost;
}

class QtLuaPluginRuntime {
public:
    struct PluginStatus {
        std::string name;
        std::string description;
        std::string author;
        std::string version;
        std::filesystem::path path;
        bool valid = false;
        bool defaultEnabled = false;
        bool enabled = false;
        int registeredActions = 0;
        std::string error;
    };

public:
    QtLuaPluginRuntime(vn::ui::common::IPluginUiBridge* bridge, vn::ui::common::ICommandHost* commandHost,
                       QWidget* parent);
    ~QtLuaPluginRuntime();

public:
    void loadEnabledPlugins();
    void saveEnabledStates(const std::vector<std::pair<std::string, bool>>& states);
    [[nodiscard]] auto statuses() const -> std::vector<PluginStatus>;
    [[nodiscard]] auto parentWidget() const -> QWidget*;

public:
    struct Plugin;

private:
    vn::ui::common::IPluginUiBridge* bridge = nullptr;
    vn::ui::common::ICommandHost* commandHost = nullptr;
    QWidget* parent = nullptr;
    std::vector<std::unique_ptr<Plugin>> plugins;
};
