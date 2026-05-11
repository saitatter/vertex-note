#include <algorithm>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <string>
#include <string_view>
#include <vector>

#include <config-features.h>
#include <gtest/gtest.h>

#include "qt/QtLuaPluginRuntime.h"
#include "ui/common/ICommandHost.h"
#include "ui/common/IPluginUiBridge.h"

namespace {

class RecordingPluginUiBridge: public vn::ui::common::IPluginUiBridge {
public:
    void registerMenuAction(const vn::ui::common::PluginUiActionDescriptor& action) override {
        this->menuActionIds.push_back(action.id);
    }

    void registerToolbarAction(const vn::ui::common::PluginUiActionDescriptor& action) override {
        this->toolbarActionIds.push_back(action.id);
    }

    void removeAction(std::string_view id) override { this->removedActionIds.emplace_back(id); }

    void registerPlaceholder(std::string_view id, std::string_view displayName,
                             std::string_view description) override {
        this->placeholderIds.emplace_back(id);
        this->placeholderDisplayNames.emplace_back(displayName);
        this->placeholderDescriptions.emplace_back(description);
    }

    void setPlaceholderValue(std::string_view id, std::string_view value) override {
        this->placeholderValues.emplace_back(std::string(id) + "=" + std::string(value));
    }

    void removePlaceholder(std::string_view id) override { this->removedPlaceholderIds.emplace_back(id); }

public:
    std::vector<std::string> menuActionIds;
    std::vector<std::string> toolbarActionIds;
    std::vector<std::string> removedActionIds;
    std::vector<std::string> placeholderIds;
    std::vector<std::string> placeholderDisplayNames;
    std::vector<std::string> placeholderDescriptions;
    std::vector<std::string> placeholderValues;
    std::vector<std::string> removedPlaceholderIds;
};

class RecordingCommandHost: public vn::ui::common::ICommandHost {
public:
    void registerCommand(vn::ui::common::CommandDescriptor descriptor, CommandHandler handler) override {
        this->commandIds.push_back(std::move(descriptor.id));
        this->handlers.push_back(std::move(handler));
    }

    void setCommandEnabled(std::string_view, bool) override {}
    void setCommandChecked(std::string_view id, bool checked) override {
        this->checkedCommandIds.push_back(std::string(id) + (checked ? "=true" : "=false"));
    }
    [[nodiscard]] auto hasCommand(std::string_view id) const -> bool override {
        return std::ranges::find(this->commandIds, std::string(id)) != this->commandIds.end();
    }
    [[nodiscard]] auto isCommandChecked(std::string_view id) const -> bool override {
        return std::ranges::find(this->checkedCommandIds, std::string(id) + "=true") != this->checkedCommandIds.end();
    }
    void triggerCommand(std::string_view id) override { this->triggeredCommandIds.emplace_back(id); }

public:
    std::vector<std::string> commandIds;
    std::vector<CommandHandler> handlers;
    std::vector<std::string> checkedCommandIds;
    std::vector<std::string> triggeredCommandIds;
};

auto hasString(const std::vector<std::string>& values, std::string_view expected) -> bool {
    return std::ranges::find(values, std::string(expected)) != values.end();
}

auto uniqueTempPath() -> std::filesystem::path {
    const auto now = std::chrono::steady_clock::now().time_since_epoch().count();
    return std::filesystem::temp_directory_path() / ("vertexnote-qt-lua-runtime-test-" + std::to_string(now));
}

void writeSmokePlugin(const std::filesystem::path& searchRoot) {
    const auto pluginDir = searchRoot / "QtSmokePlugin";
    std::filesystem::create_directories(pluginDir);

    {
        std::ofstream ini(pluginDir / "plugin.ini", std::ios::trunc);
        ini << "[about]\n";
        ini << "author=VertexNote Test\n";
        ini << "description=Qt Lua runtime smoke plugin\n";
        ini << "version=1.0\n";
        ini << "[default]\n";
        ini << "enabled=true\n";
        ini << "[plugin]\n";
        ini << "mainfile=main.lua\n";
    }

    {
        std::ofstream lua(pluginDir / "main.lua", std::ios::trunc);
        lua << "function initUi()\n";
        lua << "  app.registerUi({menu='Smoke Menu', callback='onSmoke'})\n";
        lua << "  app.registerUi({toolbarId='smoke-toolbar', iconName='Smoke', callback='onSmoke'})\n";
        lua << "  app.registerPlaceholder('mode', 'Current mode')\n";
        lua << "  app.setPlaceholderValue('mode', 'RUN')\n";
        lua << "end\n";
        lua << "function onSmoke() end\n";
    }
}

}  // namespace

TEST(VertexNoteQtLuaPluginRuntime, loadsEnabledPluginAndCleansUiRegistrationsOnReload) {
#ifdef ENABLE_PLUGINS
    const auto searchRoot = uniqueTempPath();
    writeSmokePlugin(searchRoot);

    RecordingPluginUiBridge bridge;
    RecordingCommandHost commandHost;
    QtLuaPluginRuntime runtime(&bridge, &commandHost, nullptr);
    runtime.configurePluginSearchPaths({searchRoot});

    runtime.loadEnabledPlugins();

    const auto statuses = runtime.statuses();
    ASSERT_EQ(statuses.size(), 1U);
    EXPECT_EQ(statuses.front().name, "QtSmokePlugin");
    EXPECT_TRUE(statuses.front().valid);
    EXPECT_TRUE(statuses.front().enabled);
    EXPECT_EQ(statuses.front().registeredActions, 2);
    EXPECT_TRUE(statuses.front().error.empty());
    EXPECT_TRUE(hasString(bridge.menuActionIds, "plugin.QtSmokePlugin.0.menu"));
    EXPECT_TRUE(hasString(bridge.toolbarActionIds, "plugin.QtSmokePlugin.1.toolbar"));
    EXPECT_TRUE(hasString(bridge.placeholderIds, "plugin.QtSmokePlugin.placeholder.mode"));
    EXPECT_TRUE(hasString(bridge.placeholderValues, "plugin.QtSmokePlugin.placeholder.mode=RUN"));

    runtime.loadEnabledPlugins();

    EXPECT_TRUE(hasString(bridge.removedActionIds, "plugin.QtSmokePlugin.0.menu"));
    EXPECT_TRUE(hasString(bridge.removedActionIds, "plugin.QtSmokePlugin.1.toolbar"));
    EXPECT_TRUE(hasString(bridge.removedPlaceholderIds, "plugin.QtSmokePlugin.placeholder.mode"));

    std::filesystem::remove_all(searchRoot);
#else
    GTEST_SKIP() << "Lua plugin support is disabled";
#endif
}
