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
void QtAppShell::registerNavigationCommands() {
    auto* ch = this->window.commandHost();

    // =====================================================================
    // Menu 4: Navigation
    // =====================================================================
    ch->registerCommand(
            {.id = "nav.first-page", .text = "First Page", .tooltip = "Go to first page", .shortcut = "Ctrl+Home", .menu = "Navigation"},
            [this]() { goToFirstPage(); });
    ch->registerCommand(
            {.id = "nav.prev-page", .text = "Previous Page", .tooltip = "Go to previous page", .shortcut = "Ctrl+PgUp", .menu = "Navigation"},
            [this]() { goToPreviousPage(); });
    ch->registerCommand(
            {.id = "nav.back", .text = "Jump Back", .tooltip = "Go back in navigation history", .shortcut = "Alt+Left", .menu = "Navigation"},
            [this]() { navigateBack(); });
    ch->registerCommand(
            {.id = "nav.goto-page", .text = "Go to Page...", .tooltip = "Jump to specific page", .shortcut = "Ctrl+G", .menu = "Navigation"},
            [this]() { goToPageDialog(); });
    ch->registerCommand(
            {.id = "nav.forward", .text = "Jump Forward", .tooltip = "Go forward in navigation history", .shortcut = "Alt+Right", .menu = "Navigation"},
            [this]() { navigateForward(); });
    ch->registerCommand(
            {.id = "nav.next-page", .text = "Next Page", .tooltip = "Go to next page", .shortcut = "Ctrl+PgDown", .menu = "Navigation"},
            [this]() { goToNextPage(); });
    ch->registerCommand(
            {.id = "nav.last-page", .text = "Last Page", .tooltip = "Go to last page", .shortcut = "Ctrl+End", .menu = "Navigation"},
            [this]() { goToLastPage(); });
    ch->addMenuSeparator("Navigation");
    ch->registerCommand(
            {.id = "layer.goto-prev", .text = "Previous Layer", .tooltip = "Switch to layer below", .shortcut = "Shift+PgDown", .menu = "Navigation"},
            [this]() { gotoPrevLayer(); });
    ch->registerCommand(
            {.id = "layer.goto-next", .text = "Next Layer", .tooltip = "Switch to layer above", .shortcut = "Shift+PgUp", .menu = "Navigation"},
            [this]() { gotoNextLayer(); });
    ch->registerCommand(
            {.id = "layer.goto-top", .text = "Top Layer", .tooltip = "Switch to topmost layer", .menu = "Navigation"},
            [this]() { gotoTopLayer(); });
    ch->addMenuSeparator("Navigation");
    ch->registerCommand(
            {.id = "nav.next-annotated", .text = "Next Annotated Page", .tooltip = "Jump to next annotated page",
             .shortcut = "Ctrl+Shift+PgDown", .menu = "Navigation"},
            [this]() { gotoNextAnnotatedPage(); });
    ch->registerCommand(
            {.id = "nav.prev-annotated", .text = "Previous Annotated Page", .tooltip = "Jump to previous annotated page",
             .shortcut = "Ctrl+Shift+PgUp", .menu = "Navigation"},
            [this]() { gotoPrevAnnotatedPage(); });

}
