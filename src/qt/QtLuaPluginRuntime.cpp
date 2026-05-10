/*
 * VertexNote
 *
 * Minimal Lua plugin runtime for the Qt shell.
 */

#include "QtLuaPluginRuntime.h"

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <cstdint>
#include <limits>
#include <sstream>
#include <string_view>
#include <tuple>
#include <unordered_map>
#include <utility>

#include <QMessageBox>
#include <QPushButton>
#include <QSettings>
#include <QString>
#include <QStringList>
#include <QWidget>

#include "config-features.h"
#include "config-paths.h"
#include "config.h"
#include "control/pagetype/PageTypeHandler.h"
#include "filesystem.h"
#include "model/Document.h"
#include "QtDocumentController.h"
#include "ui/common/ICommandHost.h"
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

auto luaOptionalInteger(lua_State* lua, int index, ptrdiff_t fallback = 0) -> ptrdiff_t {
    return lua_isnil(lua, index) == 1 ? fallback : static_cast<ptrdiff_t>(luaL_checkinteger(lua, index));
}

auto luaOptionalBool(lua_State* lua, int index, bool fallback = false) -> bool {
    return lua_isnil(lua, index) == 1 ? fallback : lua_toboolean(lua, index) != 0;
}

auto legacyActionCommand(std::string_view action) -> std::string {
    static const std::unordered_map<std::string_view, std::string_view> ACTIONS = {
            {"new-file", "app.new"},
            {"open", "app.open"},
            {"annotate-pdf", "file.annotate-pdf"},
            {"save", "file.save"},
            {"save-as", "app.save-as"},
            {"export-as-pdf", "export.pdf"},
            {"export-as", "export.png"},
            {"print", "file.print"},
            {"quit", "app.quit"},
            {"undo", "edit.undo-geometry"},
            {"redo", "edit.redo-geometry"},
            {"cut", "edit.cut"},
            {"copy", "edit.copy"},
            {"paste", "edit.paste"},
            {"search", "edit.find"},
            {"select-all", "edit.select-all"},
            {"delete", "edit.delete"},
            {"move-selection-layer-up", "edit.move-selection-layer-up"},
            {"move-selection-layer-down", "edit.move-selection-layer-down"},
            {"preferences", "app.settings"},
            {"manage-toolbar", "app.settings"},
            {"customize-toolbar", "view.customize-toolbar"},
            {"paired-pages-mode", "view.paired-pages"},
            {"presentation-mode", "view.presentation"},
            {"fullscreen", "view.fullscreen"},
            {"show-sidebar", "view.show-sidebar"},
            {"show-toolbar", "view.show-toolbar"},
            {"show-menubar", "view.show-menubar"},
            {"zoom-in", "view.zoom-in"},
            {"zoom-out", "view.zoom-out"},
            {"zoom-100", "view.zoom-100"},
            {"zoom-fit", "view.fit-page"},
            {"goto-first", "nav.first-page"},
            {"goto-previous", "nav.prev-page"},
            {"goto-page", "nav.goto-page"},
            {"goto-next", "nav.next-page"},
            {"goto-last", "nav.last-page"},
            {"goto-next-annotated-page", "nav.next-annotated"},
            {"goto-previous-annotated-page", "nav.prev-annotated"},
            {"new-page-before", "page.add-before"},
            {"new-page-after", "page.add"},
            {"new-page-at-end", "page.add-end"},
            {"duplicate-page", "page.duplicate"},
            {"append-new-pdf-pages", "journal.append-new-pdf-pages"},
            {"configure-page-template", "page.template"},
            {"delete-page", "page.delete"},
            {"paper-format", "page.format"},
            {"paper-background-color", "page.background"},
            {"setsquare", "tool.setsquare"},
            {"compass", "tool.compass"},
            {"tool-fill", "pen.fill-toggle"},
            {"layer-new-above-current", "layer.add-above"},
            {"layer-new-below-current", "layer.add-below"},
            {"layer-copy", "layer.copy"},
            {"layer-delete", "page.delete-layer"},
            {"layer-merge-down", "layer.merge-down"},
            {"layer-rename", "layer.rename"},
            {"layer-show-all", "layer.show-all"},
            {"layer-hide-all", "layer.hide-all"},
            {"layer-goto-next", "layer.goto-next"},
            {"layer-goto-previous", "layer.goto-prev"},
            {"layer-goto-top", "layer.goto-top"},
            {"plugin-manager", "plugins.manager"},
            {"help", "help.open"},
            {"check-for-updates", "app.check-updates"},
            {"about", "app.about-qt-shell"},
    };
    const auto it = ACTIONS.find(action);
    return it == ACTIONS.end() ? std::string(action) : std::string(it->second);
}

