/*
 * VertexNote
 *
 * Qt app shell bootstrap.
 */

#include "QtAppShell.h"

#include <array>
#include <cmath>
#include <vector>

#include <QApplication>
#include <QAction>
#include <QFile>
#include <QFileDialog>
#include <QFontDialog>
#include <QInputDialog>
#include <QLineEdit>
#include <QMenuBar>
#include <QMessageBox>
#include <QShortcut>
#include <QStatusBar>
#include <QString>
#include <QStyle>
#include <QToolBar>

#include "QtBackgroundDialog.h"
#include "QtPageSidebar.h"
#include "QtSettingsDialog.h"

namespace {

const std::vector<vn::ui::common::FileDialogFilter> SESSION_FILTERS = {
        {.label = "VertexNote Qt Session", .patterns = {"*.vnsession"}},
        {.label = "VertexNote Documents", .patterns = {"*.xopp", "*.xoj", "*.xopt", "*.pdf"}},
        {.label = "All Files", .patterns = {"*"}},
};

auto isSessionFile(const std::filesystem::path& path) -> bool { return path.extension() == ".vnsession"; }

}  // namespace

QtAppShell::QtAppShell():
        dialogs(&this->window),
        updates(&this->window, this->window.statusBar()),
        plugins(this->window.commandHost(), &this->window) {
    this->session.newDocument();
    this->window.canvas()->setDocumentController(&this->documentController);
    this->window.layerPanel()->setDocumentController(&this->documentController);

    // Wire page sidebar
    auto* sidebar = this->window.pageSidebar();
    sidebar->setDocumentController(&this->documentController);
    sidebar->setContentRenderer(this->window.canvas()->contentRenderer());

    registerBootstrapCommands();
    wireWindowState();
    rebuildToolbar();
    this->window.canvas()->newBlankDocument();
    this->window.canvas()->fitWidth();
    sidebar->refresh();
    updateWindowTitle();
}

auto QtAppShell::commandHost() -> vn::ui::common::ICommandHost* { return this->window.commandHost(); }

auto QtAppShell::canvasHost() -> vn::ui::common::ICanvasHost* { return this->window.canvas(); }

auto QtAppShell::clipboardService() -> vn::ui::common::IClipboardService* { return &this->clipboard; }

auto QtAppShell::dialogService() -> vn::ui::common::IDialogService* { return &this->dialogs; }

auto QtAppShell::recentFilesService() -> vn::ui::common::IRecentFilesService* { return &this->recentFiles; }

auto QtAppShell::updatePresentationService() -> vn::ui::common::IUpdatePresentationService* {
    return &this->updates;
}

auto QtAppShell::pluginUiBridge() -> vn::ui::common::IPluginUiBridge* { return &this->plugins; }

auto QtAppShell::nativeMainWindowHandle() const -> void* {
    return reinterpret_cast<void*>(const_cast<QtMainWindow*>(&this->window));
}

void QtAppShell::showMainWindow() { this->window.show(); }

void QtAppShell::requestQuit() { QApplication::quit(); }

void QtAppShell::setMainWindowTitle(std::string_view title) {
    this->window.setWindowTitle(QString::fromUtf8(title.data(), static_cast<int>(title.size())));
}

void QtAppShell::registerBootstrapCommands() {
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
    // TODO: Recent Documents submenu (dynamic)
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
            {.id = "page.delete-layer", .text = "Delete Layer", .tooltip = "Delete the current layer", .shortcut = "Ctrl+Shift+L", .menu = "Journal"},
            [this]() { deleteLayer(); });
    ch->registerCommand(
            {.id = "layer.merge-down", .text = "Merge Layer Down", .tooltip = "Merge into layer below", .shortcut = "Ctrl+M", .menu = "Journal"},
            [this]() { mergeLayerDown(); });
    ch->registerCommand(
            {.id = "layer.rename", .text = "Rename Layer...", .tooltip = "Rename the current layer", .shortcut = "Ctrl+R", .menu = "Journal"},
            [this]() { renameLayerDialog(); });
    ch->addMenuSeparator("Journal");
    ch->registerCommand(
            {.id = "page.format", .text = "Paper Format...", .tooltip = "Set page size and orientation", .menu = "Journal"},
            [this]() { paperFormatDialog(); });
    ch->registerCommand(
            {.id = "page.background", .text = "Paper Color...", .tooltip = "Change page background color", .menu = "Journal"},
            [this]() { showBackgroundDialog(); });

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
            {.id = "tool.text", .text = "Text", .tooltip = "Insert or edit text", .shortcut = "Ctrl+Shift+T",
             .menu = "Tools", .checkable = true, .checked = this->window.canvas()->activeTool() == QtToolType::Text},
            [this]() { selectTool(QtToolType::Text); });
    ch->registerCommand(
            {.id = "edit.insert-image", .text = "Image", .tooltip = "Insert image from file", .shortcut = "Ctrl+Shift+I", .menu = "Tools"},
            [this]() { insertImage(); });
    ch->addMenuSeparator("Tools");

    // Drawing Type submenu
    ch->registerCommand(
            {.id = "tool.draw-rectangle", .text = "Draw Rectangle", .tooltip = "Draw a rectangle", .shortcut = "Ctrl+2",
             .menu = "Tools>Drawing Type", .checkable = true, .checked = this->window.canvas()->activeTool() == QtToolType::DrawRectangle},
            [this]() { selectTool(QtToolType::DrawRectangle); });
    ch->registerCommand(
            {.id = "tool.draw-ellipse", .text = "Draw Ellipse", .tooltip = "Draw an ellipse", .shortcut = "Ctrl+3",
             .menu = "Tools>Drawing Type", .checkable = true},
            [this]() { selectTool(QtToolType::DrawEllipse); });
    ch->registerCommand(
            {.id = "tool.draw-arrow", .text = "Draw Arrow", .tooltip = "Draw an arrow", .shortcut = "Ctrl+4",
             .menu = "Tools>Drawing Type", .checkable = true},
            [this]() { selectTool(QtToolType::DrawArrow); });
    ch->registerCommand(
            {.id = "tool.draw-double-arrow", .text = "Draw Double Arrow", .tooltip = "Draw a double-headed arrow", .shortcut = "Ctrl+5",
             .menu = "Tools>Drawing Type", .checkable = true},
            [this]() { selectTool(QtToolType::DrawDoubleArrow); });
    ch->registerCommand(
            {.id = "tool.draw-coordinate-system", .text = "Draw Coordinate System", .tooltip = "Draw X-Y axes", .shortcut = "Ctrl+6",
             .menu = "Tools>Drawing Type", .checkable = true},
            [this]() { selectTool(QtToolType::DrawCoordinateSystem); });
    ch->registerCommand(
            {.id = "tool.draw-line", .text = "Draw Line", .tooltip = "Draw a straight line", .shortcut = "Ctrl+7",
             .menu = "Tools>Drawing Type", .checkable = true, .checked = this->window.canvas()->activeTool() == QtToolType::DrawLine},
            [this]() { selectTool(QtToolType::DrawLine); });
    ch->registerCommand(
            {.id = "tool.draw-spline", .text = "Draw Spline", .tooltip = "Draw a smooth spline curve", .shortcut = "Ctrl+8",
             .menu = "Tools>Drawing Type", .checkable = true},
            [this]() { selectTool(QtToolType::DrawSpline); });
    ch->addMenuSeparator("Tools>Drawing Type");
    ch->registerCommand(
            {.id = "tool.draw-circle", .text = "Draw Vertex Circle", .tooltip = "Draw a geometry circle", .shortcut = "Ctrl+9",
             .menu = "Tools>Drawing Type", .checkable = true, .checked = this->window.canvas()->activeTool() == QtToolType::DrawCircle},
            [this]() { selectTool(QtToolType::DrawCircle); });
    ch->registerCommand(
            {.id = "tool.draw-arc", .text = "Draw Vertex Arc", .tooltip = "Draw a geometry arc", .shortcut = "Ctrl+0",
             .menu = "Tools>Drawing Type", .checkable = true, .checked = this->window.canvas()->activeTool() == QtToolType::DrawArc},
            [this]() { selectTool(QtToolType::DrawArc); });
    ch->registerCommand(
            {.id = "tool.draw-construction-line", .text = "Draw Construction Line", .tooltip = "Draw a construction guide line", .shortcut = "Ctrl+Shift+0",
             .menu = "Tools>Drawing Type", .checkable = true, .checked = this->window.canvas()->activeTool() == QtToolType::DrawConstructionLine},
            [this]() { selectTool(QtToolType::DrawConstructionLine); });
    ch->registerCommand(
            {.id = "tool.draw-construction-circle", .text = "Draw Construction Circle", .tooltip = "Draw a construction guide circle",
             .menu = "Tools>Drawing Type", .checkable = true, .checked = this->window.canvas()->activeTool() == QtToolType::DrawConstructionCircle},
            [this]() { selectTool(QtToolType::DrawConstructionCircle); });
    ch->registerCommand(
            {.id = "tool.draw-polyline", .text = "Draw Polyline", .tooltip = "Draw a multi-segment line",
             .menu = "Tools>Drawing Type", .checkable = true, .checked = this->window.canvas()->activeTool() == QtToolType::DrawPolyline},
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

    // =====================================================================
    // Menu 7: Help
    // =====================================================================
    ch->registerCommand(
            {.id = "app.check-updates", .text = "Check for Updates", .tooltip = "Check for new versions", .menu = "Help"},
            [this]() {
                this->updates.showCheckingForUpdates();
                this->updates.showUpToDate("qt-bootstrap");
            });
    ch->registerCommand(
            {.id = "app.about-qt-shell", .text = "About VertexNote", .tooltip = "About this application", .menu = "Help"},
            [this]() {
                this->dialogs.showInfo("About VertexNote",
                                       "VertexNote Qt Shell\n\n"
                                       "A modern note-taking application with geometry, "
                                       "PDF annotation, and handwriting support.");
            });
}

