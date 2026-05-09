/*
 * VertexNote
 *
 * Experimental Qt command host bootstrap.
 */

#include "QtExperimentalCommandHost.h"

#include <QAction>
#include <QKeySequence>
#include <QMainWindow>
#include <QMenu>
#include <QMenuBar>
#include <QString>

QtExperimentalCommandHost::QtExperimentalCommandHost(QMainWindow* window): window(window) {}

void QtExperimentalCommandHost::registerCommand(vn::ui::common::CommandDescriptor descriptor, CommandHandler handler) {
    auto* action = new QAction(QString::fromStdString(descriptor.text), this->window);
    action->setEnabled(descriptor.enabled);
    action->setCheckable(descriptor.checkable);
    action->setChecked(descriptor.checked);
    action->setToolTip(QString::fromStdString(descriptor.tooltip));
    if (!descriptor.shortcut.empty()) {
        action->setShortcut(QKeySequence(QString::fromStdString(descriptor.shortcut)));
    }
    QObject::connect(action, &QAction::triggered, this->window, [handler = std::move(handler)]() {
        if (handler) {
            handler();
        }
    });

    auto* menu = ensureMenu(descriptor.menu.empty() ? std::string_view("File") : std::string_view(descriptor.menu));
    menu->addAction(action);
    this->actions.insert_or_assign(std::move(descriptor.id), action);
}

void QtExperimentalCommandHost::setCommandEnabled(std::string_view id, bool enabled) {
    if (auto* action = actionFor(id)) {
        action->setEnabled(enabled);
    }
}

void QtExperimentalCommandHost::setCommandChecked(std::string_view id, bool checked) {
    if (auto* action = actionFor(id)) {
        action->setChecked(checked);
    }
}

auto QtExperimentalCommandHost::hasCommand(std::string_view id) const -> bool {
    return this->actions.contains(std::string(id));
}

void QtExperimentalCommandHost::triggerCommand(std::string_view id) {
    if (auto* action = actionFor(id)) {
        action->trigger();
    }
}

auto QtExperimentalCommandHost::actionForCommand(std::string_view id) const -> QAction* { return actionFor(id); }

auto QtExperimentalCommandHost::ensureMenu(std::string_view title) -> QMenu* {
    const std::string key(title);
    if (auto it = this->menus.find(key); it != this->menus.end()) {
        return it->second;
    }

    auto* menu = this->window->menuBar()->addMenu(QString::fromUtf8(key.data(), static_cast<int>(key.size())));
    this->menus.emplace(key, menu);
    return menu;
}

auto QtExperimentalCommandHost::actionFor(std::string_view id) const -> QAction* {
    const auto it = this->actions.find(std::string(id));
    return it == this->actions.end() ? nullptr : it->second;
}
