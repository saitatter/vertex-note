/*
 * VertexNote
 *
 * Minimal Lua plugin runtime for the Qt shell.
 */

#include "QtLuaPluginRuntime.h"

#include <algorithm>
#include <cctype>
#include <limits>
#include <sstream>
#include <string_view>
#include <tuple>

#include <QMessageBox>
#include <QSettings>
#include <QString>
#include <QStringList>
#include <QWidget>

#include "config-features.h"
#include "config-paths.h"
#include "config.h"
#include "filesystem.h"
#include "util/PathUtil.h"

#ifdef ENABLE_PLUGINS
extern "C" {
#include <lauxlib.h>
#include <lua.h>
#include <lualib.h>
}
#endif

namespace {

constexpr ptrdiff_t NO_PLUGIN_MODE = std::numeric_limits<ptrdiff_t>::max();

auto toQString(const std::filesystem::path& path) -> QString { return QString::fromStdWString(path.wstring()); }

auto qStringToStd(const QString& value) -> std::string { return value.toStdString(); }

auto pluginSettingsKey(std::string_view name) -> QString {
    QString key;
    key.reserve(static_cast<int>(name.size()));
    for (const unsigned char ch: name) {
        key.append(std::isalnum(ch) != 0 ? QChar(static_cast<char>(ch)) : QChar('_'));
    }
    return key;
}

auto gtkAcceleratorToQtShortcut(std::string_view accelerator) -> std::string {
    if (accelerator.empty()) {
        return {};
    }

    std::string input(accelerator);
    struct Replacement {
        std::string_view gtk;
        std::string_view qt;
    };
    constexpr Replacement replacements[] = {
            {"<Control>", "Ctrl+"},
            {"<Primary>", "Ctrl+"},
            {"<Shift>", "Shift+"},
            {"<Alt>", "Alt+"},
            {"<Super>", "Meta+"},
    };

    for (const auto& replacement: replacements) {
        std::string::size_type pos = 0;
        while ((pos = input.find(replacement.gtk, pos)) != std::string::npos) {
            input.replace(pos, replacement.gtk.size(), replacement.qt);
            pos += replacement.qt.size();
        }
    }

    if (!input.empty() && input.back() != '+' && input.size() >= 1U) {
        input.back() = static_cast<char>(std::toupper(static_cast<unsigned char>(input.back())));
    }
    return input;
}

#ifdef ENABLE_PLUGINS
auto luaOptionalString(lua_State* lua, int index, const char* fallback = "") -> std::string {
    return lua_isnil(lua, index) == 1 ? std::string(fallback) : std::string(luaL_optstring(lua, index, fallback));
}
#endif

}  // namespace

struct QtLuaPluginRuntime::Plugin {
    std::string name;
    std::string description;
    std::string author;
    std::string version;
    std::filesystem::path path;
    std::string mainfile;
    bool valid = false;
    bool defaultEnabled = false;
    bool enabled = false;
    bool inInitUi = false;
    int registeredActions = 0;
    std::string error;
    QtLuaPluginRuntime* runtime = nullptr;

#ifdef ENABLE_PLUGINS
    struct LuaStateDeleter {
        void operator()(lua_State* lua) const {
            if (lua) {
                lua_close(lua);
            }
        }
    };
    std::unique_ptr<lua_State, LuaStateDeleter> lua;
#endif

    [[nodiscard]] auto status() const -> PluginStatus {
        return {.name = name,
                .description = description,
                .author = author,
                .version = version,
                .path = path,
                .valid = valid,
                .defaultEnabled = defaultEnabled,
                .enabled = enabled,
                .registeredActions = registeredActions,
                .error = error};
    }

#ifdef ENABLE_PLUGINS
    void addPluginToLuaPath() {
        lua_getglobal(lua.get(), "package");
        lua_getfield(lua.get(), -1, "path");
        const char* existing = lua_tostring(lua.get(), -1);
        const auto pluginPath = (path / "?.lua").string();
        const std::string combinedPath = pluginPath + ";" + (existing ? existing : "");
        lua_pop(lua.get(), 1);
        lua_pushstring(lua.get(), combinedPath.c_str());
        lua_setfield(lua.get(), -2, "path");
        lua_pop(lua.get(), 1);
    }

    auto callFunction(const std::string& functionName, ptrdiff_t mode = NO_PLUGIN_MODE) -> bool {
        if (!lua) {
            return false;
        }

        lua_getglobal(lua.get(), functionName.c_str());
        if (lua_isfunction(lua.get(), -1) != 1) {
            lua_pop(lua.get(), 1);
            error = "Missing Lua callback: " + functionName;
            return false;
        }

        int argumentCount = 0;
        if (mode != NO_PLUGIN_MODE) {
            lua_pushinteger(lua.get(), static_cast<lua_Integer>(mode));
            argumentCount = 1;
        }

        if (lua_pcall(lua.get(), argumentCount, 0, 0) != LUA_OK) {
            const char* err = lua_tostring(lua.get(), -1);
            error = err ? err : "Unknown Lua error";
            lua_pop(lua.get(), 1);
            if (runtime && runtime->parent) {
                QMessageBox::warning(runtime->parent, QStringLiteral("Plugin Error"),
                                     QString::fromStdString(name + ": " + error));
            }
            return false;
        }
        return true;
    }

