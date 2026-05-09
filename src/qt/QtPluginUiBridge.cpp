/*
 * VertexNote
 *
 * Experimental Qt plugin UI bridge.
 */

#include "QtPluginUiBridge.h"

#include "QtCommandHost.h"

QtPluginUiBridge::QtPluginUiBridge(QtCommandHost* commandHost): commandHost(commandHost) {}

void QtPluginUiBridge::registerMenuAction(const vn::ui::common::PluginUiActionDescriptor& action) {
    if (!this->commandHost) {
        return;
    }

    this->commandHost->registerCommand({.id = action.id,
                                        .text = action.label,
                                        .tooltip = action.tooltip,
                                        .menu = "Plugins"},
                                       []() {});
}

void QtPluginUiBridge::registerToolbarAction(const vn::ui::common::PluginUiActionDescriptor& action) {
    registerMenuAction(action);
}

void QtPluginUiBridge::removeAction(std::string_view id) {
    if (this->commandHost && this->commandHost->hasCommand(id)) {
        this->commandHost->setCommandEnabled(id, false);
    }
}
