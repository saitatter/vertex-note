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
#include <QStatusBar>
#include <QString>
#include <QToolBar>

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
    registerBootstrapCommands();
    wireWindowState();
    rebuildToolbar();
    this->window.canvas()->newBlankDocument();
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
             .text = "Undo Geometry Edit",
             .tooltip = "Undo the last Qt geometry edit",
             .shortcut = "Ctrl+Z",
             .menu = "Edit",
             .enabled = this->window.canvas()->canUndoGeometryEdit()},
            [this]() {
                if (this->window.canvas()->undoGeometryEdit()) {
                    this->window.statusBar()->showMessage(QStringLiteral("Undid geometry edit"), 3000);
                    updateEditCommandStates();
                }
            });

    this->window.commandHost()->registerCommand(
            {.id = "edit.redo-geometry",
             .text = "Redo Geometry Edit",
             .tooltip = "Redo the last Qt geometry edit",
             .shortcut = "Ctrl+Y",
             .menu = "Edit",
             .enabled = this->window.canvas()->canRedoGeometryEdit()},
            [this]() {
                if (this->window.canvas()->redoGeometryEdit()) {
                    this->window.statusBar()->showMessage(QStringLiteral("Redid geometry edit"), 3000);
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
                     });

    updateEditCommandStates();
}

void QtAppShell::rebuildToolbar() {
    auto* toolBar = this->window.mainToolBar();
    toolBar->clear();

    const std::array<std::string_view, 13> commandIds = {"app.new",
                                                          "app.open",
                                                          "app.save-as",
                                                          "edit.undo-geometry",
                                                          "edit.redo-geometry",
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
            if (id == "edit.delete-geometry" || id == "view.fit-page") {
                toolBar->addSeparator();
            }
        }
    }
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
    this->window.commandHost()->setCommandEnabled("edit.undo-geometry", this->window.canvas()->canUndoGeometryEdit());
    this->window.commandHost()->setCommandEnabled("edit.redo-geometry", this->window.canvas()->canRedoGeometryEdit());
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
