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

void QtAppShell::registerFileCommands() {
    auto* ch = this->window.commandHost();

    // =====================================================================
    // Menu 1: File
    // =====================================================================
    ch->registerCommand(
            {.id = "app.new", .text = "New", .tooltip = "Create a new document", .shortcut = "Ctrl+N", .menu = "File"},
            [this]() { newSession(); });
    ch->registerCommand(
            {.id = "app.open", .text = "Open...", .tooltip = "Open a document", .shortcut = "Ctrl+O", .menu = "File"},
            [this]() { openSession(); });
    ch->registerCommand(
            {.id = "file.annotate-pdf", .text = "Annotate PDF...", .tooltip = "Open a PDF as an annotation document",
             .menu = "File"},
            [this]() { annotatePdf(); });
    (void) ch->menuForPath("File>Recent Documents");
    ch->addMenuSeparator("File");
    ch->registerCommand(
            {.id = "file.save", .text = "Save", .tooltip = "Save the document", .shortcut = "Ctrl+S", .menu = "File"},
            [this]() { saveDocument(); });
    ch->registerCommand(
            {.id = "app.save-as", .text = "Save As...", .tooltip = "Save to a new file", .shortcut = "Ctrl+Shift+S", .menu = "File"},
            [this]() { saveSessionAs(); });
    ch->addMenuSeparator("File");
    ch->registerCommand(
            {.id = "export.pdf", .text = "Export as PDF...", .tooltip = "Export all pages as PDF", .menu = "File"},
            [this]() { exportPdf(); });
    ch->registerCommand(
            {.id = "export.png", .text = "Export as...", .tooltip = "Export current page as image", .shortcut = "Ctrl+E", .menu = "File"},
            [this]() { exportPng(); });
    ch->addMenuSeparator("File");
    ch->registerCommand(
            {.id = "file.print", .text = "Print...", .tooltip = "Print the document", .shortcut = "Ctrl+P", .menu = "File"},
            [this]() { printDocument(); });
    ch->addMenuSeparator("File");
    ch->registerCommand(
            {.id = "app.quit", .text = "Quit", .tooltip = "Close VertexNote", .shortcut = "Ctrl+Q", .menu = "File"},
            [this]() { requestQuit(); });

}

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

