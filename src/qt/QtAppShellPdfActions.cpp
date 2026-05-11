/*
 * VertexNote
 *
 * Qt app shell PDF, export, and print workflows.
 */

#include "QtAppShell.h"

#include <filesystem>
#include <string>

#include <QFileDialog>
#include <QMessageBox>
#include <QStatusBar>
#include <QString>
void QtAppShell::annotatePdf() {
    const QString filePath = QFileDialog::getOpenFileName(&this->window, QStringLiteral("Annotate PDF"),
                                                          dialogInitialDirectory(this->currentSettings.lastPdfPath),
                                                          QStringLiteral("PDF Files (*.pdf)"));
    if (filePath.isEmpty()) {
        return;
    }
    rememberDialogPath(this->currentSettings.lastPdfPath, filePath);

    const auto attachAnswer =
            QMessageBox::question(&this->window, QStringLiteral("Annotate PDF"),
                                  QStringLiteral("Attach the PDF data to the document when saving?"),
                                  QMessageBox::Yes | QMessageBox::No | QMessageBox::Cancel, QMessageBox::Yes);
    if (attachAnswer == QMessageBox::Cancel) {
        return;
    }

    std::string error;
    const auto path = std::filesystem::path(filePath.toStdString());
    if (!this->documentController.loadPdfAsDocument(path, attachAnswer == QMessageBox::Yes, &error)) {
        this->dialogs.showError("Annotate PDF Failed",
                                error.empty() ? "VertexNote could not open this PDF." : error);
        return;
    }

    this->session.newDocument();
    this->suppressDirtyTracking = true;
    this->window.canvas()->fitWidth();
    this->suppressDirtyTracking = false;
    this->recentFiles.addRecentFile(path);
    rebuildRecentDocumentsMenu();
    savePersistentUiState();
    updateEditCommandStates();
    this->window.layerPanel()->refresh();
    this->window.pageSidebar()->refresh();
    syncFooterWidgets();
    this->window.statusBar()->showMessage(QStringLiteral("PDF opened for annotation"), 4000);
    updateWindowTitle();
}

void QtAppShell::exportPdf() {
    if (!this->documentController.hasDocument()) {
        return;
    }

    const QString filePath = QFileDialog::getSaveFileName(&this->window, QStringLiteral("Export PDF"),
                                                          dialogInitialDirectory(this->currentSettings.lastExportPath),
                                                          QStringLiteral("PDF Files (*.pdf)"));
    if (filePath.isEmpty()) {
        return;
    }
    rememberDialogPath(this->currentSettings.lastExportPath, filePath);

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
                                                          dialogInitialDirectory(this->currentSettings.lastExportPath),
                                                          QStringLiteral("PNG Images (*.png)"));
    if (filePath.isEmpty()) {
        return;
    }
    rememberDialogPath(this->currentSettings.lastExportPath, filePath);

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
