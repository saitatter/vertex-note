/*
 * VertexNote
 *
 * Experimental Qt update presentation service.
 */

#include "QtUpdatePresentationService.h"

#include <QMessageBox>
#include <QStatusBar>
#include <QString>

QtUpdatePresentationService::QtUpdatePresentationService(QWidget* parent, QStatusBar* statusBar):
        parent(parent), statusBar(statusBar) {}

void QtUpdatePresentationService::showCheckingForUpdates() {
    if (this->statusBar) {
        this->statusBar->showMessage(QStringLiteral("Checking for updates..."), 3000);
    }
}

void QtUpdatePresentationService::showUpdateAvailable(const vn::ui::common::UpdateReleaseSummary& release) {
    if (this->statusBar) {
        this->statusBar->showMessage(QString::fromStdString("Update available: " + release.version), 5000);
    }
    QMessageBox::information(this->parent, QStringLiteral("Update Available"),
                             QString::fromStdString(release.title + "\n\n" + release.notes));
}

void QtUpdatePresentationService::showUpToDate(std::string_view currentVersion) {
    if (this->statusBar) {
        this->statusBar->showMessage(QString::fromStdString("VertexNote is up to date (" + std::string(currentVersion) + ")"),
                                     4000);
    }
}

void QtUpdatePresentationService::showUpdateError(std::string_view message) {
    QMessageBox::warning(this->parent, QStringLiteral("Update Check Failed"),
                         QString::fromUtf8(message.data(), static_cast<int>(message.size())));
}
