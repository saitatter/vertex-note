#include <algorithm>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

#include <config-features.h>
#include <config-paths.h>
#include <gtest/gtest.h>

#include "QtDocumentController.h"
#include "QtLuaPluginRuntime.h"
#include "ui/common/ICommandHost.h"
#include "ui/common/IPluginUiBridge.h"
#include "view/render/Renderers.h"

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

    void setCommandEnabled(std::string_view id, bool enabled) override {
        this->enabledCommandIds.push_back(std::string(id) + (enabled ? "=true" : "=false"));
    }
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
    std::vector<std::string> enabledCommandIds;
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

void writeLegacyCompatPlugin(const std::filesystem::path& searchRoot) {
    const auto pluginDir = searchRoot / "QtLegacyCompatPlugin";
    std::filesystem::create_directories(pluginDir);

    {
        std::ofstream ini(pluginDir / "plugin.ini", std::ios::trunc);
        ini << "[about]\n";
        ini << "author=VertexNote Test\n";
        ini << "description=Qt Lua legacy compat plugin\n";
        ini << "version=1.0\n";
        ini << "[default]\n";
        ini << "enabled=true\n";
        ini << "[plugin]\n";
        ini << "mainfile=main.lua\n";
    }

    {
        std::ofstream lua(pluginDir / "main.lua", std::ios::trunc);
        lua << "function initUi()\n";
        lua << "  local pos = app.getScrollPos()\n";
        lua << "  if pos.x ~= 10 or pos.y ~= 20 or pos.width ~= 300 or pos.height ~= 200 then error('bad scroll pos') end\n";
        lua << "  app.scrollToPos(5, 7)\n";
        lua << "  app.scrollToPos(100, 110, false)\n";
        lua << "  if app.getSidebarPageNo() ~= 1 then error('bad sidebar page') end\n";
        lua << "  app.setSidebarPageNo(3)\n";
        lua << "  app.showFloatingToolbox(12, 34)\n";
        lua << "  app.uiAction({action='ACTION_PASTE'})\n";
        lua << "  app.uiAction({action='ACTION_COPY', enabled=false})\n";
        lua << "  app.layerAction('ACTION_GOTO_NEXT_LAYER')\n";
        lua << "  app.sidebarAction('NEW_AFTER')\n";
        lua << "  app.sidebarAction('MOVE_UP')\n";
        lua << "  app.addSplines({splines={{coordinates={0,0,10,0,10,10,20,10}, width=2, color=16711935}}})\n";
        lua << "end\n";
    }
}

auto strokeCount(const vn::view::render::PageRenderSnapshot& snapshot) -> std::size_t {
    std::size_t count = 0;
    for (const auto& drawable: snapshot.drawables) {
        if (std::holds_alternative<vn::view::render::StrokeRenderModel>(drawable)) {
            ++count;
        }
    }
    return count;
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

TEST(VertexNoteQtLuaPluginRuntime, discoversBundledPluginsWithoutLoadErrors) {
#ifdef ENABLE_PLUGINS
    RecordingPluginUiBridge bridge;
    RecordingCommandHost commandHost;
    QtLuaPluginRuntime runtime(&bridge, &commandHost, nullptr);
    runtime.configurePluginSearchPaths({std::filesystem::path(PROJECT_SOURCE_DIR) / "plugins"});

    runtime.loadEnabledPlugins();

    const auto statuses = runtime.statuses();
    const std::vector<std::string> expectedNames = {"BeamerPresentation", "ColorCycle",     "Example",
                                                    "Export",             "FitToContent",   "HighlightPosition",
                                                    "ImageActions",       "LayerActions",   "QuickScreenshot",
                                                    "SpaceForNotes",      "ToggleGrid"};
    ASSERT_EQ(statuses.size(), expectedNames.size());

    std::vector<std::string> actualNames;
    actualNames.reserve(statuses.size());
    for (const auto& status: statuses) {
        actualNames.push_back(status.name);
        EXPECT_TRUE(status.valid) << status.name << ": " << status.error;
        EXPECT_TRUE(status.error.empty()) << status.name << ": " << status.error;
    }
    EXPECT_EQ(actualNames, expectedNames);
#else
    GTEST_SKIP() << "Lua plugin support is disabled";
#endif
}

TEST(VertexNoteQtLuaPluginRuntime, supportsLegacyApplicationCompatibilityApis) {
#ifdef ENABLE_PLUGINS
    const auto searchRoot = uniqueTempPath();
    writeLegacyCompatPlugin(searchRoot);

    RecordingPluginUiBridge bridge;
    RecordingCommandHost commandHost;
    commandHost.commandIds = {"edit.paste", "edit.copy", "layer.goto-next", "page.add", "page.move-up"};
    QtDocumentController controller;
    bool refreshed = false;
    bool dirty = false;
    std::vector<std::string> scrollCalls;

    QtLuaPluginRuntime runtime(&bridge, &commandHost, nullptr);
    runtime.configurePluginSearchPaths({searchRoot});
    runtime.configureDocumentAccess(&controller, []() { return 0U; }, [](std::size_t) {},
                                    [&]() { refreshed = true; }, [&]() { dirty = true; });
    runtime.configureViewportAccess(
            []() {
                return vn::ui::common::CanvasViewport{.zoom = 1.5,
                                                      .scrollX = 10.0,
                                                      .scrollY = 20.0,
                                                      .width = 300.0,
                                                      .height = 200.0,
                                                      .devicePixelRatio = 1.0};
            },
            [&](double x, double y, bool relative) {
                scrollCalls.push_back(std::to_string(static_cast<int>(x)) + "," +
                                      std::to_string(static_cast<int>(y)) + "," + (relative ? "rel" : "abs"));
            });

    runtime.loadEnabledPlugins();

    const auto statuses = runtime.statuses();
    ASSERT_EQ(statuses.size(), 1U);
    EXPECT_TRUE(statuses.front().valid) << statuses.front().error;
    EXPECT_TRUE(statuses.front().error.empty()) << statuses.front().error;
    EXPECT_TRUE(refreshed);
    EXPECT_TRUE(dirty);
    EXPECT_EQ(scrollCalls, (std::vector<std::string>{"5,7,rel", "100,110,abs"}));
    EXPECT_TRUE(hasString(commandHost.triggeredCommandIds, "edit.paste"));
    EXPECT_TRUE(hasString(commandHost.enabledCommandIds, "edit.copy=false"));
    EXPECT_TRUE(hasString(commandHost.triggeredCommandIds, "layer.goto-next"));
    EXPECT_TRUE(hasString(commandHost.triggeredCommandIds, "page.add"));
    EXPECT_TRUE(hasString(commandHost.triggeredCommandIds, "page.move-up"));
    ASSERT_EQ(1U, controller.snapshotPages().size());
    EXPECT_EQ(1U, strokeCount(controller.snapshotPages().front()));

    std::filesystem::remove_all(searchRoot);
#else
    GTEST_SKIP() << "Lua plugin support is disabled";
#endif
}
