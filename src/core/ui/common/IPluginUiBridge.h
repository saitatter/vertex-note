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
    virtual void registerPlaceholder(std::string_view id, std::string_view displayName,
                                     std::string_view description) = 0;
    virtual void setPlaceholderValue(std::string_view id, std::string_view value) = 0;
    virtual void removePlaceholder(std::string_view id) = 0;
};

}  // namespace vn::ui::common
