/*
 * VertexNote
 *
 * Experimental Qt app shell bootstrap.
 */

#include "QtExperimentalAppShell.h"

#include <QApplication>
#include <QStatusBar>
#include <QString>

QtExperimentalAppShell::QtExperimentalAppShell():
        dialogs(&this->window),
        updates(&this->window, this->window.statusBar()),
        plugins(this->window.commandHost()) {
    registerBootstrapCommands();
}

auto QtExperimentalAppShell::commandHost() -> vn::ui::common::ICommandHost* { return this->window.commandHost(); }

auto QtExperimentalAppShell::canvasHost() -> vn::ui::common::ICanvasHost* { return this->window.canvas(); }

auto QtExperimentalAppShell::clipboardService() -> vn::ui::common::IClipboardService* { return &this->clipboard; }

auto QtExperimentalAppShell::dialogService() -> vn::ui::common::IDialogService* { return &this->dialogs; }

auto QtExperimentalAppShell::recentFilesService() -> vn::ui::common::IRecentFilesService* { return &this->recentFiles; }

auto QtExperimentalAppShell::updatePresentationService() -> vn::ui::common::IUpdatePresentationService* {
    return &this->updates;
}

auto QtExperimentalAppShell::pluginUiBridge() -> vn::ui::common::IPluginUiBridge* { return &this->plugins; }

auto QtExperimentalAppShell::nativeMainWindowHandle() const -> void* {
    return reinterpret_cast<void*>(const_cast<QtExperimentalMainWindow*>(&this->window));
}

void QtExperimentalAppShell::showMainWindow() { this->window.show(); }

void QtExperimentalAppShell::requestQuit() { QApplication::quit(); }

void QtExperimentalAppShell::setMainWindowTitle(std::string_view title) {
    this->window.setWindowTitle(QString::fromUtf8(title.data(), static_cast<int>(title.size())));
}

void QtExperimentalAppShell::registerBootstrapCommands() {
    this->window.commandHost()->registerCommand(
            {.id = "app.quit",
             .text = "Quit",
             .tooltip = "Close VertexNote",
             .shortcut = "Ctrl+Q",
             .menu = "File"},
            [this]() { requestQuit(); });

    this->window.commandHost()->registerCommand(
            {.id = "app.check-updates",
             .text = "Check for Updates",
             .tooltip = "Show the experimental Qt updater surface",
             .menu = "Help"},
            [this]() {
                this->updates.showCheckingForUpdates();
                this->updates.showUpToDate("qt-bootstrap");
            });

    this->window.commandHost()->registerCommand(
            {.id = "app.about-qt-shell",
             .text = "About Qt Shell",
             .tooltip = "Show the experimental Qt shell status",
             .menu = "Help"},
            [this]() {
                this->dialogs.showInfo("VertexNote Qt Experimental Shell",
                                       "Qt shell bootstrap is active.\n\n"
                                       "Current slice includes neutral UI services, input translation, and a "
                                       "render seam scaffold.");
            });
}