void QtAppShell::wireWindowState() {
    QObject::connect(this->window.canvas(), &QtCanvas::statusHintChanged, &this->window,
                     [this](const QString& text) { this->window.statusBar()->showMessage(text); });

    QObject::connect(this->window.canvas(), &QtCanvas::viewportStateChanged, &this->window,
                     [this]() {
                         updateWindowTitle();
                         updateStatusBarLabels();
                     });

    QObject::connect(this->window.canvas(), &QtCanvas::documentEdited, &this->window,
                     [this]() {
                         if (!this->suppressDirtyTracking) {
                             markSessionDirty();
                         }
                         updateEditCommandStates();
                         this->window.layerPanel()->refresh();
                         this->window.pageSidebar()->refresh();
                         updateStatusBarLabels();
                     });

    QObject::connect(this->window.layerPanel(), &QtLayerPanel::layerChanged, &this->window,
                     [this]() {
                         this->window.canvas()->update();
                         if (!this->suppressDirtyTracking) {
                             markSessionDirty();
                         }
                     });

    // Sidebar page selection → scroll canvas to that page
    QObject::connect(this->window.pageSidebar(), &QtPageSidebar::pageSelected, &this->window,
                     [this](std::size_t pageIndex) {
                         // Scroll canvas so the selected page is visible at the top
                         const auto& pages = this->documentController.snapshotPages();
                         double y = 0.0;
                         constexpr double PAGE_GAP = 20.0;
                         for (std::size_t i = 0; i < pageIndex && i < pages.size(); ++i) {
                             y += pages[i].height + PAGE_GAP;
                         }
                         this->window.canvas()->setViewportState(
                                 this->window.canvas()->sessionViewportState().zoom, 0.0, y);
                         this->window.canvas()->update();
                     });

    // Tool palette → canvas tool state
    QObject::connect(this->window.toolPalette(), &QtToolPalette::colorChanged, &this->window,
                     [this](Color color) {
                         auto& ts = this->window.canvas()->toolState();
                         switch (ts.activeTool) {
                             case QtToolType::Pen:
                                 ts.penColor = color;
                                 break;
                             case QtToolType::Highlighter:
                                 ts.highlighterColor = color;
                                 break;
                             default:
                                 break;
                         }
                     });

    QObject::connect(this->window.toolPalette(), &QtToolPalette::widthChanged, &this->window,
                     [this](double width) {
                         auto& ts = this->window.canvas()->toolState();
                         switch (ts.activeTool) {
                             case QtToolType::Pen:
                                 ts.penWidth = width;
                                 break;
                             case QtToolType::Highlighter:
                                 ts.highlighterWidth = width;
                                 break;
                             case QtToolType::Eraser:
                                 ts.eraserWidth = width;
                                 break;
                             default:
                                 break;
                         }
                     });

    QObject::connect(this->window.toolPalette(), &QtToolPalette::pressureToggled, &this->window,
                     [this](bool enabled) { this->window.canvas()->toolState().pressureSensitive = enabled; });

    QObject::connect(this->window.toolPalette(), &QtToolPalette::eraserModeChanged, &this->window,
                     [this](QtEraserMode mode) { this->window.canvas()->toolState().eraserMode = mode; });

    updateEditCommandStates();
}

void QtAppShell::rebuildToolbar() {
    auto* toolBar = this->window.mainToolBar();
    toolBar->clear();

    // Map command IDs to Qt standard pixmap icons
    const auto setIcon = [&](std::string_view id, QStyle::StandardPixmap sp) {
        if (auto* action = this->window.commandHost()->actionForCommand(id)) {
            action->setIcon(this->window.style()->standardIcon(sp));
        }
    };
    setIcon("app.new", QStyle::SP_FileIcon);
    setIcon("app.open", QStyle::SP_DialogOpenButton);
    setIcon("app.save-as", QStyle::SP_DialogSaveButton);
    setIcon("edit.undo-geometry", QStyle::SP_ArrowBack);
    setIcon("edit.redo-geometry", QStyle::SP_ArrowForward);
    setIcon("view.zoom-in", QStyle::SP_TitleBarMaxButton);
    setIcon("view.zoom-out", QStyle::SP_TitleBarMinButton);
    setIcon("view.zoom-reset", QStyle::SP_BrowserReload);
    setIcon("view.fit-page", QStyle::SP_DesktopIcon);
    setIcon("edit.delete-geometry", QStyle::SP_TrashIcon);
    setIcon("edit.insert-vertex", QStyle::SP_FileDialogDetailedView);

    // Tool icons — use theme names with empty fallback (shows text if missing)
    const auto setThemeIcon = [&](std::string_view id, const QString& themeName) {
        if (auto* action = this->window.commandHost()->actionForCommand(id)) {
            auto icon = QIcon::fromTheme(themeName);
            if (!icon.isNull()) {
                action->setIcon(icon);
            }
        }
    };
    setThemeIcon("tool.hand", QStringLiteral("transform-move"));
    setThemeIcon("tool.pen", QStringLiteral("draw-freehand"));
    setThemeIcon("tool.eraser", QStringLiteral("draw-eraser"));
    setThemeIcon("tool.highlighter", QStringLiteral("draw-highlight"));
    setThemeIcon("tool.select", QStringLiteral("edit-select"));
    setThemeIcon("view.toggle-geometry-snap", QStringLiteral("snap-nodes-cusp"));
    setThemeIcon("view.toggle-grid-snap", QStringLiteral("snap-to-grid"));

    const std::array<std::string_view, 18> commandIds = {"app.new",
                                                          "app.open",
                                                          "app.save-as",
                                                          "edit.undo-geometry",
                                                          "edit.redo-geometry",
                                                          "tool.hand",
                                                          "tool.pen",
                                                          "tool.eraser",
                                                          "tool.highlighter",
                                                          "tool.select",
                                                          "edit.insert-vertex",
                                                          "edit.delete-geometry",
                                                          "view.zoom-in",
                                                          "view.zoom-out",
                                                          "view.zoom-reset",
                                                          "view.fit-page",
                                                          "view.toggle-geometry-snap",
                                                          "view.toggle-grid-snap"};

    for (const auto id: commandIds) {
        if (auto* action = this->window.commandHost()->actionForCommand(id)) {
            toolBar->addAction(action);
            if (id == "edit.delete-geometry" || id == "view.fit-page" || id == "tool.select") {
                toolBar->addSeparator();
            }
        }
    }

    // Tool palette widget after tool buttons
    toolBar->addSeparator();
    toolBar->addWidget(this->window.toolPalette());
}

