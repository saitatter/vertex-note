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
void QtAppShell::registerEditCommands() {
    auto* ch = this->window.commandHost();
    const bool hasSelection = this->documentController.selectionBounds().has_value();

    // =====================================================================
    // Menu 2: Edit
    // =====================================================================
    // Arrange Selection submenu
    ch->registerCommand(
            {.id = "edit.bring-to-front", .text = "Bring to Front", .tooltip = "Bring to front", .shortcut = "Ctrl+Shift+F",
             .menu = "Edit>Arrange", .enabled = hasSelection},
            [this]() { bringToFront(); });
    ch->registerCommand(
            {.id = "edit.bring-forward", .text = "Bring Forward", .tooltip = "Move forward one step",
             .menu = "Edit>Arrange", .enabled = hasSelection},
            [this]() { bringForward(); });
    ch->registerCommand(
            {.id = "edit.send-backward", .text = "Send Backward", .tooltip = "Move backward one step",
             .menu = "Edit>Arrange", .enabled = hasSelection},
            [this]() { sendBackward(); });
    ch->registerCommand(
            {.id = "edit.send-to-back", .text = "Send to Back", .tooltip = "Send to back", .shortcut = "Ctrl+Shift+B",
             .menu = "Edit>Arrange", .enabled = hasSelection},
            [this]() { sendToBack(); });
    ch->addMenuSeparator("Edit");

    ch->registerCommand(
            {.id = "edit.undo-geometry", .text = "Undo", .tooltip = "Undo the last edit", .shortcut = "Ctrl+Z",
             .menu = "Edit", .enabled = this->window.canvas()->canUndo()},
            [this]() {
                if (this->window.canvas()->performUndo()) {
                    this->window.statusBar()->showMessage(QStringLiteral("Undid edit"), 3000);
                    updateEditCommandStates();
                }
            });
    ch->registerCommand(
            {.id = "edit.redo-geometry", .text = "Redo", .tooltip = "Redo the last edit", .shortcut = "Ctrl+Shift+Z",
             .menu = "Edit", .enabled = this->window.canvas()->canRedo()},
            [this]() {
                if (this->window.canvas()->performRedo()) {
                    this->window.statusBar()->showMessage(QStringLiteral("Redid edit"), 3000);
                    updateEditCommandStates();
                }
            });
    ch->addMenuSeparator("Edit");
    ch->registerCommand(
            {.id = "edit.cut", .text = "Cut", .tooltip = "Cut selected elements", .shortcut = "Ctrl+X", .menu = "Edit",
             .enabled = hasSelection},
            [this]() { cutSelection(); });
    ch->registerCommand(
            {.id = "edit.copy", .text = "Copy", .tooltip = "Copy selected elements", .shortcut = "Ctrl+C", .menu = "Edit",
             .enabled = hasSelection},
            [this]() { copySelection(); });
    ch->registerCommand(
            {.id = "edit.paste", .text = "Paste", .tooltip = "Paste from clipboard", .shortcut = "Ctrl+V", .menu = "Edit"},
            [this]() { pasteClipboard(); });
    ch->addMenuSeparator("Edit");
    ch->registerCommand(
            {.id = "edit.select-all", .text = "Select All", .tooltip = "Select all elements on the current page",
             .shortcut = "Ctrl+A", .menu = "Edit"},
            [this]() { selectAll(); });
    ch->addMenuSeparator("Edit");
    ch->registerCommand(
            {.id = "edit.find", .text = "Find", .tooltip = "Search for text", .shortcut = "Ctrl+F", .menu = "Edit"},
            [this]() { findText(); });
    ch->registerCommand(
            {.id = "edit.delete", .text = "Delete", .tooltip = "Delete selected elements", .shortcut = "Delete", .menu = "Edit",
             .enabled = hasSelection},
            [this]() { deleteSelection(); });
    ch->addMenuSeparator("Edit");

    ch->registerCommand(
            {.id = "edit.move-selection-layer-up", .text = "Move Selection Layer Up",
             .tooltip = "Move selected elements up one layer", .menu = "Edit"},
            [this]() { moveSelectionLayerUp(); });
    ch->registerCommand(
            {.id = "edit.move-selection-layer-down", .text = "Move Selection Layer Down",
             .tooltip = "Move selected elements down one layer", .menu = "Edit"},
            [this]() { moveSelectionLayerDown(); });
    ch->addMenuSeparator("Edit");

    // Snapping toggles
    ch->registerCommand(
            {.id = "view.toggle-rotation-snap", .text = "Rotation Snapping", .tooltip = "Toggle angular snapping for shape tools",
             .menu = "Edit", .checkable = true, .checked = this->window.canvas()->isRotationSnapEnabled()},
            [this]() {
                const bool enabled = !this->window.canvas()->isRotationSnapEnabled();
                this->window.canvas()->setRotationSnapEnabled(enabled);
                this->window.commandHost()->setCommandChecked("view.toggle-rotation-snap", enabled);
                this->window.statusBar()->showMessage(
                        enabled ? QStringLiteral("Rotation snapping enabled")
                                : QStringLiteral("Rotation snapping disabled"),
                        2500);
            });
    ch->registerCommand(
            {.id = "view.toggle-grid-snap", .text = "Snap to Grid", .tooltip = "Snap drawing and geometry edits to the page grid",
             .menu = "Edit", .checkable = true, .checked = this->window.canvas()->isGridSnapEnabled()},
            [this]() { setGridSnapEnabled(!this->window.canvas()->isGridSnapEnabled()); });
    ch->addMenuSeparator("Edit");
    ch->registerCommand(
            {.id = "app.settings", .text = "Preferences", .tooltip = "Open settings", .menu = "Edit"},
            [this]() { showSettingsDialog(); });

}