auto selectToolCommand(ptrdiff_t tool) -> std::string {
    static const std::unordered_map<ptrdiff_t, std::string_view> TOOLS = {
            {1, "tool.pen"},
            {2, "tool.eraser"},
            {3, "tool.highlighter"},
            {4, "tool.text"},
            {6, "tool.select"},
            {7, "tool.select-region"},
            {8, "tool.select-multilayer-rect"},
            {9, "tool.select-multilayer-region"},
            {10, "tool.select-object"},
            {12, "tool.vertical-space"},
            {13, "tool.hand"},
            {14, "tool.draw-rectangle"},
            {15, "tool.draw-ellipse"},
            {16, "tool.draw-arrow"},
            {17, "tool.draw-double-arrow"},
            {18, "tool.draw-coordinate-system"},
            {20, "tool.draw-spline"},
            {21, "tool.select-pdf-text-linear"},
            {22, "tool.select-pdf-text-rect"},
            {23, "tool.laser-pointer-pen"},
            {24, "tool.laser-pointer-highlighter"},
    };
    const auto it = TOOLS.find(tool);
    return it == TOOLS.end() ? std::string() : std::string(it->second);
}

auto arrangementCommand(ptrdiff_t orderChange) -> std::string {
    static const std::unordered_map<ptrdiff_t, std::string_view> COMMANDS = {
            {0, "edit.bring-to-front"},
            {1, "edit.bring-forward"},
            {2, "edit.send-backward"},
            {3, "edit.send-to-back"},
    };
    const auto it = COMMANDS.find(orderChange);
    return it == COMMANDS.end() ? std::string() : std::string(it->second);
}