void QtAppShell::updateWindowTitle() {
    const auto title = std::string("VertexNote - ") + this->documentController.titleText() +
                       (this->session.isDirty() ? " *" : "");
    setMainWindowTitle(title);
}

void QtAppShell::updateStatusBarLabels() {
    const auto pageIdx = this->window.canvas()->currentPageIndex();
    const auto pageCount = this->documentController.pageCount();
    this->window.pageStatusLabel()->setText(
            QStringLiteral("Page %1 of %2").arg(pageIdx + 1).arg(pageCount > 0 ? pageCount : 1));

    if (pageCount > 0) {
        const auto layerIdx = this->documentController.selectedLayerIndex(pageIdx);
        const auto layerCount = this->documentController.layerCount(pageIdx);
        this->window.layerStatusLabel()->setText(
                QStringLiteral("Layer %1 / %2").arg(layerIdx + 1).arg(layerCount));
    }

    const auto zoom = this->window.canvas()->sessionViewportState().zoom;
    this->window.zoomStatusLabel()->setText(QStringLiteral("%1%").arg(zoom * 100.0, 0, 'f', 0));
}

void QtAppShell::newSession() {
    this->session.newDocument();
    this->documentController.newBlankDocument();
    this->suppressDirtyTracking = true;
    this->window.canvas()->newBlankDocument();
    this->window.canvas()->fitWidth();
    this->suppressDirtyTracking = false;
    updateEditCommandStates();
    this->window.layerPanel()->refresh();
    this->window.pageSidebar()->refresh();
    this->window.statusBar()->showMessage(QStringLiteral("Created a blank document"), 3000);
    updateWindowTitle();
}

void QtAppShell::openSession() {
    const auto path = this->dialogs.openDocument(SESSION_FILTERS);
    if (!path) {
        return;
    }

    if (isSessionFile(*path)) {
        const auto sessionState = this->session.openFrom(*path);
        if (!sessionState) {
            this->dialogs.showError("Open Failed", "VertexNote could not parse this Qt session file.");
            return;
        }

        if (sessionState->linkedDocumentPath) {
            std::string error;
            if (!this->documentController.loadFrom(*sessionState->linkedDocumentPath, &error)) {
                this->dialogs.showError("Open Failed", error.empty() ? "VertexNote could not open the linked document."
                                                                     : error);
                return;
            }
        } else {
            this->documentController.newBlankDocument();
        }

        this->suppressDirtyTracking = true;
        this->window.canvas()->setViewportState(sessionState->viewport.zoom, sessionState->viewport.scrollX,
                                                sessionState->viewport.scrollY);
        this->suppressDirtyTracking = false;
        this->recentFiles.addRecentFile(*path);
        updateEditCommandStates();
        this->window.layerPanel()->refresh();
        this->window.pageSidebar()->refresh();
        this->window.statusBar()->showMessage(QString::fromStdString("Opened session " + path->filename().string()), 4000);
        updateWindowTitle();
        return;
    }

    std::string error;
    if (!this->documentController.loadFrom(*path, &error)) {
        this->dialogs.showError("Open Failed", error.empty() ? "VertexNote could not open this document." : error);
        return;
    }

    this->session.newDocument();
    this->suppressDirtyTracking = true;
    this->window.canvas()->fitWidth();
    this->suppressDirtyTracking = false;
    this->recentFiles.addRecentFile(*path);
    updateEditCommandStates();
    this->window.layerPanel()->refresh();
    this->window.pageSidebar()->refresh();
    this->window.statusBar()->showMessage(QString::fromStdString("Opened document " + path->filename().string()), 4000);
    updateWindowTitle();
}

void QtAppShell::saveSessionAs() {
    const auto path = this->dialogs.saveDocument(this->session.currentPath().value_or(std::filesystem::path("session.vnsession")),
                                                 SESSION_FILTERS);
    if (!path) {
        return;
    }

    const QtSessionState sessionState{.viewport = this->window.canvas()->sessionViewportState(),
                                                  .linkedDocumentPath = this->documentController.sourcePath()};
    if (!this->session.saveAs(*path, sessionState)) {
        this->dialogs.showError("Save Failed", "VertexNote could not save the Qt session file.");
        return;
    }

    this->recentFiles.addRecentFile(*path);
    this->window.statusBar()->showMessage(QString::fromStdString("Saved " + path->filename().string()), 4000);
    updateWindowTitle();
}

void QtAppShell::markSessionDirty() {
    if (!this->session.isDirty()) {
        this->session.markDirty(true);
        updateWindowTitle();
    }
}

void QtAppShell::updateEditCommandStates() {
    this->window.commandHost()->setCommandEnabled("edit.undo-geometry", this->window.canvas()->canUndo());
    this->window.commandHost()->setCommandEnabled("edit.redo-geometry", this->window.canvas()->canRedo());
    updateToolCommandStates();
}

void QtAppShell::setGeometrySnapEnabled(bool enabled) {
    this->window.canvas()->setGeometrySnapEnabled(enabled);
    this->window.commandHost()->setCommandChecked("view.toggle-geometry-snap", enabled);
    this->window.statusBar()->showMessage(
            enabled ? QStringLiteral("Geometry snap enabled") : QStringLiteral("Geometry snap disabled"), 2500);
}

void QtAppShell::setGridSnapEnabled(bool enabled) {
    this->window.canvas()->setGridSnapEnabled(enabled);
    this->window.commandHost()->setCommandChecked("view.toggle-grid-snap", enabled);
    this->window.statusBar()->showMessage(
            enabled ? QStringLiteral("Grid snap enabled") : QStringLiteral("Grid snap disabled"), 2500);
}

void QtAppShell::selectTool(QtToolType tool) {
    this->window.canvas()->setActiveTool(tool);
    updateToolCommandStates();
    this->window.toolPalette()->syncFromToolState(this->window.canvas()->toolState());
    this->window.statusBar()->showMessage(
            QString::fromStdString("Tool: " + this->window.canvas()->toolState().activeToolName()), 2500);
}

void QtAppShell::updateToolCommandStates() {
    const auto active = this->window.canvas()->activeTool();
    this->window.commandHost()->setCommandChecked("tool.hand", active == QtToolType::Hand);
    this->window.commandHost()->setCommandChecked("tool.pen", active == QtToolType::Pen);
    this->window.commandHost()->setCommandChecked("tool.eraser", active == QtToolType::Eraser);
    this->window.commandHost()->setCommandChecked("tool.highlighter", active == QtToolType::Highlighter);
    this->window.commandHost()->setCommandChecked("tool.select", active == QtToolType::SelectRect);
    this->window.commandHost()->setCommandChecked("tool.select-region", active == QtToolType::SelectRegion);
    this->window.commandHost()->setCommandChecked("tool.select-multilayer-rect", active == QtToolType::SelectMultiLayerRect);
    this->window.commandHost()->setCommandChecked("tool.select-multilayer-region", active == QtToolType::SelectMultiLayerRegion);
    this->window.commandHost()->setCommandChecked("tool.select-object", active == QtToolType::SelectObject);
    this->window.commandHost()->setCommandChecked("tool.vertical-space", active == QtToolType::VerticalSpace);
    this->window.commandHost()->setCommandChecked("tool.text", active == QtToolType::Text);
    this->window.commandHost()->setCommandChecked("tool.draw-line", active == QtToolType::DrawLine);
    this->window.commandHost()->setCommandChecked("tool.draw-rectangle", active == QtToolType::DrawRectangle);
    this->window.commandHost()->setCommandChecked("tool.draw-circle", active == QtToolType::DrawCircle);
    this->window.commandHost()->setCommandChecked("tool.draw-ellipse", active == QtToolType::DrawEllipse);
    this->window.commandHost()->setCommandChecked("tool.draw-arrow", active == QtToolType::DrawArrow);
    this->window.commandHost()->setCommandChecked("tool.draw-double-arrow", active == QtToolType::DrawDoubleArrow);
    this->window.commandHost()->setCommandChecked("tool.draw-coordinate-system", active == QtToolType::DrawCoordinateSystem);
    this->window.commandHost()->setCommandChecked("tool.draw-spline", active == QtToolType::DrawSpline);
    this->window.commandHost()->setCommandChecked("tool.draw-arc", active == QtToolType::DrawArc);
    this->window.commandHost()->setCommandChecked("tool.draw-polyline", active == QtToolType::DrawPolyline);
    this->window.commandHost()->setCommandChecked("tool.draw-construction-line", active == QtToolType::DrawConstructionLine);
    this->window.commandHost()->setCommandChecked("tool.draw-construction-circle", active == QtToolType::DrawConstructionCircle);
}

