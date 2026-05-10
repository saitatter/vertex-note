/*
 * VertexNote
 *
 * Qt plugin UI bridge — routes plugin actions to the command host.
 */

#include "QtPluginUiBridge.h"

#include <QMainWindow>
#include <QToolBar>

#include "QtCommandHost.h"

QtPluginUiBridge::QtPluginUiBridge(QtCommandHost* commandHost, QMainWindow* window):
        commandHost(commandHost), window(window) {}

void QtPluginUiBridge::registerMenuAction(const vn::ui::common::PluginUiActionDescriptor& action) {
    if (!this->commandHost) {
        return;
    }

    auto callback = action.callback;
    this->commandHost->registerCommand({.id = action.id,
                                        .text = action.label,
                                        .tooltip = action.tooltip,
                                        .shortcut = action.shortcut,
                                        .menu = "Plugins"},
                                       std::move(callback));
    this->registeredActions.insert(action.id);
}

void QtPluginUiBridge::registerToolbarAction(const vn::ui::common::PluginUiActionDescriptor& action) {
    if (!this->commandHost) {
        return;
    }

    // Register as command (adds to Plugins menu)
    registerMenuAction(action);

    // Also add to toolbar
    if (auto* toolbarAction = this->commandHost->actionForCommand(action.id)) {
        // Find or create a "Plugins" toolbar
        QToolBar* pluginToolbar = nullptr;
        for (auto* tb: this->window->findChildren<QToolBar*>()) {
            if (tb->objectName() == QStringLiteral("PluginsToolbar")) {
                pluginToolbar = tb;
                break;
            }
        }
        if (!pluginToolbar) {
            pluginToolbar = this->window->addToolBar(QStringLiteral("Plugins"));
            pluginToolbar->setObjectName(QStringLiteral("PluginsToolbar"));
        }
        pluginToolbar->addAction(toolbarAction);
    }
}

void QtPluginUiBridge::removeAction(std::string_view id) {
    if (!this->commandHost) {
        return;
    }
    this->commandHost->removeCommand(id);
    this->registeredActions.erase(std::string(id));
}
