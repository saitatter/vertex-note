/*
 * VertexNote
 *
 * Qt plugin UI bridge — routes plugin actions to the command host.
 */

#pragma once

#include <string>
#include <unordered_set>

#include "ui/common/IPluginUiBridge.h"

class QtCommandHost;
class QMainWindow;

class QtPluginUiBridge: public vn::ui::common::IPluginUiBridge {
public:
    explicit QtPluginUiBridge(QtCommandHost* commandHost, QMainWindow* window);

public:
    void registerMenuAction(const vn::ui::common::PluginUiActionDescriptor& action) override;
    void registerToolbarAction(const vn::ui::common::PluginUiActionDescriptor& action) override;
    void removeAction(std::string_view id) override;

private:
    QtCommandHost* commandHost = nullptr;
    QMainWindow* window = nullptr;
    std::unordered_set<std::string> registeredActions;
};
