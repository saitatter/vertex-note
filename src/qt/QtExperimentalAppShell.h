/*
 * VertexNote
 *
 * Experimental Qt app shell bootstrap.
 */

#pragma once

#include "QtExperimentalMainWindow.h"
#include "ui/common/IAppShell.h"

class QtExperimentalAppShell: public vn::ui::common::IAppShell {
public:
    QtExperimentalAppShell();

public:
    [[nodiscard]] auto commandHost() -> vn::ui::common::ICommandHost* override;
    [[nodiscard]] auto canvasHost() -> vn::ui::common::ICanvasHost* override;
    [[nodiscard]] auto nativeMainWindowHandle() const -> void* override;

    void showMainWindow() override;
    void requestQuit() override;
    void setMainWindowTitle(std::string_view title) override;

private:
    void registerBootstrapCommands();

private:
    QtExperimentalMainWindow window;
};