void QtAppShell::showBackgroundDialog() {
    // Use page 0 for now (single-page focus)
    const std::size_t pageIndex = 0;
    if (!this->documentController.hasDocument() || pageIndex >= this->documentController.pageCount()) {
        return;
    }

    const auto& pages = this->documentController.snapshotPages();
    if (pageIndex >= pages.size()) {
        return;
    }

    const auto& bg = pages[pageIndex].background;
    QtBackgroundDialog dialog(bg.backgroundColor, bg.backgroundFormat, &this->window);
    if (dialog.exec() != QDialog::Accepted) {
        return;
    }

    this->documentController.setPageBackgroundColor(pageIndex, dialog.selectedColor());
    this->documentController.setPageBackgroundType(pageIndex, dialog.selectedFormat());
    this->window.canvas()->update();
    markSessionDirty();
    this->window.statusBar()->showMessage(QStringLiteral("Page background updated"), 3000);
}

void QtAppShell::exportPdf() {
    if (!this->documentController.hasDocument()) {
        return;
    }

    const QString filePath = QFileDialog::getSaveFileName(&this->window, QStringLiteral("Export PDF"),
                                                          QString(), QStringLiteral("PDF Files (*.pdf)"));
    if (filePath.isEmpty()) {
        return;
    }

    auto* renderer = this->window.canvas()->contentRenderer();
    if (!renderer) {
        return;
    }

    QtDocumentExporter exp(renderer);
    std::string errorMsg;
    const auto& pages = this->documentController.snapshotPages();
    if (exp.exportPdf(filePath.toStdString(), pages, &errorMsg)) {
        this->window.statusBar()->showMessage(QStringLiteral("PDF exported successfully"), 3000);
    } else {
        QMessageBox::warning(&this->window, QStringLiteral("Export Failed"),
                             QString::fromStdString(errorMsg));
    }
}

void QtAppShell::exportPng() {
    if (!this->documentController.hasDocument()) {
        return;
    }

    const QString filePath = QFileDialog::getSaveFileName(&this->window, QStringLiteral("Export PNG"),
                                                          QString(), QStringLiteral("PNG Images (*.png)"));
    if (filePath.isEmpty()) {
        return;
    }

    auto* renderer = this->window.canvas()->contentRenderer();
    if (!renderer) {
        return;
    }

    // Export page 0 (current single-page focus)
    const auto& pages = this->documentController.snapshotPages();
    if (pages.empty()) {
        return;
    }

    QtDocumentExporter exp(renderer);
    std::string errorMsg;
    if (exp.exportPng(filePath.toStdString(), pages[0], 2.0, &errorMsg)) {
        this->window.statusBar()->showMessage(QStringLiteral("PNG exported successfully"), 3000);
    } else {
        QMessageBox::warning(&this->window, QStringLiteral("Export Failed"),
                             QString::fromStdString(errorMsg));
    }
}

void QtAppShell::toggleFullscreen() {
    const bool isFullscreen = this->window.isFullScreen();
    if (isFullscreen) {
        this->window.showNormal();
    } else {
        this->window.showFullScreen();
    }
    this->window.commandHost()->setCommandChecked("view.fullscreen", !isFullscreen);

    // If leaving fullscreen while in presentation mode, exit presentation too
    if (isFullscreen && this->presentationMode) {
        this->presentationMode = false;
        this->window.commandHost()->setCommandChecked("view.presentation", false);
        this->window.mainToolBar()->setVisible(true);
        this->window.pageSidebar()->setVisible(true);
        this->window.layerPanel()->setVisible(true);
    }
}

void QtAppShell::togglePresentationMode() {
    this->presentationMode = !this->presentationMode;
    this->window.commandHost()->setCommandChecked("view.presentation", this->presentationMode);

    if (this->presentationMode) {
        // Enter presentation: fullscreen, hide sidebar + layer panel + toolbar, fit page
        if (!this->window.isFullScreen()) {
            this->window.showFullScreen();
            this->window.commandHost()->setCommandChecked("view.fullscreen", true);
        }
        this->window.mainToolBar()->setVisible(false);
        this->window.pageSidebar()->setVisible(false);
        this->window.layerPanel()->setVisible(false);
        this->window.canvas()->fitPage(false);
        this->window.statusBar()->showMessage(QStringLiteral("Presentation mode — press F5 or Escape to exit"), 4000);
    } else {
        // Exit presentation: restore toolbar + sidebar, leave fullscreen
        this->window.mainToolBar()->setVisible(true);
        this->window.pageSidebar()->setVisible(true);
        this->window.layerPanel()->setVisible(true);
        if (this->window.isFullScreen()) {
            this->window.showNormal();
            this->window.commandHost()->setCommandChecked("view.fullscreen", false);
        }
        this->window.statusBar()->showMessage(QStringLiteral("Exited presentation mode"), 3000);
    }
}

void QtAppShell::saveDocument() {
    if (!this->documentController.hasDocument()) {
        return;
    }

    // If there's an existing source path, save there; otherwise prompt
    auto existingPath = this->documentController.sourcePath();
    std::filesystem::path savePath;
    if (existingPath) {
        savePath = *existingPath;
    } else {
        const QString filePath = QFileDialog::getSaveFileName(&this->window, QStringLiteral("Save Document"),
                                                              QString(), QStringLiteral("VertexNote Files (*.xopp)"));
        if (filePath.isEmpty()) {
            return;
        }
        savePath = filePath.toStdString();
    }

    std::string errorMsg;
    if (this->documentController.saveDocument(savePath, &errorMsg)) {
        this->session.markDirty(false);
        updateWindowTitle();
        this->window.statusBar()->showMessage(QStringLiteral("Document saved"), 3000);
    } else {
        QMessageBox::warning(&this->window, QStringLiteral("Save Failed"), QString::fromStdString(errorMsg));
    }
}

void QtAppShell::printDocument() {
    if (!this->documentController.hasDocument()) {
        return;
    }

    auto* renderer = this->window.canvas()->contentRenderer();
    if (!renderer) {
        return;
    }

    QtDocumentExporter exp(renderer);
    const auto& pages = this->documentController.snapshotPages();
    if (exp.printDocument(pages, &this->window)) {
        this->window.statusBar()->showMessage(QStringLiteral("Document printed"), 3000);
    }
}

void QtAppShell::addPage() {
    if (!this->documentController.hasDocument()) {
        return;
    }

    // Add after the last page
    const std::size_t pageCount = this->documentController.pageCount();
    this->documentController.addPageAfter(pageCount > 0 ? pageCount - 1 : 0);
    this->window.canvas()->update();
    this->window.pageSidebar()->refresh();
    markSessionDirty();
    this->window.statusBar()->showMessage(QStringLiteral("Page added"), 3000);
}

void QtAppShell::deletePage() {
    if (!this->documentController.hasDocument()) {
        return;
    }

    const std::size_t pageCount = this->documentController.pageCount();
    if (pageCount <= 1) {
        this->window.statusBar()->showMessage(QStringLiteral("Cannot delete the only page"), 3000);
        return;
    }

    // Delete the last page (simple policy for now)
    this->documentController.deletePage(pageCount - 1);
    this->window.canvas()->update();
    this->window.pageSidebar()->refresh();
    markSessionDirty();
    this->window.statusBar()->showMessage(QStringLiteral("Page deleted"), 3000);
}

