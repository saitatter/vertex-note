/*
 * VertexNote
 *
 * Experimental Qt plugin UI bridge.
 */

#pragma once

#include "ui/common/IPluginUiBridge.h"

class QtExperimentalCommandHost;

class QtExperimentalPluginUiBridge: public vn::ui::common::IPluginUiBridge {
public:
    explicit QtExperimentalPluginUiBridge(QtExperimentalCommandHost* commandHost);

public:
    void registerMenuAction(const vn::ui::common::PluginUiActionDescriptor& action) override;
    void registerToolbarAction(const vn::ui::common::PluginUiActionDescriptor& action) override;
    void removeAction(std::string_view id) override;

private:
    QtExperimentalCommandHost* commandHost = nullptr;
};