void QtAppShell::registerViewCommands() {
    auto* ch = this->window.commandHost();

    // =====================================================================
    // Menu 3: View
    // =====================================================================
    ch->registerCommand(
            {.id = "view.paired-pages", .text = "Pair Pages", .tooltip = "Display pages side by side",
             .menu = "View", .checkable = true},
            [this]() { togglePairedPages(); });
    ch->registerCommand(
            {.id = "view.presentation", .text = "Presentation Mode", .tooltip = "Fullscreen presentation",
             .shortcut = "F5", .menu = "View", .checkable = true},
            [this]() { togglePresentationMode(); });
    ch->registerCommand(
            {.id = "view.fullscreen", .text = "Fullscreen", .tooltip = "Toggle fullscreen",
             .shortcut = "F11", .menu = "View", .checkable = true},
            [this]() { toggleFullscreen(); });
    ch->addMenuSeparator("View");
    ch->registerCommand(
            {.id = "view.show-toolbar", .text = "Show Toolbars", .tooltip = "Toggle toolbar visibility",
             .shortcut = "F9", .menu = "View", .checkable = true, .checked = true},
            [this]() { toggleToolbarVisibility(); });
    ch->registerCommand(
            {.id = "view.customize-toolbar", .text = "Customize Toolbars...", .tooltip = "Edit the Qt toolbar profile",
             .menu = "View>Toolbars"},
            [this]() { showToolbarCustomizeDialog(); });
    ch->registerCommand(
            {.id = "view.show-menubar", .text = "Show Menubar", .tooltip = "Toggle menubar visibility",
             .shortcut = "F10", .menu = "View", .checkable = true, .checked = true},
            [this]() { toggleMenubarVisibility(); });
    ch->registerCommand(
            {.id = "view.show-sidebar", .text = "Show Sidebar", .tooltip = "Toggle sidebar visibility",
             .shortcut = "F12", .menu = "View", .checkable = true, .checked = true},
            [this]() { toggleSidebarVisibility(); });
    ch->addMenuSeparator("View");

    // Layout submenu
    ch->registerCommand(
            {.id = "view.layout-horizontal", .text = "Horizontal", .tooltip = "Horizontal page layout",
             .menu = "View>Layout", .checkable = true, .checked = true},
            [this]() { setLayoutVertical(false); });
    ch->registerCommand(
            {.id = "view.layout-vertical", .text = "Vertical", .tooltip = "Vertical page layout",
             .menu = "View>Layout", .checkable = true},
            [this]() { setLayoutVertical(true); });
    ch->addMenuSeparator("View>Layout");
    ch->registerCommand(
            {.id = "view.layout-ltr", .text = "Left to Right", .tooltip = "Left to right reading order",
             .menu = "View>Layout", .checkable = true, .checked = true},
            [this]() { setLayoutRtl(false); });
    ch->registerCommand(
            {.id = "view.layout-rtl", .text = "Right to Left", .tooltip = "Right to left reading order",
             .menu = "View>Layout", .checkable = true},
            [this]() { setLayoutRtl(true); });
    ch->addMenuSeparator("View>Layout");
    ch->registerCommand(
            {.id = "view.layout-ttb", .text = "Top to Bottom", .tooltip = "Top to bottom page order",
             .menu = "View>Layout", .checkable = true, .checked = true},
            [this]() { setLayoutBtt(false); });
    ch->registerCommand(
            {.id = "view.layout-btt", .text = "Bottom to Top", .tooltip = "Bottom to top page order",
             .menu = "View>Layout", .checkable = true},
            [this]() { setLayoutBtt(true); });
    ch->addMenuSeparator("View>Layout");
    for (int columns = 1; columns <= 8; ++columns) {
        ch->registerCommand(
                {.id = "view.columns-" + std::to_string(columns),
                 .text = std::to_string(columns) + (columns == 1 ? " Column" : " Columns"),
                 .tooltip = "Use a fixed number of page columns",
                 .menu = "View>Layout>Columns",
                 .checkable = true},
                [this, columns]() { setLayoutColumns(columns); });
    }
    for (int rows = 1; rows <= 8; ++rows) {
        ch->registerCommand(
                {.id = "view.rows-" + std::to_string(rows),
                 .text = std::to_string(rows) + (rows == 1 ? " Row" : " Rows"),
                 .tooltip = "Use a fixed number of page rows",
                 .menu = "View>Layout>Rows",
                 .checkable = true},
                [this, rows]() { setLayoutRows(rows); });
    }
    ch->addMenuSeparator("View>Layout");
    for (int offset = 0; offset <= 1; ++offset) {
        ch->registerCommand(
                {.id = "view.pair-offset-" + std::to_string(offset),
                 .text = "Pair Offset " + std::to_string(offset),
                 .tooltip = "Offset paired pages before grouping",
                 .menu = "View>Layout>Pair Offset",
                 .checkable = true},
                [this, offset]() { setPairOffset(offset); });
    }

    ch->addMenuSeparator("View");
    ch->registerCommand(
            {.id = "view.zoom-in", .text = "Zoom In", .tooltip = "Zoom in", .shortcut = "Ctrl+=", .menu = "View"},
            [this]() { this->window.canvas()->zoomIn(); });
    ch->registerCommand(
            {.id = "view.zoom-out", .text = "Zoom Out", .tooltip = "Zoom out", .shortcut = "Ctrl+-", .menu = "View"},
            [this]() { this->window.canvas()->zoomOut(); });
    ch->registerCommand(
            {.id = "view.zoom-100", .text = "Normal Size", .tooltip = "Zoom to 100%", .shortcut = "Ctrl+1", .menu = "View"},
            [this]() { this->window.canvas()->zoomToActualSize(); this->window.canvas()->update(); });
    ch->registerCommand(
            {.id = "view.fit-page", .text = "Zoom to Fit", .tooltip = "Fit page in window", .shortcut = "Ctrl+9", .menu = "View"},
            [this]() { this->window.canvas()->fitPage(); });
    ch->registerCommand(
            {.id = "view.fit-width", .text = "Fit Width", .tooltip = "Fit page width", .shortcut = "Ctrl+8", .menu = "View"},
            [this]() { this->window.canvas()->fitWidth(); this->window.canvas()->update(); });
    ch->registerCommand(
            {.id = "view.zoom-reset", .text = "Reset View", .tooltip = "Reset viewport", .shortcut = "Ctrl+0", .menu = "View"},
            [this]() { this->window.canvas()->resetViewport(); });

}

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