void QtAppShell::duplicatePage() {
    if (!this->documentController.hasDocument()) {
        return;
    }

    const std::size_t pageCount = this->documentController.pageCount();
    if (pageCount == 0) {
        return;
    }

    // Duplicate the last page
    this->documentController.duplicatePage(pageCount - 1);
    this->window.canvas()->update();
    this->window.pageSidebar()->refresh();
    markSessionDirty();
    this->window.statusBar()->showMessage(QStringLiteral("Page duplicated"), 3000);
}

void QtAppShell::findText() {
    bool ok = false;
    const QString searchTerm = QInputDialog::getText(&this->window, QStringLiteral("Find Text"),
                                                     QStringLiteral("Search for:"), QLineEdit::Normal, QString(), &ok);
    if (!ok || searchTerm.isEmpty()) {
        return;
    }

    const auto results = this->documentController.findTextInDocument(searchTerm.toStdString());
    if (results.empty()) {
        QMessageBox::information(&this->window, QStringLiteral("Find Text"),
                                 QStringLiteral("No matches found for \"%1\".").arg(searchTerm));
        return;
    }

    // Show summary and scroll to first result
    const auto& first = results.front();
    QString msg = QStringLiteral("Found %1 match(es). First on page %2.")
                          .arg(results.size())
                          .arg(first.pageIndex + 1);
    this->window.statusBar()->showMessage(msg, 5000);

    // Scroll to the page of the first result
    const auto& pages = this->documentController.snapshotPages();
    double y = 0.0;
    constexpr double PAGE_GAP = 20.0;
    for (std::size_t i = 0; i < first.pageIndex && i < pages.size(); ++i) {
        y += pages[i].height + PAGE_GAP;
    }
    this->window.canvas()->setViewportState(this->window.canvas()->sessionViewportState().zoom, 0.0, y);
    this->window.canvas()->update();
}

void QtAppShell::insertImage() {
    if (!this->documentController.hasDocument()) {
        return;
    }

    const QString filePath =
            QFileDialog::getOpenFileName(&this->window, QStringLiteral("Insert Image"), QString(),
                                         QStringLiteral("Images (*.png *.jpg *.jpeg *.bmp *.gif *.svg)"));
    if (filePath.isEmpty()) {
        return;
    }

    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        QMessageBox::warning(&this->window, QStringLiteral("Insert Image"), QStringLiteral("Could not read the image file."));
        return;
    }

    const QByteArray imageData = file.readAll();
    file.close();

    // Insert on page 0, layer 0 at a default position
    this->documentController.insertImage(0, 100.0, 100.0, std::string(imageData.constData(), imageData.size()),
                                         200.0, 200.0);
    this->window.canvas()->update();
    markSessionDirty();
    this->window.statusBar()->showMessage(QStringLiteral("Image inserted"), 3000);
}

void QtAppShell::showSettingsDialog() {
    QtSettingsDialog dialog(this->currentSettings, &this->window);
    if (dialog.exec() != QDialog::Accepted) {
        return;
    }

    this->currentSettings = dialog.settings();

    // Apply relevant settings immediately
    auto& ts = this->window.canvas()->toolState();
    ts.penWidth = this->currentSettings.defaultPenWidth;
    ts.highlighterWidth = this->currentSettings.defaultHighlighterWidth;
    ts.eraserWidth = this->currentSettings.defaultEraserWidth;
    ts.pressureSensitive = this->currentSettings.defaultPressureSensitive;
    ts.eraserMode = this->currentSettings.defaultEraserMode;

    this->window.canvas()->setGeometrySnapEnabled(this->currentSettings.geometrySnapDefault);
    this->window.canvas()->setGridSnapEnabled(this->currentSettings.gridSnapDefault);
    this->window.commandHost()->setCommandChecked("view.toggle-geometry-snap", this->currentSettings.geometrySnapDefault);
    this->window.commandHost()->setCommandChecked("view.toggle-grid-snap", this->currentSettings.gridSnapDefault);

    this->window.toolPalette()->syncFromToolState(ts);
    this->window.statusBar()->showMessage(QStringLiteral("Settings applied"), 3000);
}

void QtAppShell::applyConstraint(vn::geom::ConstraintKind kind) {
    if (!this->documentController.hasDocument()) {
        return;
    }
    if (this->documentController.applyConstraint(kind)) {
        this->window.canvas()->update();
        markSessionDirty();
        this->window.statusBar()->showMessage(QStringLiteral("Constraint applied"), 3000);
    } else {
        this->window.statusBar()->showMessage(QStringLiteral("Cannot apply constraint — check selection"), 3000);
    }
}

void QtAppShell::deleteConstraints() {
    if (!this->documentController.hasDocument()) {
        return;
    }
    (void)this->documentController.deleteSelectedConstraints();
    this->window.canvas()->update();
    markSessionDirty();
    this->window.statusBar()->showMessage(QStringLiteral("Constraints deleted"), 3000);
}

void QtAppShell::editFixedLengthConstraint() {
    if (!this->documentController.hasDocument()) {
        return;
    }
    const auto existing = this->documentController.selectedFixedLengthConstraint();
    if (!existing) {
        this->window.statusBar()->showMessage(
                QStringLiteral("No fixed-length or radius constraint on selection"), 3000);
        return;
    }

    bool ok = false;
    const double newValue = QInputDialog::getDouble(&this->window, QStringLiteral("Edit Constraint Value"),
                                                    QStringLiteral("Value:"), existing->value, 0.01, 100000.0, 2, &ok);
    if (!ok) {
        return;
    }

    (void)this->documentController.updateFixedLengthConstraint(newValue);
    this->window.canvas()->update();
    markSessionDirty();
    this->window.statusBar()->showMessage(QStringLiteral("Constraint value updated"), 3000);
}

// ---------------------------------------------------------------------------
// Phase 7: Clipboard & element operations
// ---------------------------------------------------------------------------

void QtAppShell::deleteSelection() {
    if (!this->documentController.hasDocument()) {
        return;
    }
    if (this->documentController.deleteSelectedElements()) {
        this->window.canvas()->update();
        markSessionDirty();
        this->window.statusBar()->showMessage(QStringLiteral("Selection deleted"), 3000);
    }
}

void QtAppShell::selectAll() {
    if (!this->documentController.hasDocument()) {
        return;
    }
    const auto pageIndex = this->window.canvas()->currentPageIndex();
    this->documentController.selectAllElements(pageIndex);
    this->window.canvas()->update();
    this->window.statusBar()->showMessage(QStringLiteral("All elements selected"), 3000);
}

void QtAppShell::copySelection() {
    auto clones = this->documentController.copySelectedElements();
    if (clones.empty()) {
        this->window.statusBar()->showMessage(QStringLiteral("Nothing to copy"), 3000);
        return;
    }
    this->elementClipboard = std::move(clones);
    this->window.statusBar()->showMessage(
            QStringLiteral("Copied %1 element(s)").arg(this->elementClipboard.size()), 3000);
}

void QtAppShell::cutSelection() {
    auto clones = this->documentController.cutSelectedElements();
    if (clones.empty()) {
        this->window.statusBar()->showMessage(QStringLiteral("Nothing to cut"), 3000);
        return;
    }
    this->elementClipboard = std::move(clones);
    this->window.canvas()->update();
    markSessionDirty();
    this->window.statusBar()->showMessage(
            QStringLiteral("Cut %1 element(s)").arg(this->elementClipboard.size()), 3000);
}

