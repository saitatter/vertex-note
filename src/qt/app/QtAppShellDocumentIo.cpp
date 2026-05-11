/*
 * VertexNote
 *
 * Qt app shell document and session workflows.
 */

#include "QtAppShell.h"

#include <algorithm>
#include <filesystem>
#include <string>
#include <vector>

#include <QAction>
#include <QDir>
#include <QFileDialog>
#include <QMenu>
#include <QMessageBox>
#include <QStatusBar>
#include <QString>
#include <QStringList>

namespace {

const std::vector<vn::ui::common::FileDialogFilter> SESSION_FILTERS = {
        {.label = "VertexNote Qt Session", .patterns = {"*.vnsession"}},
        {.label = "VertexNote Documents", .patterns = {"*.xopp", "*.xoj", "*.xopt", "*.pdf"}},
        {.label = "All Files", .patterns = {"*"}},
};

auto isSessionFile(const std::filesystem::path& path) -> bool { return path.extension() == ".vnsession"; }

auto joinFileDialogFilters(const std::vector<vn::ui::common::FileDialogFilter>& filters) -> QString {
    QStringList items;
    for (const auto& filter: filters) {
        QStringList patterns;
        for (const auto& pattern: filter.patterns) {
            patterns << QString::fromStdString(pattern);
        }
        items << QString::fromStdString(filter.label) + " (" + patterns.join(' ') + ")";
    }
    return items.join(";;");
}

}  // namespace
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
    syncFooterWidgets();
    this->window.statusBar()->showMessage(QStringLiteral("Created a blank document"), 3000);
    updateWindowTitle();
}

void QtAppShell::rebuildRecentDocumentsMenu() {
    auto* menu = this->window.commandHost()->menuForPath("File>Recent Documents");
    menu->clear();

    const auto recentPaths = this->recentFiles.recentFiles();
    if (recentPaths.empty()) {
        auto* emptyAction = menu->addAction(QStringLiteral("No Recent Documents"));
        emptyAction->setEnabled(false);
        return;
    }

    for (std::size_t index = 0; index < recentPaths.size(); ++index) {
        const auto& path = recentPaths[index];
        const QString filename = QString::fromStdString(path.filename().string());
        const QString fullPath = QString::fromStdString(path.string());
        auto* action =
                menu->addAction(QStringLiteral("&%1 %2").arg(index + 1).arg(filename.isEmpty() ? fullPath : filename));
        action->setToolTip(fullPath);
        action->setStatusTip(fullPath);
        QObject::connect(action, &QAction::triggered, &this->window, [this, path]() { openPath(path, true); });
    }

    menu->addSeparator();
    auto* clearAction = menu->addAction(QStringLiteral("Clear Recent Documents"));
    QObject::connect(clearAction, &QAction::triggered, &this->window, [this]() {
        this->recentFiles.setRecentFiles({});
        rebuildRecentDocumentsMenu();
        savePersistentUiState();
        this->window.statusBar()->showMessage(QStringLiteral("Recent documents cleared"), 3000);
    });
}

