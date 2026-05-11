/*
 * VertexNote
 *
 * Experimental Qt command host bootstrap.
 */

#include "QtCommandHost.h"

#include <QAction>
#include <QKeySequence>
#include <QMainWindow>
#include <QMenu>
#include <QMenuBar>
#include <QString>

QtCommandHost::QtCommandHost(QMainWindow* window): window(window) {}

void QtCommandHost::registerCommand(vn::ui::common::CommandDescriptor descriptor, CommandHandler handler) {
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

void QtCommandHost::setCommandEnabled(std::string_view id, bool enabled) {
    if (auto* action = actionFor(id)) {
        action->setEnabled(enabled);
    }
}

void QtCommandHost::setCommandChecked(std::string_view id, bool checked) {
    if (auto* action = actionFor(id)) {
        action->setChecked(checked);
    }
}

auto QtCommandHost::hasCommand(std::string_view id) const -> bool {
    return this->actions.contains(std::string(id));
}

auto QtCommandHost::isCommandChecked(std::string_view id) const -> bool {
    if (auto* action = actionFor(id)) {
        return action->isChecked();
    }
    return false;
}

void QtCommandHost::triggerCommand(std::string_view id) {
    if (auto* action = actionFor(id)) {
        action->trigger();
    }
}

void QtCommandHost::removeCommand(std::string_view id) {
    const std::string key(id);
    auto it = this->actions.find(key);
    if (it == this->actions.end()) {
        return;
    }
    auto* action = it->second;
    this->actions.erase(it);
    delete action;
}

auto QtCommandHost::actionForCommand(std::string_view id) const -> QAction* { return actionFor(id); }

auto QtCommandHost::menuForPath(std::string_view menuPath) -> QMenu* { return ensureMenu(menuPath); }

void QtCommandHost::addMenuSeparator(std::string_view menuPath) {
    auto* menu = ensureMenu(menuPath);
    menu->addSeparator();
}

auto QtCommandHost::ensureMenu(std::string_view menuPath) -> QMenu* {
    const std::string key(menuPath);
    if (auto it = this->menus.find(key); it != this->menus.end()) {
        return it->second;
    }

    // Check for submenu separator '>'
    const auto sep = key.find('>');
    if (sep != std::string::npos) {
        auto* parent = ensureMenu(key.substr(0, sep));
        auto* sub = parent->addMenu(
                QString::fromUtf8(key.data() + static_cast<std::ptrdiff_t>(sep) + 1,
                                  static_cast<int>(key.size() - sep - 1)));
        this->menus.emplace(key, sub);
        return sub;
    }

    auto* menu = this->window->menuBar()->addMenu(QString::fromUtf8(key.data(), static_cast<int>(key.size())));
    this->menus.emplace(key, menu);
    return menu;
}

auto QtCommandHost::actionFor(std::string_view id) const -> QAction* {
    const auto it = this->actions.find(std::string(id));
    return it == this->actions.end() ? nullptr : it->second;
}