auto lineStyleCommand(std::string_view style) -> std::string {
    if (style == "plain") {
        return "pen.line-solid";
    }
    if (style == "dash") {
        return "pen.line-dash";
    }
    if (style == "dashdot") {
        return "pen.line-dashdot";
    }
    if (style == "dot") {
        return "pen.line-dot";
    }
    return {};
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
    std::vector<std::string> actionIds;
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
            const auto id = baseId + ".menu";
            runtime->bridge->registerMenuAction(makeDescriptor(id, menu));
            actionIds.push_back(id);
        }
        if (!toolbarId.empty() && runtime && runtime->bridge) {
            const auto label = iconName.empty() ? toolbarId : iconName;
            const auto id = baseId + ".toolbar";
            runtime->bridge->registerToolbarAction(makeDescriptor(id, label));
            actionIds.push_back(id);
        }
        return menuId;
    }

    auto triggerCommand(std::string_view commandId) -> bool {
        if (!runtime || !runtime->commandHost || !runtime->commandHost->hasCommand(commandId)) {
            error = "Qt command is not available: " + std::string(commandId);
            return false;
        }
        runtime->commandHost->triggerCommand(commandId);
        return true;
    }

    auto setBooleanCommand(std::string_view commandId, bool checked) -> bool {
        if (!runtime || !runtime->commandHost || !runtime->commandHost->hasCommand(commandId)) {
            error = "Qt command is not available: " + std::string(commandId);
            return false;
        }
        if (runtime->commandHost->isCommandChecked(commandId) != checked) {
            runtime->commandHost->triggerCommand(commandId);
        }
        return true;
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

auto documentControllerFromLua(lua_State* lua) -> QtDocumentController* {
    auto* plugin = pluginFromLua(lua);
    if (!plugin || !plugin->runtime || !plugin->runtime->documentControllerPtr() ||
        !plugin->runtime->documentControllerPtr()->hasDocument()) {
        return nullptr;
    }
    return plugin->runtime->documentControllerPtr();
}

void luaSetStringField(lua_State* lua, const char* name, const std::string& value) {
    lua_pushstring(lua, value.c_str());
    lua_setfield(lua, -2, name);
}

void luaSetNumberField(lua_State* lua, const char* name, lua_Number value) {
    lua_pushnumber(lua, value);
    lua_setfield(lua, -2, name);
}

void luaSetIntegerField(lua_State* lua, const char* name, lua_Integer value) {
    lua_pushinteger(lua, value);
    lua_setfield(lua, -2, name);
}

void luaSetBoolField(lua_State* lua, const char* name, bool value) {
    lua_pushboolean(lua, value ? 1 : 0);
    lua_setfield(lua, -2, name);
}

void luaSetLayerFields(lua_State* lua, std::string name, bool visible, bool annotated) {
    lua_newtable(lua);
    luaSetStringField(lua, "name", name);
    luaSetBoolField(lua, "isVisible", visible);
    luaSetBoolField(lua, "isAnnotated", annotated);
}

auto clampLuaPageIndex(QtLuaPluginRuntime::Plugin* plugin, ptrdiff_t pageNumber) -> std::size_t {
    auto* controller = plugin && plugin->runtime ? plugin->runtime->documentControllerPtr() : nullptr;
    if (!controller || controller->pageCount() == 0) {
        return 0U;
    }
    const auto zeroBased = std::max<ptrdiff_t>(0, pageNumber - 1);
    return std::min<std::size_t>(static_cast<std::size_t>(zeroBased), controller->pageCount() - 1U);
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

auto luaOpenDialog(lua_State* lua) -> int {
    auto* plugin = pluginFromLua(lua);
    if (!plugin || !plugin->runtime || !plugin->runtime->parentWidget()) {
        return luaL_error(lua, "Plugin runtime is not available");
    }

    const auto message = luaOptionalString(lua, 1);
    luaL_checktype(lua, 2, LUA_TTABLE);
    const auto callback = luaOptionalString(lua, 3);
    const bool error = luaOptionalBool(lua, 4, false);

    QMessageBox box(plugin->runtime->parentWidget());
    box.setWindowTitle(error ? QStringLiteral("Plugin Error") : QStringLiteral("Plugin Message"));
    box.setIcon(error ? QMessageBox::Warning : QMessageBox::Information);
    box.setText(QString::fromStdString(message));

    std::vector<QPushButton*> buttons;
    lua_pushnil(lua);
    while (lua_next(lua, 2) != 0) {
        if (lua_isstring(lua, -1) == 1) {
            buttons.push_back(box.addButton(QString::fromUtf8(lua_tostring(lua, -1)), QMessageBox::AcceptRole));
        }
        lua_pop(lua, 1);
    }
    if (buttons.empty()) {
        buttons.push_back(box.addButton(QStringLiteral("OK"), QMessageBox::AcceptRole));
    }

    box.exec();
    if (!callback.empty()) {
        const auto it = std::ranges::find(buttons, box.clickedButton());
        if (it != buttons.end()) {
            plugin->callFunction(callback, static_cast<ptrdiff_t>(std::distance(buttons.begin(), it) + 1));
        }
    }
    return 0;
}

auto luaGetDocumentStructure(lua_State* lua) -> int {
    auto* plugin = pluginFromLua(lua);
    auto* controller = documentControllerFromLua(lua);
    if (!plugin || !plugin->runtime || !controller) {
        return luaL_error(lua, "No active Qt document");
    }

    const auto& pages = controller->snapshotPages();
    lua_newtable(lua);

    lua_newtable(lua);
    for (std::size_t pageIndex = 0; pageIndex < pages.size(); ++pageIndex) {
        const auto& page = pages[pageIndex];
        const auto& background = page.background;

        lua_newtable(lua);
        luaSetNumberField(lua, "pageWidth", page.width);
        luaSetNumberField(lua, "pageHeight", page.height);
        luaSetBoolField(lua, "isAnnotated", controller->isPageAnnotated(pageIndex));
        luaSetStringField(lua, "pageTypeFormat",
                          PageTypeHandler::getStringForPageTypeFormat(background.backgroundFormat));
        luaSetStringField(lua, "pageTypeConfig", "");
        luaSetIntegerField(lua, "backgroundColor",
                           static_cast<lua_Integer>(static_cast<uint32_t>(background.backgroundColor) & 0x00ffffffU));
        luaSetIntegerField(lua, "pdfBackgroundPageNo", static_cast<lua_Integer>(background.pdfPageNumber + 1U));

        lua_newtable(lua);
        luaSetLayerFields(lua, background.hasBackgroundName ? background.backgroundName : std::string("Background"),
                          true, background.annotated);
        lua_rawseti(lua, -2, 0);

        const auto layers = controller->layerInfos(pageIndex);
        for (const auto& layer: layers) {
            luaSetLayerFields(lua, layer.name, layer.visible, layer.elementCount > 0U);
            lua_rawseti(lua, -2, static_cast<lua_Integer>(layer.index + 1U));
        }
        lua_setfield(lua, -2, "layers");

        luaSetIntegerField(lua, "currentLayer",
                           static_cast<lua_Integer>(controller->selectedLayerIndex(pageIndex) + 1U));
        lua_rawseti(lua, -2, static_cast<lua_Integer>(pageIndex + 1U));
    }
    lua_setfield(lua, -2, "pages");

    luaSetIntegerField(lua, "currentPage",
                       static_cast<lua_Integer>(plugin->runtime->currentDocumentPageIndex() + 1U));
    const auto* document = controller->documentPtr();
    luaSetStringField(lua, "pdfBackgroundFilename", document ? document->getPdfFilepath().string() : std::string());
    luaSetStringField(lua, "xoppFilename", document ? document->getFilepath().string() : std::string());
    return 1;
}

auto luaSetCurrentPage(lua_State* lua) -> int {
    auto* plugin = pluginFromLua(lua);
    auto* controller = documentControllerFromLua(lua);
    if (!plugin || !plugin->runtime || !controller) {
        return luaL_error(lua, "No active Qt document");
    }
    plugin->runtime->navigateToDocumentPage(clampLuaPageIndex(plugin, luaOptionalInteger(lua, 1, 1)));
    return 0;
}

auto luaScrollToPage(lua_State* lua) -> int {
    auto* plugin = pluginFromLua(lua);
    auto* controller = documentControllerFromLua(lua);
    if (!plugin || !plugin->runtime || !controller) {
        return luaL_error(lua, "No active Qt document");
    }

    const auto pageArgument = luaOptionalInteger(lua, 1, 1);
    const bool relative = luaOptionalBool(lua, 2, false);
    ptrdiff_t targetPage = pageArgument;
    if (relative) {
        targetPage = static_cast<ptrdiff_t>(plugin->runtime->currentDocumentPageIndex()) + 1 + pageArgument;
    }
    plugin->runtime->navigateToDocumentPage(clampLuaPageIndex(plugin, targetPage));
    return 0;
}

auto luaSetCurrentLayer(lua_State* lua) -> int {
    auto* plugin = pluginFromLua(lua);
    auto* controller = documentControllerFromLua(lua);
    if (!plugin || !plugin->runtime || !controller) {
        return luaL_error(lua, "No active Qt document");
    }

    const auto pageIndex = plugin->runtime->currentDocumentPageIndex();
    const auto layerCount = controller->layerCount(pageIndex);
    if (layerCount == 0U) {
        return 0;
    }

    const auto requested = std::max<ptrdiff_t>(1, luaOptionalInteger(lua, 1, 1));
    const auto layerIndex = std::min<std::size_t>(static_cast<std::size_t>(requested - 1), layerCount - 1U);
    const bool changeVisibility = luaOptionalBool(lua, 2, false);
    if (changeVisibility) {
        for (std::size_t index = 0; index < layerCount; ++index) {
            controller->setLayerVisible(pageIndex, index, index <= layerIndex);
        }
    }
    controller->selectLayer(pageIndex, layerIndex);
    plugin->runtime->refreshDocumentUi();
    if (changeVisibility) {
        plugin->runtime->markDocumentDirty();
    }
    return 0;
}

auto luaSetLayerVisibility(lua_State* lua) -> int {
    auto* plugin = pluginFromLua(lua);
    auto* controller = documentControllerFromLua(lua);
    if (!plugin || !plugin->runtime || !controller) {
        return luaL_error(lua, "No active Qt document");
    }

    const auto pageIndex = plugin->runtime->currentDocumentPageIndex();
    const auto layerIndex = controller->selectedLayerIndex(pageIndex);
    controller->setLayerVisible(pageIndex, layerIndex, luaOptionalBool(lua, 1, true));
    plugin->runtime->refreshDocumentUi();
    plugin->runtime->markDocumentDirty();
    return 0;
}

auto luaSetCurrentLayerName(lua_State* lua) -> int {
    auto* plugin = pluginFromLua(lua);
    auto* controller = documentControllerFromLua(lua);
    if (!plugin || !plugin->runtime || !controller) {
        return luaL_error(lua, "No active Qt document");
    }

    const auto pageIndex = plugin->runtime->currentDocumentPageIndex();
    controller->renameLayer(pageIndex, controller->selectedLayerIndex(pageIndex), luaOptionalString(lua, 1));
    plugin->runtime->refreshDocumentUi();
    plugin->runtime->markDocumentDirty();
    return 0;
}

auto luaChangeCurrentPageBackground(lua_State* lua) -> int {
    auto* plugin = pluginFromLua(lua);
    auto* controller = documentControllerFromLua(lua);
    if (!plugin || !plugin->runtime || !controller) {
        return luaL_error(lua, "No active Qt document");
    }

    const auto format = PageTypeHandler::getPageTypeFormatForString(luaOptionalString(lua, 1, "plain"));
    controller->setPageBackgroundType(plugin->runtime->currentDocumentPageIndex(), format);
    plugin->runtime->refreshDocumentUi();
    plugin->runtime->markDocumentDirty();
    return 0;
}

auto luaRefreshPage(lua_State* lua) -> int {
    auto* plugin = pluginFromLua(lua);
    if (!plugin || !plugin->runtime) {
        return luaL_error(lua, "Plugin runtime is not available");
    }
    plugin->runtime->refreshDocumentUi();
    return 0;
}

auto luaActivateAction(lua_State* lua) -> int {
    auto* plugin = pluginFromLua(lua);
    const auto action = luaOptionalString(lua, 1);
    std::string command;
    if (action == "arrange-selection-order") {
        command = arrangementCommand(luaOptionalInteger(lua, 2, -1));
    } else {
        command = legacyActionCommand(action);
    }

    if (command.empty() || !plugin || !plugin->triggerCommand(command)) {
        return luaL_error(lua, "Qt shell cannot activate action '%s'", action.c_str());
    }
    return 0;
}

auto luaChangeActionState(lua_State* lua) -> int {
    auto* plugin = pluginFromLua(lua);
    const auto action = luaOptionalString(lua, 1);
    if (!plugin) {
        return luaL_error(lua, "Plugin runtime is not available");
    }

    if (action == "select-tool") {
        const auto command = selectToolCommand(luaOptionalInteger(lua, 2, 0));
        if (command.empty() || !plugin->triggerCommand(command)) {
            return luaL_error(lua, "Qt shell cannot select requested tool");
        }
        return 0;
    }
    if (action == "set-columns-or-rows") {
        const auto value = luaOptionalInteger(lua, 2, 1);
        if (value == 0 || std::abs(value) > 8) {
            return luaL_error(lua, "Unsupported Qt page layout span");
        }
        const auto command = value > 0 ? "view.columns-" + std::to_string(value) : "view.rows-" + std::to_string(-value);
        if (!plugin->triggerCommand(command)) {
            return luaL_error(lua, "Qt shell cannot set page layout span");
        }
        return 0;
    }
    if (action == "set-layout-vertical") {
        return plugin->triggerCommand(luaOptionalBool(lua, 2) ? "view.layout-vertical" : "view.layout-horizontal") ? 0
                                                                                                                   : luaL_error(lua, "Qt shell cannot set layout direction");
    }
    if (action == "set-layout-right-to-left") {
        return plugin->triggerCommand(luaOptionalBool(lua, 2) ? "view.layout-rtl" : "view.layout-ltr") ? 0
                                                                                                      : luaL_error(lua, "Qt shell cannot set page order");
    }
    if (action == "set-layout-bottom-to-top") {
        return plugin->triggerCommand(luaOptionalBool(lua, 2) ? "view.layout-btt" : "view.layout-ttb") ? 0
                                                                                                      : luaL_error(lua, "Qt shell cannot set page order");
    }
    if (action == "grid-snapping" || action == "vertexnote-grid-snapping") {
        return plugin->setBooleanCommand("view.toggle-grid-snap", luaOptionalBool(lua, 2)) ? 0
                                                                                          : luaL_error(lua, "Qt shell cannot set grid snapping");
    }
    if (action == "vertexnote-geometry-snapping") {
        return plugin->setBooleanCommand("view.toggle-geometry-snap", luaOptionalBool(lua, 2)) ? 0
                                                                                              : luaL_error(lua, "Qt shell cannot set geometry snapping");
    }
    if (action == "rotation-snapping") {
        return plugin->setBooleanCommand("view.toggle-rotation-snap", luaOptionalBool(lua, 2)) ? 0
                                                                                              : luaL_error(lua, "Qt shell cannot set rotation snapping");
    }
    if (action == "tool-pen-line-style") {
        const auto command = lineStyleCommand(luaOptionalString(lua, 2));
        if (command.empty() || !plugin->triggerCommand(command)) {
            return luaL_error(lua, "Qt shell cannot set requested pen line style");
        }
        return 0;
    }

    return luaL_error(lua, "This VertexNote plugin action state is not available in the Qt shell yet: %s",
                      action.c_str());
}

constexpr luaL_Reg QT_APP_LIB[] = {
        {"registerUi", luaRegisterUi},
        {"openDialog", luaOpenDialog},
        {"msgbox", luaOpenDialog},
        {"getDocumentStructure", luaGetDocumentStructure},
        {"setCurrentPage", luaSetCurrentPage},
        {"scrollToPage", luaScrollToPage},
        {"setCurrentLayer", luaSetCurrentLayer},
        {"setLayerVisibility", luaSetLayerVisibility},
        {"setCurrentLayerName", luaSetCurrentLayerName},
        {"changeCurrentPageBackground", luaChangeCurrentPageBackground},
        {"refreshPage", luaRefreshPage},
        {"changeActionState", luaChangeActionState},
        {"activateAction", luaActivateAction},
        {"getActionState", luaUnsupported},
        {"registerPlaceholder", luaUnsupported},
        {nullptr, nullptr},
};

auto luaOpenQtApp(lua_State* lua) -> int {
    luaL_newlib(lua, QT_APP_LIB);
    lua_createtable(lua, 0, 32);
    const auto addConstant = [&](const char* name, lua_Integer value) {
        lua_pushinteger(lua, value);
        lua_setfield(lua, -2, name);
    };
    addConstant("Tool_pen", 1);
    addConstant("Tool_eraser", 2);
    addConstant("Tool_highlighter", 3);
    addConstant("Tool_text", 4);
    addConstant("Tool_selectRect", 6);
    addConstant("Tool_selectRegion", 7);
    addConstant("Tool_selectMultiLayerRect", 8);
    addConstant("Tool_selectMultiLayerRegion", 9);
    addConstant("Tool_selectObject", 10);
    addConstant("Tool_verticalSpace", 12);
    addConstant("Tool_hand", 13);
    addConstant("Tool_drawRect", 14);
    addConstant("Tool_drawEllipse", 15);
    addConstant("Tool_drawArrow", 16);
    addConstant("Tool_drawDoubleArrow", 17);
    addConstant("Tool_drawCoordinateSystem", 18);
    addConstant("Tool_drawSpline", 20);
    addConstant("Tool_selectPdfTextLinear", 21);
    addConstant("Tool_selectPdfTextRect", 22);
    addConstant("Tool_laserPointerPen", 23);
    addConstant("Tool_laserPointerHighlighter", 24);
    addConstant("OrderChange_bringToFront", 0);
    addConstant("OrderChange_bringForward", 1);
    addConstant("OrderChange_sendBackward", 2);
    addConstant("OrderChange_sendToBack", 3);
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

    QSettings appSettings(QStringLiteral("VertexNote"), QStringLiteral("VertexNoteQtShell"));
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

QtLuaPluginRuntime::QtLuaPluginRuntime(vn::ui::common::IPluginUiBridge* bridge,
                                       vn::ui::common::ICommandHost* commandHost, QWidget* parent):
        bridge(bridge), commandHost(commandHost), parent(parent) {}

QtLuaPluginRuntime::~QtLuaPluginRuntime() = default;

void QtLuaPluginRuntime::configureDocumentAccess(QtDocumentController* controller,
                                                 std::function<std::size_t()> currentPageProvider,
                                                 std::function<void(std::size_t)> pageNavigator,
                                                 std::function<void()> refreshUi, std::function<void()> markDirty) {
    this->documentController = controller;
    this->currentPageProvider = std::move(currentPageProvider);
    this->pageNavigator = std::move(pageNavigator);
    this->refreshUi = std::move(refreshUi);
    this->markDirty = std::move(markDirty);
}

void QtLuaPluginRuntime::loadEnabledPlugins() {
    for (const auto& plugin: this->plugins) {
        for (const auto& actionId: plugin->actionIds) {
            if (this->bridge) {
                this->bridge->removeAction(actionId);
            }
        }
    }
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

void QtLuaPluginRuntime::saveEnabledStates(const std::vector<std::pair<std::string, bool>>& states) {
    QSettings appSettings(QStringLiteral("VertexNote"), QStringLiteral("VertexNoteQtShell"));
    for (const auto& [name, enabled]: states) {
        appSettings.setValue(QStringLiteral("plugins/enabled/") + pluginSettingsKey(name), enabled);
    }
    appSettings.sync();
    loadEnabledPlugins();
}

auto QtLuaPluginRuntime::parentWidget() const -> QWidget* { return this->parent; }

auto QtLuaPluginRuntime::documentControllerPtr() const -> QtDocumentController* { return this->documentController; }

auto QtLuaPluginRuntime::currentDocumentPageIndex() const -> std::size_t {
    return this->currentPageProvider ? this->currentPageProvider() : 0U;
}

void QtLuaPluginRuntime::navigateToDocumentPage(std::size_t pageIndex) const {
    if (this->pageNavigator) {
        this->pageNavigator(pageIndex);
    }
}

void QtLuaPluginRuntime::refreshDocumentUi() const {
    if (this->refreshUi) {
        this->refreshUi();
    }
}

void QtLuaPluginRuntime::markDocumentDirty() const {
    if (this->markDirty) {
        this->markDirty();
    }
}

auto QtLuaPluginRuntime::statuses() const -> std::vector<PluginStatus> {
    std::vector<PluginStatus> result;
    result.reserve(this->plugins.size());
    for (const auto& plugin: this->plugins) {
        result.push_back(plugin->status());
    }
    return result;
}
