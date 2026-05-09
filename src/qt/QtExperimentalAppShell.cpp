/*
 * VertexNote
 *
 * Experimental Qt app shell bootstrap.
 */

#include "QtExperimentalAppShell.h"

#include <QApplication>
#include <QString>

QtExperimentalAppShell::QtExperimentalAppShell() { registerBootstrapCommands(); }

auto QtExperimentalAppShell::commandHost() -> vn::ui::common::ICommandHost* { return this->window.commandHost(); }

auto QtExperimentalAppShell::canvasHost() -> vn::ui::common::ICanvasHost* { return this->window.canvas(); }

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
}