void QtAppShell::pasteClipboard() {
    if (this->elementClipboard.empty()) {
        this->window.statusBar()->showMessage(QStringLiteral("Clipboard is empty"), 3000);
        return;
    }
    if (!this->documentController.hasDocument()) {
        return;
    }

    // Clone clipboard contents so the clipboard survives for repeated paste
    std::vector<ElementPtr> clones;
    clones.reserve(this->elementClipboard.size());
    for (const auto& elem: this->elementClipboard) {
        clones.push_back(elem->clone());
    }

    const auto pageIndex = this->window.canvas()->currentPageIndex();
    if (this->documentController.pasteElements(pageIndex, std::move(clones))) {
        this->window.canvas()->update();
        markSessionDirty();
        this->window.statusBar()->showMessage(
                QStringLiteral("Pasted %1 element(s)").arg(this->elementClipboard.size()), 3000);
    }
}

// ---------------------------------------------------------------------------
// Phase 8: Page navigation
// ---------------------------------------------------------------------------

void QtAppShell::goToPage(std::size_t pageIndex) {
    if (!this->documentController.hasDocument()) {
        return;
    }
    if (pageIndex >= this->documentController.pageCount()) {
        return;
    }
    recordNavPoint();
    this->window.canvas()->scrollToPage(pageIndex);
    this->window.canvas()->update();
    this->window.statusBar()->showMessage(
            QStringLiteral("Page %1 of %2").arg(pageIndex + 1).arg(this->documentController.pageCount()), 3000);
}

void QtAppShell::goToFirstPage() { goToPage(0); }

void QtAppShell::goToLastPage() {
    if (this->documentController.hasDocument() && this->documentController.pageCount() > 0) {
        goToPage(this->documentController.pageCount() - 1);
    }
}

void QtAppShell::goToNextPage() {
    if (!this->documentController.hasDocument()) {
        return;
    }
    const auto current = this->window.canvas()->currentPageIndex();
    if (current + 1 < this->documentController.pageCount()) {
        goToPage(current + 1);
    }
}

void QtAppShell::goToPreviousPage() {
    if (!this->documentController.hasDocument()) {
        return;
    }
    const auto current = this->window.canvas()->currentPageIndex();
    if (current > 0) {
        goToPage(current - 1);
    }
}

void QtAppShell::goToPageDialog() {
    if (!this->documentController.hasDocument()) {
        return;
    }
    bool ok = false;
    const int pageNum = QInputDialog::getInt(&this->window, QStringLiteral("Go to Page"),
                                             QStringLiteral("Page number (1-%1):").arg(this->documentController.pageCount()),
                                             static_cast<int>(this->window.canvas()->currentPageIndex()) + 1, 1,
                                             static_cast<int>(this->documentController.pageCount()), 1, &ok);
    if (ok) {
        goToPage(static_cast<std::size_t>(pageNum - 1));
    }
}

// ---------------------------------------------------------------------------
// Phase 9: Layer operations
// ---------------------------------------------------------------------------

void QtAppShell::copyLayer() {
    if (!this->documentController.hasDocument()) {
        return;
    }
    const auto pageIndex = this->window.canvas()->currentPageIndex();
    const auto layerIndex = this->documentController.selectedLayerIndex(pageIndex);
    this->documentController.copyLayer(pageIndex, layerIndex);
    this->window.canvas()->update();
    this->window.layerPanel()->refresh();
    markSessionDirty();
    this->window.statusBar()->showMessage(QStringLiteral("Layer copied"), 3000);
}

void QtAppShell::mergeLayerDown() {
    if (!this->documentController.hasDocument()) {
        return;
    }
    const auto pageIndex = this->window.canvas()->currentPageIndex();
    const auto layerIndex = this->documentController.selectedLayerIndex(pageIndex);
    if (layerIndex == 0) {
        this->window.statusBar()->showMessage(QStringLiteral("Cannot merge bottom layer"), 3000);
        return;
    }
    this->documentController.mergeLayerDown(pageIndex, layerIndex);
    this->window.canvas()->update();
    this->window.layerPanel()->refresh();
    markSessionDirty();
    this->window.statusBar()->showMessage(QStringLiteral("Layer merged down"), 3000);
}

void QtAppShell::showAllLayers() {
    if (!this->documentController.hasDocument()) {
        return;
    }
    const auto pageIndex = this->window.canvas()->currentPageIndex();
    this->documentController.showAllLayers(pageIndex);
    this->window.canvas()->update();
    this->window.layerPanel()->refresh();
    this->window.statusBar()->showMessage(QStringLiteral("All layers visible"), 3000);
}

void QtAppShell::hideAllLayers() {
    if (!this->documentController.hasDocument()) {
        return;
    }
    const auto pageIndex = this->window.canvas()->currentPageIndex();
    this->documentController.hideAllLayers(pageIndex);
    this->window.canvas()->update();
    this->window.layerPanel()->refresh();
    this->window.statusBar()->showMessage(QStringLiteral("All layers hidden"), 3000);
}

void QtAppShell::renameLayerDialog() {
    if (!this->documentController.hasDocument()) {
        return;
    }
    const auto pageIndex = this->window.canvas()->currentPageIndex();
    const auto layerIndex = this->documentController.selectedLayerIndex(pageIndex);
    const auto infos = this->documentController.layerInfos(pageIndex);
    if (layerIndex >= infos.size()) {
        return;
    }

    bool ok = false;
    const QString newName = QInputDialog::getText(
            &this->window, QStringLiteral("Rename Layer"), QStringLiteral("Layer name:"),
            QLineEdit::Normal, QString::fromStdString(infos[layerIndex].name), &ok);
    if (ok && !newName.isEmpty()) {
        this->documentController.renameLayer(pageIndex, layerIndex, newName.toStdString());
        this->window.layerPanel()->refresh();
        markSessionDirty();
        this->window.statusBar()->showMessage(QStringLiteral("Layer renamed"), 3000);
    }
}

// ---------------------------------------------------------------------------
// Phase 10: Page operations
// ---------------------------------------------------------------------------

void QtAppShell::addPageBefore() {
    if (!this->documentController.hasDocument()) {
        return;
    }
    const auto pageIndex = this->window.canvas()->currentPageIndex();
    this->documentController.addPageBefore(pageIndex);
    this->window.canvas()->update();
    this->window.pageSidebar()->refresh();
    markSessionDirty();
    this->window.statusBar()->showMessage(QStringLiteral("Page added before"), 3000);
}

void QtAppShell::movePageUp() {
    if (!this->documentController.hasDocument()) {
        return;
    }
    const auto pageIndex = this->window.canvas()->currentPageIndex();
    if (pageIndex == 0) {
        this->window.statusBar()->showMessage(QStringLiteral("Already at the first page"), 3000);
        return;
    }
    this->documentController.movePageTowards(pageIndex, -1);
    this->window.canvas()->scrollToPage(pageIndex - 1);
    this->window.canvas()->update();
    this->window.pageSidebar()->refresh();
    markSessionDirty();
    this->window.statusBar()->showMessage(QStringLiteral("Page moved up"), 3000);
}

void QtAppShell::movePageDown() {
    if (!this->documentController.hasDocument()) {
        return;
    }
    const auto pageIndex = this->window.canvas()->currentPageIndex();
    if (pageIndex + 1 >= this->documentController.pageCount()) {
        this->window.statusBar()->showMessage(QStringLiteral("Already at the last page"), 3000);
        return;
    }
    this->documentController.movePageTowards(pageIndex, 1);
    this->window.canvas()->scrollToPage(pageIndex + 1);
    this->window.canvas()->update();
    this->window.pageSidebar()->refresh();
    markSessionDirty();
    this->window.statusBar()->showMessage(QStringLiteral("Page moved down"), 3000);
}

// ---------------------------------------------------------------------------
// Phase 11: Z-order operations
// ---------------------------------------------------------------------------

void QtAppShell::bringToFront() {
    if (this->documentController.bringSelectionToFront()) {
        this->window.canvas()->update();
        markSessionDirty();
        this->window.statusBar()->showMessage(QStringLiteral("Brought to front"), 3000);
    }
}

void QtAppShell::sendToBack() {
    if (this->documentController.sendSelectionToBack()) {
        this->window.canvas()->update();
        markSessionDirty();
        this->window.statusBar()->showMessage(QStringLiteral("Sent to back"), 3000);
    }
}

