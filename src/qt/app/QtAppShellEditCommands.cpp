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

    // =====================================================================
    // Menu 2: Edit
    // =====================================================================
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
            {.id = "edit.redo-geometry", .text = "Redo", .tooltip = "Redo the last edit", .shortcut = "Ctrl+Y",
             .menu = "Edit", .enabled = this->window.canvas()->canRedo()},
            [this]() {
                if (this->window.canvas()->performRedo()) {
                    this->window.statusBar()->showMessage(QStringLiteral("Redid edit"), 3000);
                    updateEditCommandStates();
                }
            });
    ch->addMenuSeparator("Edit");
    ch->registerCommand(
            {.id = "edit.cut", .text = "Cut", .tooltip = "Cut selected elements", .shortcut = "Ctrl+X", .menu = "Edit"},
            [this]() { cutSelection(); });
    ch->registerCommand(
            {.id = "edit.copy", .text = "Copy", .tooltip = "Copy selected elements", .shortcut = "Ctrl+C", .menu = "Edit"},
            [this]() { copySelection(); });
    ch->registerCommand(
            {.id = "edit.paste", .text = "Paste", .tooltip = "Paste from clipboard", .shortcut = "Ctrl+V", .menu = "Edit"},
            [this]() { pasteClipboard(); });
    ch->addMenuSeparator("Edit");
    ch->registerCommand(
            {.id = "edit.select-all", .text = "Select All", .tooltip = "Select all elements on the current page",
             .shortcut = "Ctrl+A", .menu = "Edit"},
            [this]() { selectAll(); });
    ch->registerCommand(
            {.id = "edit.find", .text = "Find...", .tooltip = "Search for text", .shortcut = "Ctrl+F", .menu = "Edit"},
            [this]() { findText(); });
    ch->registerCommand(
            {.id = "edit.delete", .text = "Delete", .tooltip = "Delete selected elements", .shortcut = "Delete", .menu = "Edit"},
            [this]() { deleteSelection(); });
    ch->addMenuSeparator("Edit");

    // Arrange Selection submenu
    ch->registerCommand(
            {.id = "edit.bring-to-front", .text = "Bring to Front", .tooltip = "Bring to front", .shortcut = "Ctrl+Shift+F",
             .menu = "Edit>Arrange Selection"},
            [this]() { bringToFront(); });
    ch->registerCommand(
            {.id = "edit.bring-forward", .text = "Bring Forward", .tooltip = "Move forward one step",
             .menu = "Edit>Arrange Selection"},
            [this]() { bringForward(); });
    ch->registerCommand(
            {.id = "edit.send-backward", .text = "Send Backward", .tooltip = "Move backward one step",
             .menu = "Edit>Arrange Selection"},
            [this]() { sendBackward(); });
    ch->registerCommand(
            {.id = "edit.send-to-back", .text = "Send to Back", .tooltip = "Send to back", .shortcut = "Ctrl+Shift+B",
             .menu = "Edit>Arrange Selection"},
            [this]() { sendToBack(); });

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
            {.id = "view.toggle-geometry-snap", .text = "Geometry Snapping", .tooltip = "Toggle geometry snapping",
             .menu = "Edit", .checkable = true, .checked = this->window.canvas()->isGeometrySnapEnabled()},
            [this]() { setGeometrySnapEnabled(!this->window.canvas()->isGeometrySnapEnabled()); });
    ch->registerCommand(
            {.id = "view.toggle-grid-snap", .text = "Grid Snapping", .tooltip = "Toggle grid snapping",
             .menu = "Edit", .checkable = true, .checked = this->window.canvas()->isGridSnapEnabled()},
            [this]() { setGridSnapEnabled(!this->window.canvas()->isGridSnapEnabled()); });
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
            {.id = "view.toggle-touch-drawing", .text = "Touch Drawing", .tooltip = "Toggle finger drawing on touch devices",
             .menu = "Edit", .checkable = true, .checked = this->window.canvas()->isTouchDrawingEnabled()},
            [this]() {
                const bool enabled = !this->window.canvas()->isTouchDrawingEnabled();
                this->window.canvas()->setTouchDrawingEnabled(enabled);
                this->window.commandHost()->setCommandChecked("view.toggle-touch-drawing", enabled);
                this->window.statusBar()->showMessage(
                        enabled ? QStringLiteral("Touch drawing enabled")
                                : QStringLiteral("Touch drawing disabled"),
                        2500);
            });
    ch->addMenuSeparator("Edit");

    // Geometry Constraints submenu
    ch->registerCommand(
            {.id = "constraint.coincident", .text = "Coincident", .tooltip = "Merge vertices", .shortcut = "Ctrl+Alt+C",
             .menu = "Edit>Geometry Constraints"},
            [this]() { applyConstraint(vn::geom::ConstraintKind::Coincident); });
    ch->registerCommand(
            {.id = "constraint.horizontal", .text = "Horizontal", .tooltip = "Force horizontal", .shortcut = "Ctrl+Alt+H",
             .menu = "Edit>Geometry Constraints"},
            [this]() { applyConstraint(vn::geom::ConstraintKind::Horizontal); });
    ch->registerCommand(
            {.id = "constraint.vertical", .text = "Vertical", .tooltip = "Force vertical", .shortcut = "Ctrl+Alt+V",
             .menu = "Edit>Geometry Constraints"},
            [this]() { applyConstraint(vn::geom::ConstraintKind::Vertical); });
    ch->registerCommand(
            {.id = "constraint.fixed-length", .text = "Fixed Length", .tooltip = "Set fixed edge length", .shortcut = "Ctrl+Alt+L",
             .menu = "Edit>Geometry Constraints"},
            [this]() { applyConstraint(vn::geom::ConstraintKind::FixedLength); });
    ch->registerCommand(
            {.id = "constraint.edit-length", .text = "Edit Fixed Length...", .tooltip = "Edit constraint value", .shortcut = "Ctrl+Alt+E",
             .menu = "Edit>Geometry Constraints"},
            [this]() { editFixedLengthConstraint(); });
    ch->registerCommand(
            {.id = "constraint.radius", .text = "Radius", .tooltip = "Set fixed radius", .shortcut = "Ctrl+Alt+R",
             .menu = "Edit>Geometry Constraints"},
            [this]() { applyConstraint(vn::geom::ConstraintKind::Radius); });
    ch->registerCommand(
            {.id = "constraint.parallel", .text = "Parallel", .tooltip = "Force parallel edges", .shortcut = "Ctrl+Alt+P",
             .menu = "Edit>Geometry Constraints"},
            [this]() { applyConstraint(vn::geom::ConstraintKind::Parallel); });
    ch->registerCommand(
            {.id = "constraint.perpendicular", .text = "Perpendicular", .tooltip = "Force perpendicular", .shortcut = "Ctrl+Alt+Shift+P",
             .menu = "Edit>Geometry Constraints"},
            [this]() { applyConstraint(vn::geom::ConstraintKind::Perpendicular); });
    ch->registerCommand(
            {.id = "constraint.delete", .text = "Delete Constraints", .tooltip = "Remove constraints", .shortcut = "Ctrl+Alt+Delete",
             .menu = "Edit>Geometry Constraints"},
            [this]() { deleteConstraints(); });
    ch->addMenuSeparator("Edit");
    ch->registerCommand(
            {.id = "app.settings", .text = "Preferences...", .tooltip = "Open settings", .menu = "Edit"},
            [this]() { showSettingsDialog(); });

}