auto QtAppShell::openPath(const std::filesystem::path& path, bool fromRecentDocuments) -> bool {
    if (!std::filesystem::exists(path)) {
        if (fromRecentDocuments) {
            auto recentPaths = this->recentFiles.recentFiles();
            recentPaths.erase(std::remove(recentPaths.begin(), recentPaths.end(), path), recentPaths.end());
            this->recentFiles.setRecentFiles(recentPaths);
            rebuildRecentDocumentsMenu();
            savePersistentUiState();
        }
        this->dialogs.showError("Open Failed",
                                "VertexNote could not find this recent document anymore. It was removed from the list.");
        return false;
    }

    if (isSessionFile(path)) {
        const auto sessionState = this->session.openFrom(path);
        if (!sessionState) {
            this->dialogs.showError("Open Failed", "VertexNote could not parse this Qt session file.");
            return false;
        }

        if (sessionState->linkedDocumentPath) {
            std::string error;
            if (!this->documentController.loadFrom(*sessionState->linkedDocumentPath, &error)) {
                this->dialogs.showError("Open Failed", error.empty() ? "VertexNote could not open the linked document."
                                                                     : error);
                return false;
            }
        } else {
            this->documentController.newBlankDocument();
        }

        this->suppressDirtyTracking = true;
        this->window.canvas()->setViewportState(sessionState->viewport.zoom, sessionState->viewport.scrollX,
                                                sessionState->viewport.scrollY);
        this->suppressDirtyTracking = false;
        this->recentFiles.addRecentFile(path);
        this->currentSettings.lastOpenPath = path.parent_path().string();
        rebuildRecentDocumentsMenu();
        savePersistentUiState();
        updateEditCommandStates();
        this->window.layerPanel()->refresh();
        this->window.pageSidebar()->refresh();
        syncFooterWidgets();
        this->window.statusBar()->showMessage(QString::fromStdString("Opened session " + path.filename().string()), 4000);
        updateWindowTitle();
        return true;
    }

    std::string error;
    if (!this->documentController.loadFrom(path, &error)) {
        this->dialogs.showError("Open Failed", error.empty() ? "VertexNote could not open this document." : error);
        return false;
    }

    this->session.newDocument();
    this->suppressDirtyTracking = true;
    this->window.canvas()->fitWidth();
    this->suppressDirtyTracking = false;
    this->recentFiles.addRecentFile(path);
    this->currentSettings.lastOpenPath = path.parent_path().string();
    rebuildRecentDocumentsMenu();
    savePersistentUiState();
    updateEditCommandStates();
    this->window.layerPanel()->refresh();
    this->window.pageSidebar()->refresh();
    syncFooterWidgets();
    this->window.statusBar()->showMessage(QString::fromStdString("Opened document " + path.filename().string()), 4000);
    updateWindowTitle();
    return true;
}

void QtAppShell::openSession() {
    const QString filePath = QFileDialog::getOpenFileName(&this->window, QStringLiteral("Open Document"),
                                                          dialogInitialDirectory(this->currentSettings.lastOpenPath),
                                                          joinFileDialogFilters(SESSION_FILTERS));
    if (filePath.isEmpty()) {
        return;
    }
    rememberDialogPath(this->currentSettings.lastOpenPath, filePath);
    openPath(std::filesystem::path(filePath.toStdWString()), false);
}

void QtAppShell::saveSessionAs() {
    const auto suggestedPath = this->session.currentPath().value_or(std::filesystem::path("session.vnsession"));
    QString initialPath = QString::fromStdWString(suggestedPath.wstring());
    if (!this->currentSettings.lastSavePath.empty() && !suggestedPath.is_absolute()) {
        initialPath = QDir(dialogInitialDirectory(this->currentSettings.lastSavePath)).filePath(initialPath);
    }
    const QString filePath = QFileDialog::getSaveFileName(&this->window, QStringLiteral("Save Document"), initialPath,
                                                          joinFileDialogFilters(SESSION_FILTERS));
    if (filePath.isEmpty()) {
        return;
    }
    rememberDialogPath(this->currentSettings.lastSavePath, filePath);
    const auto path = std::filesystem::path(filePath.toStdWString());

    const QtSessionState sessionState{.viewport = this->window.canvas()->sessionViewportState(),
                                                  .linkedDocumentPath = this->documentController.sourcePath()};
    if (!this->session.saveAs(path, sessionState)) {
        this->dialogs.showError("Save Failed", "VertexNote could not save the Qt session file.");
        return;
    }

    this->recentFiles.addRecentFile(path);
    rebuildRecentDocumentsMenu();
    savePersistentUiState();
    this->window.statusBar()->showMessage(QString::fromStdString("Saved " + path.filename().string()), 4000);
    updateWindowTitle();
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
                                                              dialogInitialDirectory(this->currentSettings.lastSavePath),
                                                              QStringLiteral("VertexNote Files (*.xopp)"));
        if (filePath.isEmpty()) {
            return;
        }
        rememberDialogPath(this->currentSettings.lastSavePath, filePath);
        savePath = filePath.toStdString();
    }

    std::string errorMsg;
    if (this->documentController.saveDocument(savePath, &errorMsg)) {
        this->session.markDirty(false);
        this->recentFiles.addRecentFile(savePath);
        rebuildRecentDocumentsMenu();
        savePersistentUiState();
        updateWindowTitle();
        this->window.statusBar()->showMessage(QStringLiteral("Document saved"), 3000);
    } else {
        QMessageBox::warning(&this->window, QStringLiteral("Save Failed"), QString::fromStdString(errorMsg));
    }
}

void QtAppShell::markSessionDirty() {
    if (!this->session.isDirty()) {
        this->session.markDirty(true);
        updateWindowTitle();
    }
}