void QtAppShell::bringForward() {
    if (this->documentController.bringSelectionForward()) {
        this->window.canvas()->update();
        markSessionDirty();
        this->window.statusBar()->showMessage(QStringLiteral("Brought forward"), 3000);
    }
}

void QtAppShell::sendBackward() {
    if (this->documentController.sendSelectionBackward()) {
        this->window.canvas()->update();
        markSessionDirty();
        this->window.statusBar()->showMessage(QStringLiteral("Sent backward"), 3000);
    }
}

// ---------------------------------------------------------------------------
// Phase 12: Pen styling & font
// ---------------------------------------------------------------------------

void QtAppShell::setPenLineStyle(const std::string& style) {
    auto& ts = this->window.canvas()->toolState();
    ts.penLineStyle = style;
    this->window.commandHost()->setCommandChecked("pen.line-solid", style == "plain");
    this->window.commandHost()->setCommandChecked("pen.line-dash", style == "dash");
    this->window.commandHost()->setCommandChecked("pen.line-dashdot", style == "dashdot");
    this->window.commandHost()->setCommandChecked("pen.line-dot", style == "dot");
    this->window.statusBar()->showMessage(
            QStringLiteral("Line style: %1").arg(QString::fromStdString(style)), 3000);
}

void QtAppShell::setStrokeFill(int fillOpacity) {
    auto& ts = this->window.canvas()->toolState();
    ts.fillOpacity = fillOpacity;
    ts.fillEnabled = fillOpacity > 0;
}

void QtAppShell::selectFont() {
    auto& ts = this->window.canvas()->toolState();

    bool ok = false;
    QFont current;
    current.setFamily(QString::fromStdString(ts.fontName));
    current.setPointSizeF(ts.fontSize);

    QFont selected = QFontDialog::getFont(&ok, current, &this->window, QStringLiteral("Select Font"));
    if (ok) {
        ts.fontName = selected.family().toStdString();
        ts.fontSize = selected.pointSizeF();
        this->window.statusBar()->showMessage(
                QStringLiteral("Font: %1 %2pt").arg(selected.family()).arg(selected.pointSizeF()), 3000);
    }
}

// ---------------------------------------------------------------------------
// Phase 13: Navigation history
// ---------------------------------------------------------------------------

void QtAppShell::recordNavPoint() {
    auto* canvas = this->window.canvas();
    const auto state = canvas->sessionViewportState();
    NavPoint point{.pageIndex = canvas->currentPageIndex(),
                   .scrollX = state.scrollX,
                   .scrollY = state.scrollY,
                   .zoom = state.zoom};

    // Trim forward history when recording a new point
    if (this->navHistoryIndex < this->navHistory.size()) {
        this->navHistory.resize(this->navHistoryIndex);
    }

    // Don't record duplicate consecutive positions
    if (!this->navHistory.empty()) {
        const auto& last = this->navHistory.back();
        if (last.pageIndex == point.pageIndex && std::abs(last.scrollX - point.scrollX) < 1.0 &&
            std::abs(last.scrollY - point.scrollY) < 1.0) {
            return;
        }
    }

    this->navHistory.push_back(point);
    this->navHistoryIndex = this->navHistory.size();

    // Limit history size
    if (this->navHistory.size() > 100) {
        this->navHistory.erase(this->navHistory.begin());
        this->navHistoryIndex--;
    }
}

void QtAppShell::navigateBack() {
    if (this->navHistoryIndex == 0 || this->navHistory.empty()) {
        this->window.statusBar()->showMessage(QStringLiteral("No previous position"), 3000);
        return;
    }

    // Save current position if we're at the end
    if (this->navHistoryIndex == this->navHistory.size()) {
        recordNavPoint();
        this->navHistoryIndex--;  // Step back past the just-recorded point
    }

    this->navHistoryIndex--;
    const auto& point = this->navHistory[this->navHistoryIndex];
    this->window.canvas()->setViewportState(point.zoom, point.scrollX, point.scrollY);
    this->window.canvas()->update();
}

void QtAppShell::navigateForward() {
    if (this->navHistoryIndex + 1 >= this->navHistory.size()) {
        this->window.statusBar()->showMessage(QStringLiteral("No next position"), 3000);
        return;
    }

    this->navHistoryIndex++;
    const auto& point = this->navHistory[this->navHistoryIndex];
    this->window.canvas()->setViewportState(point.zoom, point.scrollX, point.scrollY);
    this->window.canvas()->update();
}

// ---------------------------------------------------------------------------
// Phase 13: Layer navigation
// ---------------------------------------------------------------------------

void QtAppShell::gotoNextLayer() {
    auto pageIdx = this->window.canvas()->currentPageIndex();
    auto current = this->documentController.selectedLayerIndex(pageIdx);
    auto count = this->documentController.layerCount(pageIdx);
    if (current + 1 < count) {
        this->documentController.selectLayer(pageIdx, current + 1);
        this->window.canvas()->update();
        this->window.statusBar()->showMessage(
                QStringLiteral("Layer %1 / %2").arg(current + 2).arg(count), 3000);
    }
}

void QtAppShell::gotoPrevLayer() {
    auto pageIdx = this->window.canvas()->currentPageIndex();
    auto current = this->documentController.selectedLayerIndex(pageIdx);
    if (current > 0) {
        this->documentController.selectLayer(pageIdx, current - 1);
        this->window.canvas()->update();
        auto count = this->documentController.layerCount(pageIdx);
        this->window.statusBar()->showMessage(
                QStringLiteral("Layer %1 / %2").arg(current).arg(count), 3000);
    }
}

void QtAppShell::gotoTopLayer() {
    auto pageIdx = this->window.canvas()->currentPageIndex();
    auto count = this->documentController.layerCount(pageIdx);
    if (count > 0) {
        this->documentController.selectLayer(pageIdx, count - 1);
        this->window.canvas()->update();
        this->window.statusBar()->showMessage(
                QStringLiteral("Top layer (%1 / %2)").arg(count).arg(count), 3000);
    }
}

void QtAppShell::addLayerAbove() {
    auto pageIdx = this->window.canvas()->currentPageIndex();
    this->documentController.addLayer(pageIdx);
    auto count = this->documentController.layerCount(pageIdx);
    // Select the newly added layer (top)
    this->documentController.selectLayer(pageIdx, count - 1);
    this->window.canvas()->update();
    markSessionDirty();
    this->window.statusBar()->showMessage(QStringLiteral("Added layer above"), 3000);
}

void QtAppShell::addLayerBelow() {
    auto pageIdx = this->window.canvas()->currentPageIndex();
    auto current = this->documentController.selectedLayerIndex(pageIdx);
    this->documentController.addLayer(pageIdx);
    // addLayer adds at top; keep selection on the same layer (shifted up by one)
    this->documentController.selectLayer(pageIdx, current);
    this->window.canvas()->update();
    markSessionDirty();
    this->window.statusBar()->showMessage(QStringLiteral("Added layer below"), 3000);
}

// ---------------------------------------------------------------------------
// Phase 14: Annotated page navigation
// ---------------------------------------------------------------------------

void QtAppShell::gotoNextAnnotatedPage() {
    auto current = this->window.canvas()->currentPageIndex();
    auto count = this->documentController.pageCount();
    for (std::size_t i = current + 1; i < count; ++i) {
        if (this->documentController.isPageAnnotated(i)) {
            recordNavPoint();
            this->window.canvas()->scrollToPage(i);
            this->window.canvas()->update();
            this->window.statusBar()->showMessage(
                    QStringLiteral("Annotated page %1").arg(i + 1), 3000);
            return;
        }
    }
    this->window.statusBar()->showMessage(QStringLiteral("No more annotated pages"), 3000);
}

void QtAppShell::gotoPrevAnnotatedPage() {
    auto current = this->window.canvas()->currentPageIndex();
    if (current == 0) {
        this->window.statusBar()->showMessage(QStringLiteral("No previous annotated pages"), 3000);
        return;
    }
    for (std::size_t i = current; i > 0; --i) {
        if (this->documentController.isPageAnnotated(i - 1)) {
            recordNavPoint();
            this->window.canvas()->scrollToPage(i - 1);
            this->window.canvas()->update();
            this->window.statusBar()->showMessage(
                    QStringLiteral("Annotated page %1").arg(i), 3000);
            return;
        }
    }
    this->window.statusBar()->showMessage(QStringLiteral("No previous annotated pages"), 3000);
}

