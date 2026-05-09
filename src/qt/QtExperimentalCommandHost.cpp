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

    auto* menu = ensureMenu(descriptor.menu.empty() ? QStringLiteral("File") : QString::fromStdString(descriptor.menu));
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

auto QtExperimentalCommandHost::ensureMenu(const QString& title) -> QMenu* {
    if (auto it = this->menus.find(title); it != this->menus.end()) {
        return it->second;
    }

    auto* menu = this->window->menuBar()->addMenu(title);
    this->menus.emplace(title, menu);
    return menu;
}

auto QtExperimentalCommandHost::actionFor(std::string_view id) const -> QAction* {
    const auto it = this->actions.find(std::string(id));
    return it == this->actions.end() ? nullptr : it->second;
}
