/*
 * VertexNote
 *
 * Platform-neutral command registration contract.
 */

#pragma once

#include <functional>
#include <string_view>

#include "UiTypes.h"

namespace vn::ui::common {

class ICommandHost {
public:
    using CommandHandler = std::function<void()>;

public:
    virtual ~ICommandHost() = default;

    virtual void registerCommand(CommandDescriptor descriptor, CommandHandler handler) = 0;
    virtual void setCommandEnabled(std::string_view id, bool enabled) = 0;
    virtual void setCommandChecked(std::string_view id, bool checked) = 0;
    [[nodiscard]] virtual auto hasCommand(std::string_view id) const -> bool = 0;
    virtual void triggerCommand(std::string_view id) = 0;
};

}  // namespace vn::ui::common
