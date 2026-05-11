/*
 * VertexNote
 *
 * Qt app shell core dialogs.
 */

#include "QtAppShell.h"

#include <cstddef>
#include <string>

#include <QAction>
#include <QByteArray>
#include <QDialog>
#include <QFileInfo>
#include <QStatusBar>
#include <QString>

#include "QtBackgroundDialog.h"
#include "QtSettingsDialog.h"
#include "util/PathUtil.h"

namespace {

void applyQtPreferredLocale(const std::string& preferredLocale) {
    qputenv("LANGUAGE", QByteArray(preferredLocale.c_str(), static_cast<qsizetype>(preferredLocale.size())));
}

}  // namespace

void QtAppShell::showBackgroundDialog() {
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

void QtAppShell::showSettingsDialog() {
    QtSettingsDialog dialog(this->currentSettings, this->availableToolbarProfiles,
                            this->audioController.inputDeviceOptions(),
                            this->audioController.outputDeviceOptions(), &this->window);
    if (dialog.exec() != QDialog::Accepted) {
        return;
    }

    const auto previousToolbarProfileId = this->currentSettings.toolbarProfileId;
    const auto previousIconTheme = this->currentSettings.iconTheme;
    const auto previousThemeVariant = this->currentSettings.themeVariant;
    const auto previousLocale = this->currentSettings.preferredLocale;
    this->currentSettings = dialog.settings();
    if (this->currentSettings.audioFolder.empty()) {
        this->currentSettings.audioFolder = Util::getDataSubfolder("audio").string();
    }

    applyRuntimeSettings();
    if (this->currentSettings.preferredLocale != previousLocale) {
        applyQtPreferredLocale(this->currentSettings.preferredLocale);
    }
    this->audioController.applySettings(this->currentSettings);
    this->window.commandHost()->setCommandChecked("view.toggle-geometry-snap", this->currentSettings.geometrySnapDefault);
    this->window.commandHost()->setCommandChecked("view.toggle-grid-snap", this->currentSettings.gridSnapDefault);
    this->window.commandHost()->setCommandChecked("view.toggle-rotation-snap", this->currentSettings.rotationSnapDefault);
    this->window.commandHost()->setCommandChecked("view.toggle-touch-drawing", this->currentSettings.touchDrawingDefault);

    this->window.toolPalette()->syncFromToolState(this->window.canvas()->toolState());
    if (this->currentSettings.toolbarProfileId != previousToolbarProfileId ||
        this->currentSettings.iconTheme != previousIconTheme ||
        this->currentSettings.themeVariant != previousThemeVariant) {
        rebuildToolbar();
        applySidebarVisibility(this->window.commandHost()->actionForCommand("view.show-sidebar")
                                       ? this->window.commandHost()->actionForCommand("view.show-sidebar")->isChecked()
                                       : this->persistedShowSidebar);
    }
    configureAutosave();
    savePersistentUiState();
    updateWindowTitle();
    updateAudioCommandStates();
    this->window.statusBar()->showMessage(QStringLiteral("Settings applied"), 3000);
}

auto QtAppShell::dialogInitialDirectory(const std::string& storedPath) const -> QString {
    if (storedPath.empty()) {
        return QString();
    }
    const QFileInfo info(QString::fromStdString(storedPath));
    if (info.exists() && info.isDir()) {
        return info.absoluteFilePath();
    }
    if (info.exists()) {
        return info.absolutePath();
    }
    return QString();
}

void QtAppShell::rememberDialogPath(std::string& storedPath, const QString& filePath) {
    const QFileInfo info(filePath);
    const QString directory = info.isDir() ? info.absoluteFilePath() : info.absolutePath();
    if (!directory.isEmpty()) {
        storedPath = directory.toStdString();
    }
}