void QtAppShell::registerJournalCommands() {
    auto* ch = this->window.commandHost();

    // =====================================================================
    // Menu 5: Journal
    // =====================================================================
    ch->registerCommand(
            {.id = "page.add-before", .text = "New Page Before", .tooltip = "Add page before current", .menu = "Journal"},
            [this]() { addPageBefore(); });
    ch->registerCommand(
            {.id = "page.add", .text = "New Page After", .tooltip = "Add page after current", .shortcut = "Ctrl+D", .menu = "Journal"},
            [this]() { addPage(); });
    ch->registerCommand(
            {.id = "page.add-end", .text = "New Page at End", .tooltip = "Add page at end of document", .menu = "Journal"},
            [this]() { addPageAtEnd(); });
    ch->registerCommand(
            {.id = "page.duplicate", .text = "Duplicate Page", .tooltip = "Duplicate the current page", .menu = "Journal"},
            [this]() { duplicatePage(); });
    ch->registerCommand(
            {.id = "page.move-up", .text = "Move Page Up", .tooltip = "Move the current page toward the start of the document",
             .menu = "Journal"},
            [this]() { movePageUp(); });
    ch->registerCommand(
            {.id = "page.move-down", .text = "Move Page Down", .tooltip = "Move the current page toward the end of the document",
             .menu = "Journal"},
            [this]() { movePageDown(); });
    ch->registerCommand(
            {.id = "journal.append-new-pdf-pages", .text = "Append New PDF Pages",
             .tooltip = "Append PDF pages not yet present in the document", .menu = "Journal"},
            [this]() { appendNewPdfPages(); });
    ch->addMenuSeparator("Journal");
    ch->registerCommand(
            {.id = "page.delete", .text = "Delete Page", .tooltip = "Delete the current page", .shortcut = "Ctrl+Shift+Delete", .menu = "Journal"},
            [this]() { deletePage(); });
    ch->addMenuSeparator("Journal");
    ch->registerCommand(
            {.id = "layer.add-above", .text = "Add Layer Above", .tooltip = "Add layer above current", .shortcut = "Ctrl+L", .menu = "Journal"},
            [this]() { addLayerAbove(); });
    ch->registerCommand(
            {.id = "layer.add-below", .text = "Add Layer Below", .tooltip = "Add layer below current", .menu = "Journal"},
            [this]() { addLayerBelow(); });
    ch->registerCommand(
            {.id = "layer.copy", .text = "Copy Layer", .tooltip = "Copy the current layer", .menu = "Journal"},
            [this]() { copyLayer(); });
    ch->registerCommand(
            {.id = "page.delete-layer", .text = "Delete Layer", .tooltip = "Delete the current layer", .shortcut = "Ctrl+Shift+L", .menu = "Journal"},
            [this]() { deleteLayer(); });
    ch->registerCommand(
            {.id = "layer.merge-down", .text = "Merge Layer Down", .tooltip = "Merge into layer below", .shortcut = "Ctrl+M", .menu = "Journal"},
            [this]() { mergeLayerDown(); });
    ch->registerCommand(
            {.id = "layer.rename", .text = "Rename Layer...", .tooltip = "Rename the current layer", .shortcut = "Ctrl+R", .menu = "Journal"},
            [this]() { renameLayerDialog(); });
    ch->registerCommand(
            {.id = "layer.show-all", .text = "Show All Layers", .tooltip = "Show all layers on the current page", .menu = "Journal"},
            [this]() { showAllLayers(); });
    ch->registerCommand(
            {.id = "layer.hide-all", .text = "Hide All Layers", .tooltip = "Hide all layers on the current page", .menu = "Journal"},
            [this]() { hideAllLayers(); });
    ch->addMenuSeparator("Journal");
    ch->registerCommand(
            {.id = "page.format", .text = "Paper Format...", .tooltip = "Set page size and orientation", .menu = "Journal"},
            [this]() { paperFormatDialog(); });
    ch->registerCommand(
            {.id = "page.template", .text = "Configure Page Template...",
             .tooltip = "Set the default page template for new Qt pages", .menu = "Journal"},
            [this]() { configurePageTemplateDialog(); });
    ch->registerCommand(
            {.id = "page.background", .text = "Paper Color...", .tooltip = "Change page background color", .menu = "Journal"},
            [this]() { showBackgroundDialog(); });

}

