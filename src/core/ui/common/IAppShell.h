/*
 * VertexNote
 *
 * Platform-neutral app shell contract.
 */

#pragma once

#include <string_view>

namespace vn::ui::common {

class ICommandHost;
class ICanvasHost;

class IAppShell {
public:
    virtual ~IAppShell() = default;

    [[nodiscard]] virtual auto commandHost() -> ICommandHost* = 0;
    [[nodiscard]] virtual auto canvasHost() -> ICanvasHost* = 0;
    [[nodiscard]] virtual auto nativeMainWindowHandle() const -> void* = 0;

    virtual void showMainWindow() = 0;
    virtual void requestQuit() = 0;
    virtual void setMainWindowTitle(std::string_view title) = 0;
};

}  // namespace vn::ui::common