// ---------------------------------------------------------------------------
// Pen/eraser/highlighter size and type
// ---------------------------------------------------------------------------

namespace {
constexpr std::array<double, 5> PEN_SIZES = {0.40, 0.85, 1.41, 3.54, 5.00};
constexpr std::array<double, 5> ERASER_SIZES = {3.00, 5.00, 8.50, 14.00, 20.00};
constexpr std::array<double, 5> HIGHLIGHTER_SIZES = {3.00, 5.00, 8.50, 14.00, 20.00};

const std::array<const char*, 5> PEN_SIZE_IDS = {
        "pen.size-very-fine", "pen.size-fine", "pen.size-medium", "pen.size-thick", "pen.size-very-thick"};
const std::array<const char*, 5> ERASER_SIZE_IDS = {
        "eraser.size-very-fine", "eraser.size-fine", "eraser.size-medium", "eraser.size-thick", "eraser.size-very-thick"};
const std::array<const char*, 5> HIGHLIGHTER_SIZE_IDS = {
        "highlighter.size-very-fine", "highlighter.size-fine", "highlighter.size-medium", "highlighter.size-thick", "highlighter.size-very-thick"};
}  // namespace

void QtAppShell::setPenSize(int sizeIndex) {
    if (sizeIndex < 0 || sizeIndex >= 5) return;
    this->window.canvas()->toolState().penWidth = PEN_SIZES[static_cast<std::size_t>(sizeIndex)];
    for (int i = 0; i < 5; ++i) {
        this->window.commandHost()->setCommandChecked(PEN_SIZE_IDS[static_cast<std::size_t>(i)], i == sizeIndex);
    }
    this->window.toolPalette()->syncFromToolState(this->window.canvas()->toolState());
}

void QtAppShell::setEraserSize(int sizeIndex) {
    if (sizeIndex < 0 || sizeIndex >= 5) return;
    this->window.canvas()->toolState().eraserWidth = ERASER_SIZES[static_cast<std::size_t>(sizeIndex)];
    for (int i = 0; i < 5; ++i) {
        this->window.commandHost()->setCommandChecked(ERASER_SIZE_IDS[static_cast<std::size_t>(i)], i == sizeIndex);
    }
    this->window.toolPalette()->syncFromToolState(this->window.canvas()->toolState());
}

void QtAppShell::setEraserType(QtEraserMode mode) {
    this->window.canvas()->toolState().eraserMode = mode;
    this->window.commandHost()->setCommandChecked("eraser.type-standard", mode == QtEraserMode::Standard);
    this->window.commandHost()->setCommandChecked("eraser.type-whiteout", mode == QtEraserMode::Whiteout);
    this->window.commandHost()->setCommandChecked("eraser.type-delete-stroke", mode == QtEraserMode::DeleteStroke);
}

void QtAppShell::setHighlighterSize(int sizeIndex) {
    if (sizeIndex < 0 || sizeIndex >= 5) return;
    this->window.canvas()->toolState().highlighterWidth = HIGHLIGHTER_SIZES[static_cast<std::size_t>(sizeIndex)];
    for (int i = 0; i < 5; ++i) {
        this->window.commandHost()->setCommandChecked(HIGHLIGHTER_SIZE_IDS[static_cast<std::size_t>(i)], i == sizeIndex);
    }
    this->window.toolPalette()->syncFromToolState(this->window.canvas()->toolState());
}

// ---------------------------------------------------------------------------
// View & UI toggles
// ---------------------------------------------------------------------------

void QtAppShell::togglePairedPages() {
    // TODO: implement paired pages layout
    this->window.statusBar()->showMessage(QStringLiteral("Paired pages not yet implemented"), 3000);
}

void QtAppShell::toggleToolbarVisibility() {
    auto* toolbar = this->window.mainToolBar();
    const bool visible = !toolbar->isVisible();
    toolbar->setVisible(visible);
    this->window.commandHost()->setCommandChecked("view.show-toolbar", visible);
}

void QtAppShell::toggleMenubarVisibility() {
    auto* menubar = this->window.menuBar();
    const bool visible = !menubar->isVisible();
    menubar->setVisible(visible);
    this->window.commandHost()->setCommandChecked("view.show-menubar", visible);
}

void QtAppShell::toggleSidebarVisibility() {
    auto* sidebar = this->window.pageSidebar();
    auto* layers = this->window.layerPanel();
    const bool visible = !sidebar->isVisible();
    sidebar->setVisible(visible);
    layers->setVisible(visible);
    this->window.commandHost()->setCommandChecked("view.show-sidebar", visible);
}

void QtAppShell::setLayoutVertical(bool vertical) {
    // TODO: implement layout direction switching in canvas
    this->window.commandHost()->setCommandChecked("view.layout-horizontal", !vertical);
    this->window.commandHost()->setCommandChecked("view.layout-vertical", vertical);
    this->window.statusBar()->showMessage(
            vertical ? QStringLiteral("Vertical layout") : QStringLiteral("Horizontal layout"), 3000);
}

void QtAppShell::setLayoutRtl(bool rtl) {
    this->window.commandHost()->setCommandChecked("view.layout-ltr", !rtl);
    this->window.commandHost()->setCommandChecked("view.layout-rtl", rtl);
    this->window.statusBar()->showMessage(
            rtl ? QStringLiteral("Right to left") : QStringLiteral("Left to right"), 3000);
}

void QtAppShell::setLayoutBtt(bool btt) {
    this->window.commandHost()->setCommandChecked("view.layout-ttb", !btt);
    this->window.commandHost()->setCommandChecked("view.layout-btt", btt);
    this->window.statusBar()->showMessage(
            btt ? QStringLiteral("Bottom to top") : QStringLiteral("Top to bottom"), 3000);
}

// ---------------------------------------------------------------------------
// Journal extras
// ---------------------------------------------------------------------------

void QtAppShell::addPageAtEnd() {
    if (!this->documentController.hasDocument()) return;
    const auto pageCount = this->documentController.pageCount();
    this->documentController.addPageAfter(pageCount > 0 ? pageCount - 1 : 0);
    this->window.canvas()->update();
    this->window.pageSidebar()->refresh();
    markSessionDirty();
    this->window.statusBar()->showMessage(QStringLiteral("Page added at end"), 3000);
}

void QtAppShell::deleteLayer() {
    if (!this->documentController.hasDocument()) return;
    const auto pageIndex = this->window.canvas()->currentPageIndex();
    const auto layerCount = this->documentController.layerCount(pageIndex);
    if (layerCount <= 1) {
        this->window.statusBar()->showMessage(QStringLiteral("Cannot delete the only layer"), 3000);
        return;
    }
    const auto layerIndex = this->documentController.selectedLayerIndex(pageIndex);
    this->documentController.removeLayer(pageIndex, layerIndex);
    this->window.canvas()->update();
    this->window.layerPanel()->refresh();
    markSessionDirty();
    this->window.statusBar()->showMessage(QStringLiteral("Layer deleted"), 3000);
}

void QtAppShell::paperFormatDialog() {
    // TODO: implement full paper format dialog (page size, orientation)
    this->window.statusBar()->showMessage(QStringLiteral("Paper format dialog not yet implemented"), 3000);
}

// ---------------------------------------------------------------------------
// Edit extras
// ---------------------------------------------------------------------------

void QtAppShell::moveSelectionLayerUp() {
    // TODO: implement move-selection-layer-up via document controller
    this->window.statusBar()->showMessage(QStringLiteral("Move selection layer up not yet implemented"), 3000);
}

void QtAppShell::moveSelectionLayerDown() {
    // TODO: implement move-selection-layer-down via document controller
    this->window.statusBar()->showMessage(QStringLiteral("Move selection layer down not yet implemented"), 3000);
}
