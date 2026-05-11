/*
 * VertexNote
 *
 * Qt plugin UI bridge — routes plugin actions to the command host.
 */

#pragma once

#include <QPointer>

#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>

#include "ui/common/IPluginUiBridge.h"

class QAction;
class QLabel;
class QtCommandHost;
class QMainWindow;
class QToolBar;

class QtPluginUiBridge: public vn::ui::common::IPluginUiBridge {
public:
    explicit QtPluginUiBridge(QtCommandHost* commandHost, QMainWindow* window);

public:
    void registerMenuAction(const vn::ui::common::PluginUiActionDescriptor& action) override;
    void registerToolbarAction(const vn::ui::common::PluginUiActionDescriptor& action) override;
    void removeAction(std::string_view id) override;
    void registerPlaceholder(std::string_view id, std::string_view displayName,
                             std::string_view description) override;
    void setPlaceholderValue(std::string_view id, std::string_view value) override;
    void removePlaceholder(std::string_view id) override;

private:
    struct PluginPlaceholder {
        QPointer<QAction> action;
        QPointer<QLabel> label;
        std::string displayName;
    };

private:
    [[nodiscard]] auto ensurePluginToolbar() const -> QToolBar*;
    [[nodiscard]] static auto sanitizedPlaceholderText(std::string_view text) -> QString;

private:
    QtCommandHost* commandHost = nullptr;
    QMainWindow* window = nullptr;
    std::unordered_set<std::string> registeredActions;
    std::unordered_map<std::string, PluginPlaceholder> placeholders;
};