void QtAppShell::registerToolCommands() {
    auto* ch = this->window.commandHost();

    // =====================================================================
    // Menu 6: Tools
    // =====================================================================
    // Drawing tools
    ch->registerCommand(
            {.id = "tool.pen", .text = "Pen", .tooltip = "Draw freehand strokes", .shortcut = "Ctrl+Shift+P",
             .menu = "Tools", .checkable = true, .checked = this->window.canvas()->activeTool() == QtToolType::Pen},
            [this]() { selectTool(QtToolType::Pen); });
    ch->registerCommand(
            {.id = "tool.eraser", .text = "Eraser", .tooltip = "Erase strokes", .shortcut = "Ctrl+Shift+E",
             .menu = "Tools", .checkable = true, .checked = this->window.canvas()->activeTool() == QtToolType::Eraser},
            [this]() { selectTool(QtToolType::Eraser); });
    ch->registerCommand(
            {.id = "tool.highlighter", .text = "Highlighter", .tooltip = "Draw highlight strokes", .shortcut = "Ctrl+Shift+H",
             .menu = "Tools", .checkable = true, .checked = this->window.canvas()->activeTool() == QtToolType::Highlighter},
            [this]() { selectTool(QtToolType::Highlighter); });
    ch->registerCommand(
            {.id = "tool.laser-pointer-pen", .text = "Laser Pointer Pen", .tooltip = "Draw temporary laser pen strokes",
             .menu = "Tools", .checkable = true},
            [this]() { selectTool(QtToolType::LaserPointerPen); });
    ch->registerCommand(
            {.id = "tool.laser-pointer-highlighter", .text = "Laser Pointer Highlighter",
             .tooltip = "Draw temporary laser highlighter strokes", .menu = "Tools", .checkable = true},
            [this]() { selectTool(QtToolType::LaserPointerHighlighter); });
    ch->registerCommand(
            {.id = "tool.setsquare", .text = "Setsquare", .tooltip = "Draw guided straight strokes with a setsquare",
             .menu = "Tools", .checkable = true},
            [this]() { selectTool(QtToolType::Setsquare); });
    ch->registerCommand(
            {.id = "tool.compass", .text = "Compass", .tooltip = "Draw guided arcs and radius strokes with a compass",
             .menu = "Tools", .checkable = true},
            [this]() { selectTool(QtToolType::Compass); });
    ch->registerCommand(
            {.id = "tool.text", .text = "Text", .tooltip = "Insert or edit text", .shortcut = "Ctrl+Shift+T",
             .menu = "Tools", .checkable = true, .checked = this->window.canvas()->activeTool() == QtToolType::Text},
            [this]() { selectTool(QtToolType::Text); });
    ch->registerCommand(
            {.id = "tool.math-tex", .text = "Math TeX", .tooltip = "Insert a LaTeX formula", .shortcut = "Ctrl+Shift+X",
             .menu = "Tools"},
            [this]() { insertMathTex(); });
    ch->registerCommand(
            {.id = "tool.select-pdf-text-linear", .text = "Select Linear PDF Text",
             .tooltip = "Select PDF text along dragged glyphs", .menu = "Tools", .checkable = true},
            [this]() { selectTool(QtToolType::PdfTextLinear); });
    ch->registerCommand(
            {.id = "tool.select-pdf-text-rect", .text = "Select Area PDF Text",
             .tooltip = "Select PDF text inside a dragged rectangle", .menu = "Tools", .checkable = true},
            [this]() { selectTool(QtToolType::PdfTextRect); });
    ch->registerCommand(
            {.id = "tool.pdf-text-highlight", .text = "Highlight Selected PDF Text",
             .tooltip = "Create highlighter strokes over the active PDF text selection", .menu = "Tools"},
            [this]() { highlightPdfTextSelection(); });
    ch->registerCommand(
            {.id = "tool.select-pdf-text-marker-opacity", .text = "PDF Text Marker Opacity",
             .tooltip = "Set PDF text highlight marker opacity", .menu = "Tools"},
            [this]() {
                bool ok = false;
                const int opacity =
                        QInputDialog::getInt(&this->window, QStringLiteral("PDF Text Marker Opacity"),
                                             QStringLiteral("Opacity:"), this->window.canvas()->toolState().pdfTextMarkerOpacity,
                                             0, 255, 8, &ok);
                if (ok) {
                    setPdfTextMarkerOpacity(opacity);
                }
            });
    ch->registerCommand(
            {.id = "edit.insert-image", .text = "Image", .tooltip = "Insert image from file", .shortcut = "Ctrl+Shift+I", .menu = "Tools"},
            [this]() { insertImage(); });
    ch->registerCommand(
            {.id = "audio.record", .text = "Audio Record", .tooltip = "Start or stop audio recording",
             .menu = "Tools", .checkable = true},
            [this]() { toggleAudioRecording(); });
    ch->registerCommand(
            {.id = "audio.pause-playback", .text = "Audio Play / Pause", .tooltip = "Play, pause, or resume audio",
             .menu = "Tools", .checkable = true},
            [this]() { toggleAudioPausePlayback(); });
    ch->registerCommand(
            {.id = "audio.seek-backwards", .text = "Audio Back", .tooltip = "Seek backwards in the active clip",
             .menu = "Tools"},
            [this]() { seekAudioBackwards(); });
    ch->registerCommand(
            {.id = "audio.seek-forwards", .text = "Audio Forward", .tooltip = "Seek forwards in the active clip",
             .menu = "Tools"},
            [this]() { seekAudioForwards(); });
    ch->registerCommand(
            {.id = "audio.stop-playback", .text = "Audio Stop", .tooltip = "Stop audio playback",
             .menu = "Tools"},
            [this]() { stopAudioPlayback(); });
    ch->registerCommand(
            {.id = "audio.play-object", .text = "Play Object", .tooltip = "Play audio attached to the selected object",
             .menu = "Tools"},
            [this]() { toggleAudioPausePlayback(); });
    ch->addMenuSeparator("Tools");

    // Stroke Drawing submenu
    ch->registerCommand(
            {.id = "tool.draw-rectangle", .text = "Draw Rectangle", .tooltip = "Draw a rectangle", .shortcut = "Ctrl+2",
             .menu = "Tools>Stroke Drawing", .checkable = true, .checked = this->window.canvas()->activeTool() == QtToolType::DrawRectangle},
            [this]() { selectTool(QtToolType::DrawRectangle); });
    ch->registerCommand(
            {.id = "tool.draw-ellipse", .text = "Draw Ellipse", .tooltip = "Draw an ellipse", .shortcut = "Ctrl+3",
             .menu = "Tools>Stroke Drawing", .checkable = true},
            [this]() { selectTool(QtToolType::DrawEllipse); });
    ch->registerCommand(
            {.id = "tool.draw-arrow", .text = "Draw Arrow", .tooltip = "Draw an arrow", .shortcut = "Ctrl+4",
             .menu = "Tools>Stroke Drawing", .checkable = true},
            [this]() { selectTool(QtToolType::DrawArrow); });
    ch->registerCommand(
            {.id = "tool.draw-double-arrow", .text = "Draw Double Arrow", .tooltip = "Draw a double-headed arrow", .shortcut = "Ctrl+5",
             .menu = "Tools>Stroke Drawing", .checkable = true},
            [this]() { selectTool(QtToolType::DrawDoubleArrow); });
    ch->registerCommand(
            {.id = "tool.draw-coordinate-system", .text = "Draw Coordinate System", .tooltip = "Draw X-Y axes", .shortcut = "Ctrl+6",
             .menu = "Tools>Stroke Drawing", .checkable = true},
            [this]() { selectTool(QtToolType::DrawCoordinateSystem); });
    ch->registerCommand(
            {.id = "tool.draw-line", .text = "Draw Line", .tooltip = "Draw a straight line", .shortcut = "Ctrl+7",
             .menu = "Tools>Stroke Drawing", .checkable = true, .checked = this->window.canvas()->activeTool() == QtToolType::DrawLine},
            [this]() { selectTool(QtToolType::DrawLine); });
    ch->registerCommand(
            {.id = "tool.draw-spline", .text = "Draw Spline", .tooltip = "Draw a smooth spline curve", .shortcut = "Ctrl+8",
             .menu = "Tools>Stroke Drawing", .checkable = true},
            [this]() { selectTool(QtToolType::DrawSpline); });
    ch->registerCommand(
            {.id = "tool.draw-shape-recognizer", .text = "Shape Recognizer",
             .tooltip = "Recognize strokes as clean geometric shapes", .menu = "Tools>Stroke Drawing", .checkable = true},
            [this]() { selectTool(QtToolType::ShapeRecognizer); });
    ch->addMenuSeparator("Tools");
    // Vertex Drawing submenu
    ch->registerCommand(
            {.id = "tool.draw-circle", .text = "Draw Vertex Circle", .tooltip = "Draw a geometry circle", .shortcut = "Ctrl+9",
             .menu = "Tools>Vertex Drawing", .checkable = true, .checked = this->window.canvas()->activeTool() == QtToolType::DrawCircle},
            [this]() { selectTool(QtToolType::DrawCircle); });
    ch->registerCommand(
            {.id = "tool.draw-arc", .text = "Draw Vertex Arc", .tooltip = "Draw a geometry arc", .shortcut = "Ctrl+0",
             .menu = "Tools>Vertex Drawing", .checkable = true, .checked = this->window.canvas()->activeTool() == QtToolType::DrawArc},
            [this]() { selectTool(QtToolType::DrawArc); });
    ch->registerCommand(
            {.id = "tool.draw-construction-line", .text = "Draw Construction Line", .tooltip = "Draw a construction guide line", .shortcut = "Ctrl+Shift+0",
             .menu = "Tools>Vertex Drawing", .checkable = true, .checked = this->window.canvas()->activeTool() == QtToolType::DrawConstructionLine},
            [this]() { selectTool(QtToolType::DrawConstructionLine); });
    ch->registerCommand(
            {.id = "tool.draw-construction-circle", .text = "Draw Construction Circle", .tooltip = "Draw a construction guide circle",
             .menu = "Tools>Vertex Drawing", .checkable = true, .checked = this->window.canvas()->activeTool() == QtToolType::DrawConstructionCircle},
            [this]() { selectTool(QtToolType::DrawConstructionCircle); });
    ch->registerCommand(
            {.id = "tool.draw-polyline", .text = "Draw Polyline", .tooltip = "Draw a multi-segment line",
             .menu = "Tools>Vertex Drawing", .checkable = true, .checked = this->window.canvas()->activeTool() == QtToolType::DrawPolyline},
            [this]() { selectTool(QtToolType::DrawPolyline); });
    ch->addMenuSeparator("Tools");

    // Selection tools
    ch->registerCommand(
            {.id = "tool.select", .text = "Select Rectangle", .tooltip = "Rectangle selection", .shortcut = "Ctrl+Shift+R",
             .menu = "Tools", .checkable = true, .checked = this->window.canvas()->activeTool() == QtToolType::SelectRect},
            [this]() { selectTool(QtToolType::SelectRect); });
    ch->registerCommand(
            {.id = "tool.select-region", .text = "Select Region", .tooltip = "Free-form selection", .shortcut = "Ctrl+Shift+G",
             .menu = "Tools", .checkable = true},
            [this]() { selectTool(QtToolType::SelectRegion); });
    ch->registerCommand(
            {.id = "tool.select-multilayer-rect", .text = "Select Multi-Layer Rect", .tooltip = "Rectangle selection across layers",
             .menu = "Tools", .checkable = true},
            [this]() { selectTool(QtToolType::SelectMultiLayerRect); });
    ch->registerCommand(
            {.id = "tool.select-multilayer-region", .text = "Select Multi-Layer Region", .tooltip = "Free-form selection across layers",
             .menu = "Tools", .checkable = true},
            [this]() { selectTool(QtToolType::SelectMultiLayerRegion); });
    ch->registerCommand(
            {.id = "tool.select-object", .text = "Select Object", .tooltip = "Select individual objects", .shortcut = "Ctrl+Shift+O",
             .menu = "Tools", .checkable = true},
            [this]() { selectTool(QtToolType::SelectObject); });
    ch->registerCommand(
            {.id = "tool.vertical-space", .text = "Vertical Space", .tooltip = "Insert vertical space", .shortcut = "Ctrl+Shift+V",
             .menu = "Tools", .checkable = true},
            [this]() { selectTool(QtToolType::VerticalSpace); });
    ch->registerCommand(
            {.id = "tool.hand", .text = "Hand Tool", .tooltip = "Pan the canvas", .shortcut = "Ctrl+Shift+A",
             .menu = "Tools", .checkable = true, .checked = this->window.canvas()->activeTool() == QtToolType::Hand},
            [this]() { selectTool(QtToolType::Hand); });
    ch->registerCommand(
            {.id = "tool.default-preset", .text = "Default Tool", .tooltip = "Restore the default pen preset and select it",
             .menu = "Tools"},
            [this]() {
                auto& ts = this->window.canvas()->toolState();
                ts.activeTool = QtToolType::Pen;
                ts.penWidth = this->currentSettings.defaultPenWidth;
                ts.highlighterWidth = this->currentSettings.defaultHighlighterWidth;
                ts.eraserWidth = this->currentSettings.defaultEraserWidth;
                ts.pressureSensitive = this->currentSettings.defaultPressureSensitive;
                ts.eraserMode = this->currentSettings.defaultEraserMode;
                ts.penLineStyle = "plain";
                ts.fillEnabled = false;
                this->window.canvas()->setActiveTool(QtToolType::Pen);
                this->window.toolPalette()->syncFromToolState(ts);
                syncToolbarWidgets();
                this->window.statusBar()->showMessage(QStringLiteral("Default pen preset restored"), 2500);
            });
    ch->addMenuSeparator("Tools");

    // Pen Options submenu
    ch->registerCommand(
            {.id = "pen.size-very-fine", .text = "Very Fine", .tooltip = "Very fine pen", .menu = "Tools>Pen Options", .checkable = true},
            [this]() { setPenSize(0); });
    ch->registerCommand(
            {.id = "pen.size-fine", .text = "Fine", .tooltip = "Fine pen", .menu = "Tools>Pen Options", .checkable = true, .checked = true},
            [this]() { setPenSize(1); });
    ch->registerCommand(
            {.id = "pen.size-medium", .text = "Medium", .tooltip = "Medium pen", .menu = "Tools>Pen Options", .checkable = true},
            [this]() { setPenSize(2); });
    ch->registerCommand(
            {.id = "pen.size-thick", .text = "Thick", .tooltip = "Thick pen", .menu = "Tools>Pen Options", .checkable = true},
            [this]() { setPenSize(3); });
    ch->registerCommand(
            {.id = "pen.size-very-thick", .text = "Very Thick", .tooltip = "Very thick pen", .menu = "Tools>Pen Options", .checkable = true},
            [this]() { setPenSize(4); });
    ch->addMenuSeparator("Tools>Pen Options");
    ch->registerCommand(
            {.id = "pen.line-solid", .text = "Standard", .tooltip = "Solid line", .menu = "Tools>Pen Options", .checkable = true, .checked = true},
            [this]() { setPenLineStyle("plain"); });
    ch->registerCommand(
            {.id = "pen.line-dash", .text = "Dashed", .tooltip = "Dashed line", .menu = "Tools>Pen Options", .checkable = true},
            [this]() { setPenLineStyle("dash"); });
    ch->registerCommand(
            {.id = "pen.line-dashdot", .text = "Dash-Dotted", .tooltip = "Dash-dot line", .menu = "Tools>Pen Options", .checkable = true},
            [this]() { setPenLineStyle("dashdot"); });
    ch->registerCommand(
            {.id = "pen.line-dot", .text = "Dotted", .tooltip = "Dotted line", .menu = "Tools>Pen Options", .checkable = true},
            [this]() { setPenLineStyle("dot"); });
    ch->addMenuSeparator("Tools>Pen Options");
    ch->registerCommand(
            {.id = "pen.fill-toggle", .text = "Fill", .tooltip = "Toggle fill", .menu = "Tools>Pen Options", .checkable = true},
            [this]() {
                auto& ts = this->window.canvas()->toolState();
                ts.fillEnabled = !ts.fillEnabled;
                this->window.commandHost()->setCommandChecked("pen.fill-toggle", ts.fillEnabled);
            });

    // Eraser Options submenu
    ch->registerCommand(
            {.id = "eraser.size-very-fine", .text = "Very Fine", .tooltip = "Very fine eraser", .menu = "Tools>Eraser Options", .checkable = true},
            [this]() { setEraserSize(0); });
    ch->registerCommand(
            {.id = "eraser.size-fine", .text = "Fine", .tooltip = "Fine eraser", .menu = "Tools>Eraser Options", .checkable = true},
            [this]() { setEraserSize(1); });
    ch->registerCommand(
            {.id = "eraser.size-medium", .text = "Medium", .tooltip = "Medium eraser", .menu = "Tools>Eraser Options", .checkable = true, .checked = true},
            [this]() { setEraserSize(2); });
    ch->registerCommand(
            {.id = "eraser.size-thick", .text = "Thick", .tooltip = "Thick eraser", .menu = "Tools>Eraser Options", .checkable = true},
            [this]() { setEraserSize(3); });
    ch->registerCommand(
            {.id = "eraser.size-very-thick", .text = "Very Thick", .tooltip = "Very thick eraser", .menu = "Tools>Eraser Options", .checkable = true},
            [this]() { setEraserSize(4); });
    ch->addMenuSeparator("Tools>Eraser Options");
    ch->registerCommand(
            {.id = "eraser.type-standard", .text = "Standard", .tooltip = "Standard eraser", .menu = "Tools>Eraser Options", .checkable = true, .checked = true},
            [this]() { setEraserType(QtEraserMode::Standard); });
    ch->registerCommand(
            {.id = "eraser.type-whiteout", .text = "Whiteout", .tooltip = "White-out eraser", .menu = "Tools>Eraser Options", .checkable = true},
            [this]() { setEraserType(QtEraserMode::Whiteout); });
    ch->registerCommand(
            {.id = "eraser.type-delete-stroke", .text = "Delete Strokes", .tooltip = "Delete entire strokes", .menu = "Tools>Eraser Options", .checkable = true},
            [this]() { setEraserType(QtEraserMode::DeleteStroke); });

    // Highlighter Options submenu
    ch->registerCommand(
            {.id = "highlighter.size-very-fine", .text = "Very Fine", .tooltip = "Very fine highlighter", .menu = "Tools>Highlighter Options", .checkable = true},
            [this]() { setHighlighterSize(0); });
    ch->registerCommand(
            {.id = "highlighter.size-fine", .text = "Fine", .tooltip = "Fine highlighter", .menu = "Tools>Highlighter Options", .checkable = true},
            [this]() { setHighlighterSize(1); });
    ch->registerCommand(
            {.id = "highlighter.size-medium", .text = "Medium", .tooltip = "Medium highlighter", .menu = "Tools>Highlighter Options", .checkable = true, .checked = true},
            [this]() { setHighlighterSize(2); });
    ch->registerCommand(
            {.id = "highlighter.size-thick", .text = "Thick", .tooltip = "Thick highlighter", .menu = "Tools>Highlighter Options", .checkable = true},
            [this]() { setHighlighterSize(3); });
    ch->registerCommand(
            {.id = "highlighter.size-very-thick", .text = "Very Thick", .tooltip = "Very thick highlighter", .menu = "Tools>Highlighter Options", .checkable = true},
            [this]() { setHighlighterSize(4); });
    ch->addMenuSeparator("Tools>Highlighter Options");
    ch->registerCommand(
            {.id = "highlighter.fill-toggle", .text = "Fill", .tooltip = "Toggle highlighter fill", .menu = "Tools>Highlighter Options", .checkable = true},
            [this]() {
                auto& ts = this->window.canvas()->toolState();
                ts.highlighterFillEnabled = !ts.highlighterFillEnabled;
                this->window.commandHost()->setCommandChecked("highlighter.fill-toggle", ts.highlighterFillEnabled);
            });
    ch->addMenuSeparator("Tools");

    // Other tool items
    ch->registerCommand(
            {.id = "edit.select-font", .text = "Text Font...", .tooltip = "Select font for text tool", .shortcut = "Ctrl+Shift+F", .menu = "Tools"},
            [this]() { selectFont(); });
    ch->addMenuSeparator("Tools");

    // Geometry editing
    ch->registerCommand(
            {.id = "edit.insert-vertex", .text = "Insert Vertex on Edge", .tooltip = "Insert vertex on selected edge",
             .shortcut = "Insert", .menu = "Tools"},
            [this]() {
                if (this->window.canvas()->insertVertexOnSelectedEdge()) {
                    this->window.statusBar()->showMessage(QStringLiteral("Inserted geometry vertex"), 3000);
                    updateEditCommandStates();
                }
            });
    ch->registerCommand(
            {.id = "edit.delete-geometry", .text = "Delete Selected Geometry", .tooltip = "Delete selected geometry",
             .menu = "Tools"},
            [this]() {
                if (this->window.canvas()->deleteSelectedGeometry()) {
                    this->window.statusBar()->showMessage(QStringLiteral("Deleted selected geometry"), 3000);
                    updateEditCommandStates();
                }
            });

}

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
