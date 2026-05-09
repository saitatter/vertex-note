/*
 * VertexNote
 *
 * Qt app shell bootstrap.
 */

#include "QtAppShell.h"

#include <array>
#include <vector>

#include <QApplication>
#include <QAction>
#include <QFile>
#include <QFileDialog>
#include <QInputDialog>
#include <QLineEdit>
#include <QMessageBox>
#include <QShortcut>
#include <QStatusBar>
#include <QString>
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
    this->window.commandHost()->registerCommand(
            {.id = "app.new",
             .text = "New",
             .tooltip = "Create a new Qt session",
             .shortcut = "Ctrl+N",
             .menu = "File"},
            [this]() { newSession(); });

    this->window.commandHost()->registerCommand(
            {.id = "app.open",
             .text = "Open...",
             .tooltip = "Open a Qt session or a VertexNote document",
             .shortcut = "Ctrl+O",
             .menu = "File"},
            [this]() { openSession(); });

    this->window.commandHost()->registerCommand(
            {.id = "app.save-as",
             .text = "Save As...",
             .tooltip = "Save the current Qt viewport session sidecar",
             .shortcut = "Ctrl+Shift+S",
             .menu = "File"},
            [this]() { saveSessionAs(); });

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
             .tooltip = "Show the Qt updater surface",
             .menu = "Help"},
            [this]() {
                this->updates.showCheckingForUpdates();
                this->updates.showUpToDate("qt-bootstrap");
            });

    this->window.commandHost()->registerCommand(
            {.id = "app.about-qt-shell",
             .text = "About Qt Shell",
             .tooltip = "Show the Qt shell status",
             .menu = "Help"},
            [this]() {
                this->dialogs.showInfo("VertexNote Qt Shell",
                                       "Qt shell bootstrap is active.\n\n"
                                       "Current slice includes neutral UI services, input translation, a Qt painter "
                                       "render seam, real document-backed page previews, and a "
                                       "viewport session flow with pan/zoom.");
            });

    this->window.commandHost()->registerCommand(
            {.id = "view.zoom-in",
             .text = "Zoom In",
             .tooltip = "Zoom in on the Qt canvas",
             .shortcut = "Ctrl+=",
             .menu = "View"},
            [this]() { this->window.canvas()->zoomIn(); });

    this->window.commandHost()->registerCommand(
            {.id = "view.zoom-out",
             .text = "Zoom Out",
             .tooltip = "Zoom out on the Qt canvas",
             .shortcut = "Ctrl+-",
             .menu = "View"},
            [this]() { this->window.canvas()->zoomOut(); });

    this->window.commandHost()->registerCommand(
            {.id = "view.zoom-reset",
             .text = "Reset View",
             .tooltip = "Reset the canvas viewport",
             .shortcut = "Ctrl+0",
             .menu = "View"},
            [this]() { this->window.canvas()->resetViewport(); });

    this->window.commandHost()->registerCommand(
            {.id = "view.fit-page",
             .text = "Fit Page",
             .tooltip = "Fit the page into the Qt canvas",
             .shortcut = "Ctrl+9",
             .menu = "View"},
            [this]() { this->window.canvas()->fitPage(); });

    this->window.commandHost()->registerCommand(
            {.id = "edit.undo-geometry",
             .text = "Undo",
             .tooltip = "Undo the last edit",
             .shortcut = "Ctrl+Z",
             .menu = "Edit",
             .enabled = this->window.canvas()->canUndo()},
            [this]() {
                if (this->window.canvas()->performUndo()) {
                    this->window.statusBar()->showMessage(QStringLiteral("Undid edit"), 3000);
                    updateEditCommandStates();
                }
            });

    this->window.commandHost()->registerCommand(
            {.id = "edit.redo-geometry",
             .text = "Redo",
             .tooltip = "Redo the last edit",
             .shortcut = "Ctrl+Y",
             .menu = "Edit",
             .enabled = this->window.canvas()->canRedo()},
            [this]() {
                if (this->window.canvas()->performRedo()) {
                    this->window.statusBar()->showMessage(QStringLiteral("Redid edit"), 3000);
                    updateEditCommandStates();
                }
            });

    this->window.commandHost()->registerCommand(
            {.id = "edit.insert-vertex",
             .text = "Insert Vertex on Edge",
             .tooltip = "Insert a geometry vertex on the selected Qt edge",
             .shortcut = "Insert",
             .menu = "Edit"},
            [this]() {
                if (this->window.canvas()->insertVertexOnSelectedEdge()) {
                    this->window.statusBar()->showMessage(QStringLiteral("Inserted geometry vertex"), 3000);
                    updateEditCommandStates();
                }
            });

    this->window.commandHost()->registerCommand(
            {.id = "edit.delete-geometry",
             .text = "Delete Selected Geometry",
             .tooltip = "Delete the selected Qt geometry vertex or edge",
             .shortcut = "Delete",
             .menu = "Edit"},
            [this]() {
                if (this->window.canvas()->deleteSelectedGeometry()) {
                    this->window.statusBar()->showMessage(QStringLiteral("Deleted selected geometry"), 3000);
                    updateEditCommandStates();
                }
            });

    this->window.commandHost()->registerCommand(
            {.id = "view.toggle-geometry-snap",
             .text = "Geometry Snap",
             .tooltip = "Toggle geometry snapping in the Qt shell",
             .menu = "View",
             .checkable = true,
             .checked = this->window.canvas()->isGeometrySnapEnabled()},
            [this]() { setGeometrySnapEnabled(!this->window.canvas()->isGeometrySnapEnabled()); });

    this->window.commandHost()->registerCommand(
            {.id = "view.toggle-grid-snap",
             .text = "Grid Snap",
             .tooltip = "Toggle grid snapping in the Qt shell",
             .menu = "View",
             .checkable = true,
             .checked = this->window.canvas()->isGridSnapEnabled()},
            [this]() { setGridSnapEnabled(!this->window.canvas()->isGridSnapEnabled()); });

    // Tool selection commands
    this->window.commandHost()->registerCommand(
            {.id = "tool.hand",
             .text = "Hand Tool",
             .tooltip = "Pan the canvas with click and drag",
             .shortcut = "H",
             .menu = "Tools",
             .checkable = true,
             .checked = this->window.canvas()->activeTool() == QtToolType::Hand},
            [this]() { selectTool(QtToolType::Hand); });

    this->window.commandHost()->registerCommand(
            {.id = "tool.pen",
             .text = "Pen",
             .tooltip = "Draw freehand strokes",
             .shortcut = "P",
             .menu = "Tools",
             .checkable = true,
             .checked = this->window.canvas()->activeTool() == QtToolType::Pen},
            [this]() { selectTool(QtToolType::Pen); });

    this->window.commandHost()->registerCommand(
            {.id = "tool.eraser",
             .text = "Eraser",
             .tooltip = "Draw white-out strokes",
             .shortcut = "E",
             .menu = "Tools",
             .checkable = true,
             .checked = this->window.canvas()->activeTool() == QtToolType::Eraser},
            [this]() { selectTool(QtToolType::Eraser); });

    this->window.commandHost()->registerCommand(
            {.id = "tool.highlighter",
             .text = "Highlighter",
             .tooltip = "Draw semi-transparent highlight strokes",
             .shortcut = "G",
             .menu = "Tools",
             .checkable = true,
             .checked = this->window.canvas()->activeTool() == QtToolType::Highlighter},
            [this]() { selectTool(QtToolType::Highlighter); });

    this->window.commandHost()->registerCommand(
            {.id = "tool.select",
             .text = "Select",
             .tooltip = "Select elements with click or rubber-band rectangle",
             .shortcut = "S",
             .menu = "Tools",
             .checkable = true,
             .checked = this->window.canvas()->activeTool() == QtToolType::SelectRect},
            [this]() { selectTool(QtToolType::SelectRect); });

    this->window.commandHost()->registerCommand(
            {.id = "page.background",
             .text = "Page Background...",
             .tooltip = "Change the page background colour and pattern",
             .menu = "Edit"},
            [this]() { showBackgroundDialog(); });

    this->window.commandHost()->registerCommand(
            {.id = "export.pdf",
             .text = "Export PDF...",
             .tooltip = "Export all pages as a PDF file",
             .shortcut = "Ctrl+Shift+P",
             .menu = "File"},
            [this]() { exportPdf(); });

    this->window.commandHost()->registerCommand(
            {.id = "export.png",
             .text = "Export PNG...",
             .tooltip = "Export the current page as a PNG image",
             .shortcut = "Ctrl+Shift+E",
             .menu = "File"},
            [this]() { exportPng(); });

    this->window.commandHost()->registerCommand(
            {.id = "view.fullscreen",
             .text = "Fullscreen",
             .tooltip = "Toggle fullscreen mode",
             .shortcut = "F11",
             .menu = "View",
             .checkable = true},
            [this]() { toggleFullscreen(); });

    this->window.commandHost()->registerCommand(
            {.id = "view.presentation",
             .text = "Presentation Mode",
             .tooltip = "Toggle presentation mode (fullscreen, no sidebar, fit page)",
             .shortcut = "F5",
             .menu = "View",
             .checkable = true},
            [this]() { togglePresentationMode(); });

    this->window.commandHost()->registerCommand(
            {.id = "file.save",
             .text = "Save Document",
             .tooltip = "Save the document as .xopp",
             .shortcut = "Ctrl+S",
             .menu = "File"},
            [this]() { saveDocument(); });

    this->window.commandHost()->registerCommand(
            {.id = "file.print",
             .text = "Print...",
             .tooltip = "Print the current document",
             .shortcut = "Ctrl+P",
             .menu = "File"},
            [this]() { printDocument(); });

    this->window.commandHost()->registerCommand(
            {.id = "page.add",
             .text = "Add Page",
             .tooltip = "Add a blank page after the current page",
             .menu = "Edit"},
            [this]() { addPage(); });

    this->window.commandHost()->registerCommand(
            {.id = "page.delete",
             .text = "Delete Page",
             .tooltip = "Delete the current page",
             .menu = "Edit"},
            [this]() { deletePage(); });

    this->window.commandHost()->registerCommand(
            {.id = "page.duplicate",
             .text = "Duplicate Page",
             .tooltip = "Duplicate the current page",
             .menu = "Edit"},
            [this]() { duplicatePage(); });

    this->window.commandHost()->registerCommand(
            {.id = "edit.find",
             .text = "Find Text...",
             .tooltip = "Search for text in the document",
             .shortcut = "Ctrl+F",
             .menu = "Edit"},
            [this]() { findText(); });

    this->window.commandHost()->registerCommand(
            {.id = "edit.insert-image",
             .text = "Insert Image...",
             .tooltip = "Insert an image from a file",
             .menu = "Edit"},
            [this]() { insertImage(); });

    this->window.commandHost()->registerCommand(
            {.id = "tool.text",
             .text = "Text",
             .tooltip = "Insert or edit text on the canvas",
             .shortcut = "T",
             .menu = "Tools",
             .checkable = true,
             .checked = this->window.canvas()->activeTool() == QtToolType::Text},
            [this]() { selectTool(QtToolType::Text); });

    this->window.commandHost()->registerCommand(
            {.id = "app.settings",
             .text = "Preferences...",
             .tooltip = "Open the settings dialog",
             .menu = "Edit"},
            [this]() { showSettingsDialog(); });

    // ---- Shape drawing tools ----
    this->window.commandHost()->registerCommand(
            {.id = "tool.draw-line",
             .text = "Line",
             .tooltip = "Draw a straight line (click start, release at end)",
             .shortcut = "L",
             .menu = "Tools",
             .checkable = true,
             .checked = this->window.canvas()->activeTool() == QtToolType::DrawLine},
            [this]() { selectTool(QtToolType::DrawLine); });

    this->window.commandHost()->registerCommand(
            {.id = "tool.draw-rectangle",
             .text = "Rectangle",
             .tooltip = "Draw a rectangle (click corner, release at opposite corner)",
             .shortcut = "R",
             .menu = "Tools",
             .checkable = true,
             .checked = this->window.canvas()->activeTool() == QtToolType::DrawRectangle},
            [this]() { selectTool(QtToolType::DrawRectangle); });

    this->window.commandHost()->registerCommand(
            {.id = "tool.draw-circle",
             .text = "Circle",
             .tooltip = "Draw a circle (click centre, release at edge)",
             .shortcut = "C",
             .menu = "Tools",
             .checkable = true,
             .checked = this->window.canvas()->activeTool() == QtToolType::DrawCircle},
            [this]() { selectTool(QtToolType::DrawCircle); });

    this->window.commandHost()->registerCommand(
            {.id = "tool.draw-arc",
             .text = "Arc",
             .tooltip = "Draw an arc (click center, click start, click end)",
             .menu = "Tools",
             .checkable = true,
             .checked = this->window.canvas()->activeTool() == QtToolType::DrawArc},
            [this]() { selectTool(QtToolType::DrawArc); });

    this->window.commandHost()->registerCommand(
            {.id = "tool.draw-polyline",
             .text = "Polyline",
             .tooltip = "Draw a polyline (click to add points, double-click to finish)",
             .menu = "Tools",
             .checkable = true,
             .checked = this->window.canvas()->activeTool() == QtToolType::DrawPolyline},
            [this]() { selectTool(QtToolType::DrawPolyline); });

    this->window.commandHost()->registerCommand(
            {.id = "tool.draw-construction-line",
             .text = "Construction Line",
             .tooltip = "Draw a construction line (non-printing guide)",
             .menu = "Tools",
             .checkable = true,
             .checked = this->window.canvas()->activeTool() == QtToolType::DrawConstructionLine},
            [this]() { selectTool(QtToolType::DrawConstructionLine); });

    this->window.commandHost()->registerCommand(
            {.id = "tool.draw-construction-circle",
             .text = "Construction Circle",
             .tooltip = "Draw a construction circle (non-printing guide)",
             .menu = "Tools",
             .checkable = true,
             .checked = this->window.canvas()->activeTool() == QtToolType::DrawConstructionCircle},
            [this]() { selectTool(QtToolType::DrawConstructionCircle); });

    // ---- Geometry constraints ----
    this->window.commandHost()->registerCommand(
            {.id = "constraint.coincident",
             .text = "Coincident Constraint",
             .tooltip = "Merge selected vertices to the same point",
             .menu = "Tools"},
            [this]() { applyConstraint(vn::geom::ConstraintKind::Coincident); });

    this->window.commandHost()->registerCommand(
            {.id = "constraint.horizontal",
             .text = "Horizontal Constraint",
             .tooltip = "Force an edge or vertex pair to be horizontal",
             .menu = "Tools"},
            [this]() { applyConstraint(vn::geom::ConstraintKind::Horizontal); });

    this->window.commandHost()->registerCommand(
            {.id = "constraint.vertical",
             .text = "Vertical Constraint",
             .tooltip = "Force an edge or vertex pair to be vertical",
             .menu = "Tools"},
            [this]() { applyConstraint(vn::geom::ConstraintKind::Vertical); });

    this->window.commandHost()->registerCommand(
            {.id = "constraint.fixed-length",
             .text = "Fixed Length Constraint",
             .tooltip = "Set a fixed length on an edge",
             .menu = "Tools"},
            [this]() { applyConstraint(vn::geom::ConstraintKind::FixedLength); });

    this->window.commandHost()->registerCommand(
            {.id = "constraint.radius",
             .text = "Radius Constraint",
             .tooltip = "Set a fixed radius on a circle or arc",
             .menu = "Tools"},
            [this]() { applyConstraint(vn::geom::ConstraintKind::Radius); });

    this->window.commandHost()->registerCommand(
            {.id = "constraint.parallel",
             .text = "Parallel Constraint",
             .tooltip = "Force two line edges to be parallel",
             .menu = "Tools"},
            [this]() { applyConstraint(vn::geom::ConstraintKind::Parallel); });

    this->window.commandHost()->registerCommand(
            {.id = "constraint.perpendicular",
             .text = "Perpendicular Constraint",
             .tooltip = "Force two line edges to be perpendicular",
             .menu = "Tools"},
            [this]() { applyConstraint(vn::geom::ConstraintKind::Perpendicular); });

    this->window.commandHost()->registerCommand(
            {.id = "constraint.delete",
             .text = "Delete Constraints",
             .tooltip = "Remove constraints touching selected geometry",
             .menu = "Tools"},
            [this]() { deleteConstraints(); });

    this->window.commandHost()->registerCommand(
            {.id = "constraint.edit-length",
             .text = "Edit Constraint Value...",
             .tooltip = "Edit the value of a fixed-length or radius constraint",
             .menu = "Tools"},
            [this]() { editFixedLengthConstraint(); });

    // ---- Phase 7: Clipboard & element operations ----
    this->window.commandHost()->registerCommand(
            {.id = "edit.delete",
             .text = "Delete",
             .tooltip = "Delete selected elements",
             .shortcut = "Delete",
             .menu = "Edit"},
            [this]() { deleteSelection(); });

    this->window.commandHost()->registerCommand(
            {.id = "edit.select-all",
             .text = "Select All",
             .tooltip = "Select all elements on the current page",
             .shortcut = "Ctrl+A",
             .menu = "Edit"},
            [this]() { selectAll(); });

    this->window.commandHost()->registerCommand(
            {.id = "edit.copy",
             .text = "Copy",
             .tooltip = "Copy selected elements to clipboard",
             .shortcut = "Ctrl+C",
             .menu = "Edit"},
            [this]() { copySelection(); });

    this->window.commandHost()->registerCommand(
            {.id = "edit.cut",
             .text = "Cut",
             .tooltip = "Cut selected elements to clipboard",
             .shortcut = "Ctrl+X",
             .menu = "Edit"},
            [this]() { cutSelection(); });

    this->window.commandHost()->registerCommand(
            {.id = "edit.paste",
             .text = "Paste",
             .tooltip = "Paste elements from clipboard",
             .shortcut = "Ctrl+V",
             .menu = "Edit"},
            [this]() { pasteClipboard(); });

    // ---- Phase 8: Page navigation ----
    this->window.commandHost()->registerCommand(
            {.id = "nav.first-page",
             .text = "First Page",
             .tooltip = "Go to the first page",
             .shortcut = "Home",
             .menu = "View"},
            [this]() { goToFirstPage(); });

    this->window.commandHost()->registerCommand(
            {.id = "nav.last-page",
             .text = "Last Page",
             .tooltip = "Go to the last page",
             .shortcut = "End",
             .menu = "View"},
            [this]() { goToLastPage(); });

    this->window.commandHost()->registerCommand(
            {.id = "nav.next-page",
             .text = "Next Page",
             .tooltip = "Go to the next page",
             .shortcut = "PgDown",
             .menu = "View"},
            [this]() { goToNextPage(); });

    this->window.commandHost()->registerCommand(
            {.id = "nav.prev-page",
             .text = "Previous Page",
             .tooltip = "Go to the previous page",
             .shortcut = "PgUp",
             .menu = "View"},
            [this]() { goToPreviousPage(); });

    this->window.commandHost()->registerCommand(
            {.id = "nav.goto-page",
             .text = "Go to Page...",
             .tooltip = "Jump to a specific page number",
             .shortcut = "Ctrl+G",
             .menu = "View"},
            [this]() { goToPageDialog(); });

    // ---- Phase 9: Layer operations ----
    this->window.commandHost()->registerCommand(
            {.id = "layer.copy",
             .text = "Copy Layer",
             .tooltip = "Duplicate the current layer",
             .menu = "Edit"},
            [this]() { copyLayer(); });

    this->window.commandHost()->registerCommand(
            {.id = "layer.merge-down",
             .text = "Merge Layer Down",
             .tooltip = "Merge the current layer into the one below",
             .menu = "Edit"},
            [this]() { mergeLayerDown(); });

    this->window.commandHost()->registerCommand(
            {.id = "layer.show-all",
             .text = "Show All Layers",
             .tooltip = "Make all layers visible",
             .menu = "View"},
            [this]() { showAllLayers(); });

    this->window.commandHost()->registerCommand(
            {.id = "layer.hide-all",
             .text = "Hide All Layers",
             .tooltip = "Hide all layers",
             .menu = "View"},
            [this]() { hideAllLayers(); });

    this->window.commandHost()->registerCommand(
            {.id = "layer.rename",
             .text = "Rename Layer...",
             .tooltip = "Rename the current layer",
             .menu = "Edit"},
            [this]() { renameLayerDialog(); });

    // ---- Phase 10: Page operations ----
    this->window.commandHost()->registerCommand(
            {.id = "page.add-before",
             .text = "Add Page Before",
             .tooltip = "Add a blank page before the current page",
             .menu = "Edit"},
            [this]() { addPageBefore(); });

    this->window.commandHost()->registerCommand(
            {.id = "page.move-up",
             .text = "Move Page Up",
             .tooltip = "Move the current page towards the beginning",
             .menu = "Edit"},
            [this]() { movePageUp(); });

    this->window.commandHost()->registerCommand(
            {.id = "page.move-down",
             .text = "Move Page Down",
             .tooltip = "Move the current page towards the end",
             .menu = "Edit"},
            [this]() { movePageDown(); });
}