    auto registerUi(std::string menu, std::string callback, ptrdiff_t mode, std::string accelerator,
                    std::string toolbarId, std::string iconName) -> int {
        const int menuId = registeredActions++;
        const auto baseId = std::string("plugin.") + name + "." + std::to_string(menuId);
        const auto shortcut = gtkAcceleratorToQtShortcut(accelerator);

        auto makeDescriptor = [&](std::string id, std::string label) {
            return vn::ui::common::PluginUiActionDescriptor{
                    .id = std::move(id),
                    .label = std::move(label),
                    .tooltip = description.empty() ? "Plugin: " + name : description,
                    .shortcut = shortcut,
                    .callback = [this, callback, mode]() { this->callFunction(callback, mode); },
            };
        };

        if (!menu.empty() && runtime && runtime->bridge) {
            runtime->bridge->registerMenuAction(makeDescriptor(baseId + ".menu", menu));
        }
        if (!toolbarId.empty() && runtime && runtime->bridge) {
            const auto label = iconName.empty() ? toolbarId : iconName;
            runtime->bridge->registerToolbarAction(makeDescriptor(baseId + ".toolbar", label));
        }
        return menuId;
    }
#endif
};

#ifdef ENABLE_PLUGINS
namespace {

auto pluginFromLua(lua_State* lua) -> QtLuaPluginRuntime::Plugin* {
    lua_getfield(lua, LUA_REGISTRYINDEX, "VertexNote_QtLuaPlugin");
    auto* plugin = lua_islightuserdata(lua, -1) == 1
                           ? static_cast<QtLuaPluginRuntime::Plugin*>(lua_touserdata(lua, -1))
                           : nullptr;
    lua_pop(lua, 1);
    return plugin;
}

auto luaRegisterUi(lua_State* lua) -> int {
    auto* plugin = pluginFromLua(lua);
    if (!plugin || !plugin->inInitUi) {
        return luaL_error(lua, "registerUi needs to be called within initUi()");
    }

    lua_settop(lua, 1);
    luaL_checktype(lua, 1, LUA_TTABLE);

    lua_getfield(lua, 1, "accelerator");
    lua_getfield(lua, 1, "menu");
    lua_getfield(lua, 1, "callback");
    lua_getfield(lua, 1, "mode");
    lua_getfield(lua, 1, "toolbarId");
    lua_getfield(lua, 1, "iconName");

    const auto accelerator = luaOptionalString(lua, -6);
    const auto menu = luaOptionalString(lua, -5);
    const char* callback = lua_isnil(lua, -4) == 1 ? nullptr : luaL_optstring(lua, -4, nullptr);
    const ptrdiff_t mode = static_cast<ptrdiff_t>(luaL_optinteger(lua, -3, NO_PLUGIN_MODE));
    const auto toolbarId = luaOptionalString(lua, -2);
    const auto iconName = luaOptionalString(lua, -1);

    if (!callback) {
        return luaL_error(lua, "Missing callback function!");
    }

    const int menuId = plugin->registerUi(menu, callback, mode, accelerator, toolbarId, iconName);
    lua_pop(lua, 6);

    lua_createtable(lua, 0, 1);
    lua_pushstring(lua, "menuId");
    lua_pushinteger(lua, menuId);
    lua_settable(lua, -3);
    return 1;
}

auto luaUnsupported(lua_State* lua) -> int {
    return luaL_error(lua, "This VertexNote plugin API is not available in the Qt shell yet");
}

constexpr luaL_Reg QT_APP_LIB[] = {
        {"registerUi", luaRegisterUi},
        {"openDialog", luaUnsupported},
        {"msgbox", luaUnsupported},
        {"changeActionState", luaUnsupported},
        {"activateAction", luaUnsupported},
        {"getActionState", luaUnsupported},
        {"registerPlaceholder", luaUnsupported},
        {nullptr, nullptr},
};

auto luaOpenQtApp(lua_State* lua) -> int {
    luaL_newlib(lua, QT_APP_LIB);
    lua_createtable(lua, 0, 0);
    lua_setfield(lua, -2, "C");
    return 1;
}

void registerQtAppLib(lua_State* lua) {
    luaL_requiref(lua, "app", luaOpenQtApp, 1);
    lua_pop(lua, 1);
}

auto loadIni(const std::filesystem::path& pluginPath) -> std::unique_ptr<QtLuaPluginRuntime::Plugin> {
    auto plugin = std::make_unique<QtLuaPluginRuntime::Plugin>();
    plugin->name = pluginPath.filename().string();
    plugin->path = pluginPath;

    const auto iniPath = pluginPath / "plugin.ini";
    if (!std::filesystem::exists(iniPath)) {
        plugin->error = "Missing plugin.ini";
        return plugin;
    }

    QSettings ini(toQString(iniPath), QSettings::IniFormat);
    plugin->author = qStringToStd(ini.value(QStringLiteral("about/author")).toString());
    plugin->description = qStringToStd(ini.value(QStringLiteral("about/description")).toString());
    plugin->version = qStringToStd(ini.value(QStringLiteral("about/version")).toString());
    if (plugin->version == "<vertexnote>" || plugin->version == "<vertex-note>") {
        plugin->version = PROJECT_VERSION;
    }
    plugin->mainfile = qStringToStd(ini.value(QStringLiteral("plugin/mainfile")).toString());
    plugin->defaultEnabled = ini.value(QStringLiteral("default/enabled"), false).toBool();
    if (plugin->mainfile.empty()) {
        plugin->error = "Missing plugin/mainfile in plugin.ini";
        return plugin;
    }
    if (plugin->mainfile.find("..") != std::string::npos) {
        plugin->error = "Unsupported plugin mainfile path: " + plugin->mainfile;
        return plugin;
    }

    QSettings appSettings;
    plugin->enabled = appSettings.value(QStringLiteral("plugins/enabled/") + pluginSettingsKey(plugin->name),
                                        plugin->defaultEnabled)
                              .toBool();
    plugin->valid = true;
    return plugin;
}

void loadPluginScript(QtLuaPluginRuntime::Plugin& plugin) {
    if (!plugin.valid || !plugin.enabled) {
        return;
    }

    plugin.lua.reset(luaL_newstate());
    luaL_openlibs(plugin.lua.get());
    registerQtAppLib(plugin.lua.get());
    plugin.addPluginToLuaPath();

    lua_pushlightuserdata(plugin.lua.get(), &plugin);
    lua_setfield(plugin.lua.get(), LUA_REGISTRYINDEX, "VertexNote_QtLuaPlugin");

    const auto luaFile = plugin.path / plugin.mainfile;
    if (luaL_loadfile(plugin.lua.get(), luaFile.string().c_str()) != LUA_OK) {
        const char* err = lua_tostring(plugin.lua.get(), -1);
        plugin.error = err ? err : "Could not load Lua file";
        lua_pop(plugin.lua.get(), 1);
        plugin.valid = false;
        return;
    }
    if (lua_pcall(plugin.lua.get(), 0, 0, 0) != LUA_OK) {
        const char* err = lua_tostring(plugin.lua.get(), -1);
        plugin.error = err ? err : "Could not run Lua file";
        lua_pop(plugin.lua.get(), 1);
        plugin.valid = false;
        return;
    }

    plugin.inInitUi = true;
    lua_getglobal(plugin.lua.get(), "initUi");
    if (lua_isfunction(plugin.lua.get(), -1) == 1) {
        if (lua_pcall(plugin.lua.get(), 0, 0, 0) != LUA_OK) {
            const char* err = lua_tostring(plugin.lua.get(), -1);
            plugin.error = err ? err : "initUi failed";
            lua_pop(plugin.lua.get(), 1);
            plugin.valid = false;
        }
    } else {
        lua_pop(plugin.lua.get(), 1);
    }
    plugin.inInitUi = false;
}

}  // namespace
#endif

