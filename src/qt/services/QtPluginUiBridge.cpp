/*
 * VertexNote
 *
 * Qt plugin UI bridge — routes plugin actions to the command host.
 */

#include "QtPluginUiBridge.h"

#include <algorithm>

#include <QAction>
#include <QLabel>
#include <QMainWindow>
#include <QString>
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
        if (auto* toolbar = ensurePluginToolbar()) {
            toolbar->addAction(toolbarAction);
        }
    }
}

void QtPluginUiBridge::removeAction(std::string_view id) {
    if (!this->commandHost) {
        return;
    }
    this->commandHost->removeCommand(id);
    this->registeredActions.erase(std::string(id));
}

void QtPluginUiBridge::registerPlaceholder(std::string_view id, std::string_view displayName,
                                           std::string_view description) {
    if (!this->window) {
        return;
    }

    const std::string key(id);
    const std::string display(displayName);
    if (auto it = this->placeholders.find(key); it != this->placeholders.end()) {
        it->second.displayName = display;
        if (it->second.label) {
            it->second.label->setToolTip(QString::fromUtf8(description.data(), static_cast<int>(description.size())));
            if (it->second.label->text().isEmpty()) {
                it->second.label->setText(sanitizedPlaceholderText(display));
            }
        }
        return;
    }

    auto* label = new QLabel(sanitizedPlaceholderText(display), this->window);
    label->setObjectName(QStringLiteral("vertexNoteQtPluginPlaceholder"));
    label->setMinimumWidth(80);
    label->setMaximumWidth(220);
    label->setTextInteractionFlags(Qt::NoTextInteraction);
    label->setToolTip(QString::fromUtf8(description.data(), static_cast<int>(description.size())));

    auto* toolbar = ensurePluginToolbar();
    if (!toolbar) {
        delete label;
        return;
    }
    auto* action = toolbar->addWidget(label);
    action->setObjectName(QString::fromUtf8(key.data(), static_cast<int>(key.size())));
    this->placeholders.emplace(key, PluginPlaceholder{.action = action, .label = label, .displayName = display});
}

void QtPluginUiBridge::setPlaceholderValue(std::string_view id, std::string_view value) {
    const std::string key(id);
    auto it = this->placeholders.find(key);
    if (it == this->placeholders.end() || !it->second.label) {
        return;
    }
    const auto text = value.empty() ? std::string_view(it->second.displayName) : value;
    it->second.label->setText(sanitizedPlaceholderText(text));
}

void QtPluginUiBridge::removePlaceholder(std::string_view id) {
    const std::string key(id);
    auto it = this->placeholders.find(key);
    if (it == this->placeholders.end()) {
        return;
    }
    if (it->second.action) {
        delete it->second.action;
    } else if (it->second.label) {
        delete it->second.label;
    }
    this->placeholders.erase(it);
}

auto QtPluginUiBridge::ensurePluginToolbar() const -> QToolBar* {
    QToolBar* pluginToolbar = nullptr;
    if (this->window) {
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
    }
    return pluginToolbar;
}

auto QtPluginUiBridge::sanitizedPlaceholderText(std::string_view text) -> QString {
    std::string sanitized(text);
    std::replace(sanitized.begin(), sanitized.end(), '\n', ' ');
    std::replace(sanitized.begin(), sanitized.end(), '\r', ' ');
    return QString::fromStdString(sanitized);
}
