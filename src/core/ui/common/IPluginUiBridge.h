/*
 * VertexNote
 *
 * Platform-neutral plugin UI bridge contract.
 */

#pragma once

#include <string_view>

#include "UiTypes.h"

namespace vn::ui::common {

class IPluginUiBridge {
public:
    virtual ~IPluginUiBridge() = default;

    virtual void registerMenuAction(const PluginUiActionDescriptor& action) = 0;
    virtual void registerToolbarAction(const PluginUiActionDescriptor& action) = 0;
    virtual void removeAction(std::string_view id) = 0;
};

}  // namespace vn::ui::common