QtLuaPluginRuntime::QtLuaPluginRuntime(vn::ui::common::IPluginUiBridge* bridge, QWidget* parent):
        bridge(bridge), parent(parent) {}

QtLuaPluginRuntime::~QtLuaPluginRuntime() = default;

void QtLuaPluginRuntime::loadEnabledPlugins() {
    this->plugins.clear();

#ifdef ENABLE_PLUGINS
    const std::vector<std::filesystem::path> searchPaths = {std::filesystem::path(PROJECT_SOURCE_DIR) / "plugins",
                                                            Util::getConfigSubfolder("plugins")};

    for (const auto& searchPath: searchPaths) {
        if (!std::filesystem::is_directory(searchPath)) {
            continue;
        }
        for (const auto& entry: std::filesystem::directory_iterator(searchPath)) {
            if (!entry.is_directory()) {
                continue;
            }
            auto plugin = loadIni(entry.path());
            plugin->runtime = this;
            this->plugins.push_back(std::move(plugin));
        }
    }

    std::ranges::sort(this->plugins, [](const auto& lhs, const auto& rhs) {
        return std::tie(lhs->name, lhs->path) < std::tie(rhs->name, rhs->path);
    });

    for (auto& plugin: this->plugins) {
        loadPluginScript(*plugin);
    }
#else
    auto plugin = std::make_unique<Plugin>();
    plugin->name = "Lua plugins";
    plugin->error = "VertexNote was built without Lua plugin support.";
    this->plugins.push_back(std::move(plugin));
#endif
}

auto QtLuaPluginRuntime::statuses() const -> std::vector<PluginStatus> {
    std::vector<PluginStatus> result;
    result.reserve(this->plugins.size());
    for (const auto& plugin: this->plugins) {
        result.push_back(plugin->status());
    }
    return result;
}
