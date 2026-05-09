/*
 * VertexNote
 *
 * Experimental Qt app shell bootstrap.
 */

#pragma once

#include "QtExperimentalClipboardService.h"
#include "QtExperimentalDialogService.h"
#include "QtExperimentalDocumentController.h"
#include "QtExperimentalDocumentSession.h"
#include "QtExperimentalMainWindow.h"
#include "QtExperimentalPluginUiBridge.h"
#include "QtExperimentalRecentFilesService.h"
#include "QtExperimentalUpdatePresentationService.h"
#include "ui/common/IAppShell.h"

class QtExperimentalAppShell: public vn::ui::common::IAppShell {
public:
    QtExperimentalAppShell();

public:
    [[nodiscard]] auto commandHost() -> vn::ui::common::ICommandHost* override;
    [[nodiscard]] auto canvasHost() -> vn::ui::common::ICanvasHost* override;
    [[nodiscard]] auto clipboardService() -> vn::ui::common::IClipboardService* override;
    [[nodiscard]] auto dialogService() -> vn::ui::common::IDialogService* override;
    [[nodiscard]] auto recentFilesService() -> vn::ui::common::IRecentFilesService* override;
    [[nodiscard]] auto updatePresentationService() -> vn::ui::common::IUpdatePresentationService* override;
    [[nodiscard]] auto pluginUiBridge() -> vn::ui::common::IPluginUiBridge* override;
    [[nodiscard]] auto nativeMainWindowHandle() const -> void* override;

    void showMainWindow() override;
    void requestQuit() override;
    void setMainWindowTitle(std::string_view title) override;

private:
    void registerBootstrapCommands();
    void wireWindowState();
    void rebuildToolbar();
    void updateWindowTitle();
    void newSession();
    void openSession();
    void saveSessionAs();
    void markSessionDirty();

private:
    QtExperimentalMainWindow window;
    QtExperimentalClipboardService clipboard;
    QtExperimentalDialogService dialogs;
    QtExperimentalRecentFilesService recentFiles;
    QtExperimentalUpdatePresentationService updates;
    QtExperimentalPluginUiBridge plugins;
    QtExperimentalDocumentController documentController;
    QtExperimentalDocumentSession session;
    bool suppressDirtyTracking = false;
};
