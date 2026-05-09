/*
 * VertexNote
 *
 * Experimental Qt plugin UI bridge.
 */

#include "QtExperimentalPluginUiBridge.h"

#include "QtExperimentalCommandHost.h"

QtExperimentalPluginUiBridge::QtExperimentalPluginUiBridge(QtExperimentalCommandHost* commandHost): commandHost(commandHost) {}

void QtExperimentalPluginUiBridge::registerMenuAction(const vn::ui::common::PluginUiActionDescriptor& action) {
    if (!this->commandHost) {
        return;
    }

    this->commandHost->registerCommand({.id = action.id,
                                        .text = action.label,
                                        .tooltip = action.tooltip,
                                        .menu = "Plugins"},
                                       []() {});
}

void QtExperimentalPluginUiBridge::registerToolbarAction(const vn::ui::common::PluginUiActionDescriptor& action) {
    registerMenuAction(action);
}

void QtExperimentalPluginUiBridge::removeAction(std::string_view id) {
    if (this->commandHost && this->commandHost->hasCommand(id)) {
        this->commandHost->setCommandEnabled(id, false);
    }
}