void QtAppShell::wireWindowState() {
    QObject::connect(this->window.canvas(), &QtCanvas::statusHintChanged, &this->window,
                     [this](const QString& text) { this->window.statusBar()->showMessage(text); });

    QObject::connect(this->window.canvas(), &QtCanvas::viewportStateChanged, &this->window,
                     [this]() { updateWindowTitle(); });

    QObject::connect(this->window.canvas(), &QtCanvas::documentEdited, &this->window,
                     [this]() {
                         if (!this->suppressDirtyTracking) {
                             markSessionDirty();
                         }
                         updateEditCommandStates();
                         this->window.layerPanel()->refresh();
                         this->window.pageSidebar()->refresh();
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

void QtAppShell::newSession() {
    this->session.newDocument();
    this->documentController.newBlankDocument();
    this->suppressDirtyTracking = true;
    this->window.canvas()->newBlankDocument();
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
    this->window.canvas()->fitPage(false);
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
    this->window.commandHost()->setCommandChecked("tool.text", active == QtToolType::Text);
    this->window.commandHost()->setCommandChecked("tool.draw-line", active == QtToolType::DrawLine);
    this->window.commandHost()->setCommandChecked("tool.draw-rectangle", active == QtToolType::DrawRectangle);
    this->window.commandHost()->setCommandChecked("tool.draw-circle", active == QtToolType::DrawCircle);
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
