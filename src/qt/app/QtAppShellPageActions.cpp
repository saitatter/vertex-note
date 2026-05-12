/*
 * VertexNote
 *
 * Qt app shell page actions.
 */

#include "QtAppShell.h"

#include <QStatusBar>
#include <QString>
void QtAppShell::addPage() {
    if (!this->documentController.hasDocument()) {
        return;
    }

    // Add after the last page
    const std::size_t pageCount = this->documentController.pageCount();
    this->documentController.addPageAfter(pageCount > 0 ? pageCount - 1 : 0);
    this->window.canvas()->update();
    this->window.pageSidebar()->refresh();
    updateEditCommandStates();
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
    updateEditCommandStates();
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
    updateEditCommandStates();
    markSessionDirty();
    this->window.statusBar()->showMessage(QStringLiteral("Page duplicated"), 3000);
}
