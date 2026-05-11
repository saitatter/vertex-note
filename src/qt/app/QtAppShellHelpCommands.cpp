/*
 * VertexNote
 *
 * Qt app shell command registration.
 */

#include "QtAppShell.h"

#include <string>

#include <QDesktopServices>
#include <QInputDialog>
#include <QStatusBar>
#include <QString>
#include <QUrl>
void QtAppShell::registerHelpCommands() {
    auto* ch = this->window.commandHost();

    // =====================================================================
    // Menu 7: Help
    // =====================================================================
    ch->registerCommand(
            {.id = "plugins.manager", .text = "Plugin Manager...", .tooltip = "Show Qt plugin runtime status",
             .menu = "Plugins"},
            [this]() { showPluginManagerDialog(); });
    ch->registerCommand(
            {.id = "help.open", .text = "Help", .tooltip = "Open the VertexNote help page", .menu = "Help"},
            []() { QDesktopServices::openUrl(QUrl(QStringLiteral("https://github.com/saitatter/vertex-note"))); });
    ch->registerCommand(
            {.id = "app.check-updates", .text = "Check for Updates", .tooltip = "Check for new versions", .menu = "Help"},
            [this]() { checkForUpdates(false); });
    ch->registerCommand(
            {.id = "app.about-qt-shell", .text = "About VertexNote", .tooltip = "About this application", .menu = "Help"},
            [this]() {
                this->dialogs.showInfo("About VertexNote",
                                       "VertexNote Qt Shell\n\n"
                                       "A modern note-taking application with geometry, "
                                       "PDF annotation, and handwriting support.");
            });
}
