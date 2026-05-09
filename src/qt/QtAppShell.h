/*
 * VertexNote
 *
 * Qt app shell bootstrap.
 */

#pragma once

#include "QtClipboardService.h"
#include "QtDialogService.h"
#include "QtDocumentController.h"
#include "QtDocumentExporter.h"
#include "QtDocumentSession.h"
#include "QtMainWindow.h"
#include "QtPluginUiBridge.h"
#include "QtRecentFilesService.h"
#include "QtSettingsDialog.h"
#include "QtToolState.h"
#include "QtUpdatePresentationService.h"
#include "ui/common/IAppShell.h"

class QtAppShell: public vn::ui::common::IAppShell {
public:
    QtAppShell();

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
    void updateEditCommandStates();
    void setGeometrySnapEnabled(bool enabled);
    void setGridSnapEnabled(bool enabled);
    void selectTool(QtToolType tool);
    void updateToolCommandStates();
    void showBackgroundDialog();
    void exportPdf();
    void exportPng();
    void toggleFullscreen();
    void togglePresentationMode();
    void saveDocument();
    void printDocument();
    void addPage();
    void deletePage();
    void duplicatePage();
    void findText();
    void insertImage();
    void showSettingsDialog();
    void applyConstraint(vn::geom::ConstraintKind kind);
    void deleteConstraints();
    void editFixedLengthConstraint();

private:
    QtMainWindow window;
    QtClipboardService clipboard;
    QtDialogService dialogs;
    QtRecentFilesService recentFiles;
    QtUpdatePresentationService updates;
    QtPluginUiBridge plugins;
    QtDocumentController documentController;
    QtDocumentSession session;
    std::unique_ptr<QtDocumentExporter> exporter;
    bool suppressDirtyTracking = false;
    bool presentationMode = false;
    QtSettings currentSettings;
};
