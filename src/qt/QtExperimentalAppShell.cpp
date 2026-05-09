/*
 * VertexNote
 *
 * Experimental Qt app shell bootstrap.
 */

#include "QtExperimentalAppShell.h"

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

QtExperimentalAppShell::QtExperimentalAppShell():
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

auto QtExperimentalAppShell::commandHost() -> vn::ui::common::ICommandHost* { return this->window.commandHost(); }

auto QtExperimentalAppShell::canvasHost() -> vn::ui::common::ICanvasHost* { return this->window.canvas(); }

auto QtExperimentalAppShell::clipboardService() -> vn::ui::common::IClipboardService* { return &this->clipboard; }

auto QtExperimentalAppShell::dialogService() -> vn::ui::common::IDialogService* { return &this->dialogs; }

auto QtExperimentalAppShell::recentFilesService() -> vn::ui::common::IRecentFilesService* { return &this->recentFiles; }

auto QtExperimentalAppShell::updatePresentationService() -> vn::ui::common::IUpdatePresentationService* {
    return &this->updates;
}

auto QtExperimentalAppShell::pluginUiBridge() -> vn::ui::common::IPluginUiBridge* { return &this->plugins; }

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
            {.id = "app.new",
             .text = "New",
             .tooltip = "Create a new experimental Qt session",
             .shortcut = "Ctrl+N",
             .menu = "File"},
            [this]() { newSession(); });

    this->window.commandHost()->registerCommand(
            {.id = "app.open",
             .text = "Open...",
             .tooltip = "Open an experimental Qt session or a VertexNote document",
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
             .tooltip = "Show the experimental Qt updater surface",
             .menu = "Help"},
            [this]() {
                this->updates.showCheckingForUpdates();
                this->updates.showUpToDate("qt-bootstrap");
            });

    this->window.commandHost()->registerCommand(
            {.id = "app.about-qt-shell",
             .text = "About Qt Shell",
             .tooltip = "Show the experimental Qt shell status",
             .menu = "Help"},
            [this]() {
                this->dialogs.showInfo("VertexNote Qt Experimental Shell",
                                       "Qt shell bootstrap is active.\n\n"
                                       "Current slice includes neutral UI services, input translation, a Qt painter "
                                       "render seam, real document-backed page previews, and an experimental "
                                       "viewport session flow with pan/zoom.");
            });

    this->window.commandHost()->registerCommand(
            {.id = "view.zoom-in",
             .text = "Zoom In",
             .tooltip = "Zoom in on the Qt experimental canvas",
             .shortcut = "Ctrl+=",
             .menu = "View"},
            [this]() { this->window.canvas()->zoomIn(); });

    this->window.commandHost()->registerCommand(
            {.id = "view.zoom-out",
             .text = "Zoom Out",
             .tooltip = "Zoom out on the Qt experimental canvas",
             .shortcut = "Ctrl+-",
             .menu = "View"},
            [this]() { this->window.canvas()->zoomOut(); });

    this->window.commandHost()->registerCommand(
            {.id = "view.zoom-reset",
             .text = "Reset View",
             .tooltip = "Reset the experimental canvas viewport",
             .shortcut = "Ctrl+0",
             .menu = "View"},
            [this]() { this->window.canvas()->resetViewport(); });

    this->window.commandHost()->registerCommand(
            {.id = "view.fit-page",
             .text = "Fit Page",
             .tooltip = "Fit the page into the Qt experimental canvas",
             .shortcut = "Ctrl+9",
             .menu = "View"},
            [this]() { this->window.canvas()->fitPage(); });
}

void QtExperimentalAppShell::wireWindowState() {
    QObject::connect(this->window.canvas(), &QtExperimentalCanvas::statusHintChanged, &this->window,
                     [this](const QString& text) { this->window.statusBar()->showMessage(text); });

    QObject::connect(this->window.canvas(), &QtExperimentalCanvas::viewportStateChanged, &this->window,
                     [this]() { updateWindowTitle(); });

    QObject::connect(this->window.canvas(), &QtExperimentalCanvas::documentEdited, &this->window,
                     [this]() {
                         if (!this->suppressDirtyTracking) {
                             markSessionDirty();
                         }
                     });
}

void QtExperimentalAppShell::rebuildToolbar() {
    auto* toolBar = this->window.mainToolBar();
    toolBar->clear();

    const std::array<std::string_view, 7> commandIds = {"app.new",       "app.open",      "app.save-as",
                                                         "view.zoom-in",  "view.zoom-out",  "view.zoom-reset",
                                                         "view.fit-page"};

    for (const auto id: commandIds) {
        if (auto* action = this->window.commandHost()->actionForCommand(id)) {
            toolBar->addAction(action);
            if (id == "app.save-as" || id == "view.zoom-out") {
                toolBar->addSeparator();
            }
        }
    }
}

void QtExperimentalAppShell::updateWindowTitle() {
    const auto title = std::string("VertexNote - ") + this->documentController.titleText() +
                       (this->session.isDirty() ? " *" : "");
    setMainWindowTitle(title);
}

void QtExperimentalAppShell::newSession() {
    this->session.newDocument();
    this->documentController.newBlankDocument();
    this->suppressDirtyTracking = true;
    this->window.canvas()->newBlankDocument();
    this->suppressDirtyTracking = false;
    this->window.statusBar()->showMessage(QStringLiteral("Created a blank experimental document"), 3000);
    updateWindowTitle();
}

void QtExperimentalAppShell::openSession() {
    const auto path = this->dialogs.openDocument(SESSION_FILTERS);
    if (!path) {
        return;
    }

    if (isSessionFile(*path)) {
        const auto sessionState = this->session.openFrom(*path);
        if (!sessionState) {
            this->dialogs.showError("Open Failed", "VertexNote could not parse this experimental Qt session file.");
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
    this->window.statusBar()->showMessage(QString::fromStdString("Opened document " + path->filename().string()), 4000);
    updateWindowTitle();
}

void QtExperimentalAppShell::saveSessionAs() {
    const auto path = this->dialogs.saveDocument(this->session.currentPath().value_or(std::filesystem::path("session.vnsession")),
                                                 SESSION_FILTERS);
    if (!path) {
        return;
    }

    const QtExperimentalSessionState sessionState{.viewport = this->window.canvas()->sessionViewportState(),
                                                  .linkedDocumentPath = this->documentController.sourcePath()};
    if (!this->session.saveAs(*path, sessionState)) {
        this->dialogs.showError("Save Failed", "VertexNote could not save the experimental Qt session file.");
        return;
    }

    this->recentFiles.addRecentFile(*path);
    this->window.statusBar()->showMessage(QString::fromStdString("Saved " + path->filename().string()), 4000);
    updateWindowTitle();
}

void QtExperimentalAppShell::markSessionDirty() {
    if (!this->session.isDirty()) {
        this->session.markDirty(true);
        updateWindowTitle();
    }
}
