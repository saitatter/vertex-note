/*
 * VertexNote
 *
 * Experimental Qt command host bootstrap.
 */

#pragma once

#include <string>
#include <unordered_map>

#include "ui/common/ICommandHost.h"

class QMainWindow;
class QMenu;
class QAction;

class QtCommandHost: public vn::ui::common::ICommandHost {
public:
    explicit QtCommandHost(QMainWindow* window);

public:
    void registerCommand(vn::ui::common::CommandDescriptor descriptor, CommandHandler handler) override;
    void setCommandEnabled(std::string_view id, bool enabled) override;
    void setCommandChecked(std::string_view id, bool checked) override;
    [[nodiscard]] auto hasCommand(std::string_view id) const -> bool override;
    [[nodiscard]] auto isCommandChecked(std::string_view id) const -> bool override;
    void triggerCommand(std::string_view id) override;
    void removeCommand(std::string_view id);
    [[nodiscard]] auto actionForCommand(std::string_view id) const -> QAction*;
    [[nodiscard]] auto menuForPath(std::string_view menuPath) -> QMenu*;
    void addMenuSeparator(std::string_view menuPath);

private:
    [[nodiscard]] auto ensureMenu(std::string_view menuPath) -> QMenu*;
    [[nodiscard]] auto actionFor(std::string_view id) const -> QAction*;

private:
    QMainWindow* window = nullptr;
    std::unordered_map<std::string, QAction*> actions;
    std::unordered_map<std::string, QMenu*> menus;
};
