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
#include <QFileDialog>
#include <QMessageBox>
#include <QStatusBar>
#include <QString>
#include <QToolBar>

#include "QtBackgroundDialog.h"
#include "QtPageSidebar.h"

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
        plugins(this->window.commandHost()) {
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
