/*
 * VertexNote
 *
 * Qt app shell navigation and layout actions.
 */

#include "QtAppShell.h"

#include <algorithm>
#include <cmath>
#include <string>
#include <vector>

#include <QInputDialog>
#include <QLineEdit>
#include <QMenuBar>
#include <QStatusBar>
#include <QString>
#include <QToolBar>

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

void QtAppShell::recordNavPoint() {
    auto* canvas = this->window.canvas();
    const auto state = canvas->sessionViewportState();
    NavPoint point{.pageIndex = canvas->currentPageIndex(),
                   .scrollX = state.scrollX,
                   .scrollY = state.scrollY,
                   .zoom = state.zoom};

    if (this->navHistoryIndex < this->navHistory.size()) {
        this->navHistory.resize(this->navHistoryIndex);
    }

    if (!this->navHistory.empty()) {
        const auto& last = this->navHistory.back();
        if (last.pageIndex == point.pageIndex && std::abs(last.scrollX - point.scrollX) < 1.0 &&
            std::abs(last.scrollY - point.scrollY) < 1.0) {
            return;
        }
    }

    this->navHistory.push_back(point);
    this->navHistoryIndex = this->navHistory.size();

    if (this->navHistory.size() > 100) {
        this->navHistory.erase(this->navHistory.begin());
        this->navHistoryIndex--;
    }
}

auto QtAppShell::selectedElementsForAudioPlayback() const -> std::vector<const Element*> {
    if (const auto& selection = this->documentController.elementSelection(); selection.has_value()) {
        return selection->elements;
    }
    return {};
}

void QtAppShell::navigateBack() {
    if (this->navHistoryIndex == 0 || this->navHistory.empty()) {
        this->window.statusBar()->showMessage(QStringLiteral("No previous position"), 3000);
        return;
    }

    if (this->navHistoryIndex == this->navHistory.size()) {
        recordNavPoint();
        this->navHistoryIndex--;
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

void QtAppShell::gotoNextLayer() {
    auto pageIdx = this->window.canvas()->currentPageIndex();
    auto current = this->documentController.selectedLayerIndex(pageIdx);
    auto count = this->documentController.layerCount(pageIdx);
    if (current + 1 < count) {
        this->documentController.selectLayer(pageIdx, current + 1);
        this->window.canvas()->update();
        this->window.statusBar()->showMessage(QStringLiteral("Layer %1 / %2").arg(current + 2).arg(count), 3000);
    }
}

void QtAppShell::gotoPrevLayer() {
    auto pageIdx = this->window.canvas()->currentPageIndex();
    auto current = this->documentController.selectedLayerIndex(pageIdx);
    if (current > 0) {
        this->documentController.selectLayer(pageIdx, current - 1);
        this->window.canvas()->update();
        auto count = this->documentController.layerCount(pageIdx);
        this->window.statusBar()->showMessage(QStringLiteral("Layer %1 / %2").arg(current).arg(count), 3000);
    }
}

void QtAppShell::gotoTopLayer() {
    auto pageIdx = this->window.canvas()->currentPageIndex();
    auto count = this->documentController.layerCount(pageIdx);
    if (count > 0) {
        this->documentController.selectLayer(pageIdx, count - 1);
        this->window.canvas()->update();
        this->window.statusBar()->showMessage(QStringLiteral("Top layer (%1 / %2)").arg(count).arg(count), 3000);
    }
}

void QtAppShell::addLayerAbove() {
    auto pageIdx = this->window.canvas()->currentPageIndex();
    this->documentController.addLayer(pageIdx);
    auto count = this->documentController.layerCount(pageIdx);
    this->documentController.selectLayer(pageIdx, count - 1);
    this->window.canvas()->update();
    markSessionDirty();
    this->window.statusBar()->showMessage(QStringLiteral("Added layer above"), 3000);
}

void QtAppShell::addLayerBelow() {
    auto pageIdx = this->window.canvas()->currentPageIndex();
    auto current = this->documentController.selectedLayerIndex(pageIdx);
    this->documentController.addLayer(pageIdx);
    this->documentController.selectLayer(pageIdx, current);
    this->window.canvas()->update();
    markSessionDirty();
    this->window.statusBar()->showMessage(QStringLiteral("Added layer below"), 3000);
}

void QtAppShell::gotoNextAnnotatedPage() {
    auto current = this->window.canvas()->currentPageIndex();
    auto count = this->documentController.pageCount();
    for (std::size_t i = current + 1; i < count; ++i) {
        if (this->documentController.isPageAnnotated(i)) {
            recordNavPoint();
            this->window.canvas()->scrollToPage(i);
            this->window.canvas()->update();
            this->window.statusBar()->showMessage(QStringLiteral("Annotated page %1").arg(i + 1), 3000);
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
            this->window.statusBar()->showMessage(QStringLiteral("Annotated page %1").arg(i), 3000);
            return;
        }
    }
    this->window.statusBar()->showMessage(QStringLiteral("No previous annotated pages"), 3000);
}

void QtAppShell::togglePairedPages() {
    const bool enabled = !this->window.canvas()->isPairedPagesEnabled();
    if (enabled) {
        this->window.canvas()->setLayoutColumns(2);
    } else {
        this->window.canvas()->setLayoutColumns(1);
    }
    syncLayoutSpanCommandStates();
    savePersistentUiState();
    this->window.statusBar()->showMessage(
            enabled ? QStringLiteral("Paired pages enabled") : QStringLiteral("Paired pages disabled"), 3000);
    syncFooterWidgets();
}

void QtAppShell::toggleToolbarVisibility() {
    const bool visible = !this->window.mainToolBar()->isVisible();
    this->window.mainToolBar()->setVisible(visible);
    this->window.toolsToolBar()->setVisible(visible);
    this->window.footerToolBar()->setVisible(visible);
    applyAuxiliaryToolBarVisibility(visible);
    this->window.commandHost()->setCommandChecked("view.show-toolbar", visible);
    savePersistentUiState();
}

void QtAppShell::toggleMenubarVisibility() {
    auto* menubar = this->window.menuBar();
    const bool visible = !menubar->isVisible();
    menubar->setVisible(visible);
    this->window.commandHost()->setCommandChecked("view.show-menubar", visible);
    savePersistentUiState();
}

void QtAppShell::toggleSidebarVisibility() {
    auto* sidebar = this->window.pageSidebar();
    const bool visible = !sidebar->isVisible();
    applySidebarVisibility(visible);
    this->window.commandHost()->setCommandChecked("view.show-sidebar", visible);
    savePersistentUiState();
}

void QtAppShell::setLayoutVertical(bool vertical) {
    this->window.canvas()->setVerticalLayout(vertical);
    this->window.commandHost()->setCommandChecked("view.layout-horizontal", !vertical);
    this->window.commandHost()->setCommandChecked("view.layout-vertical", vertical);
    savePersistentUiState();
    this->window.statusBar()->showMessage(
            vertical ? QStringLiteral("Vertical layout") : QStringLiteral("Horizontal layout"), 3000);
    syncFooterWidgets();
}

void QtAppShell::setLayoutRtl(bool rtl) {
    this->window.canvas()->setRightToLeftLayout(rtl);
    this->window.commandHost()->setCommandChecked("view.layout-ltr", !rtl);
    this->window.commandHost()->setCommandChecked("view.layout-rtl", rtl);
    savePersistentUiState();
    this->window.statusBar()->showMessage(
            rtl ? QStringLiteral("Right to left") : QStringLiteral("Left to right"), 3000);
    syncFooterWidgets();
}

void QtAppShell::setLayoutBtt(bool btt) {
    this->window.canvas()->setBottomToTopLayout(btt);
    this->window.commandHost()->setCommandChecked("view.layout-ttb", !btt);
    this->window.commandHost()->setCommandChecked("view.layout-btt", btt);
    savePersistentUiState();
    this->window.statusBar()->showMessage(
            btt ? QStringLiteral("Bottom to top") : QStringLiteral("Top to bottom"), 3000);
    syncFooterWidgets();
}

void QtAppShell::setLayoutColumns(int columns) {
    this->window.canvas()->setLayoutColumns(columns);
    syncLayoutSpanCommandStates();
    savePersistentUiState();
    this->window.statusBar()->showMessage(
            QStringLiteral("%1 page %2").arg(columns).arg(columns == 1 ? QStringLiteral("column") : QStringLiteral("columns")),
            3000);
    syncFooterWidgets();
}

void QtAppShell::setLayoutRows(int rows) {
    this->window.canvas()->setLayoutRows(rows);
    syncLayoutSpanCommandStates();
    savePersistentUiState();
    this->window.statusBar()->showMessage(
            QStringLiteral("%1 page %2").arg(rows).arg(rows == 1 ? QStringLiteral("row") : QStringLiteral("rows")),
            3000);
    syncFooterWidgets();
}

void QtAppShell::setPairOffset(int offset) {
    this->window.canvas()->setPairOffset(offset);
    syncLayoutSpanCommandStates();
    savePersistentUiState();
    this->window.statusBar()->showMessage(QStringLiteral("Pair offset %1").arg(offset), 3000);
    syncFooterWidgets();
}

void QtAppShell::syncLayoutSpanCommandStates() {
    const int value = this->window.canvas()->layoutColumnsRows();
    this->window.commandHost()->setCommandChecked("view.paired-pages", value == 2);
    for (int columns = 1; columns <= 8; ++columns) {
        this->window.commandHost()->setCommandChecked("view.columns-" + std::to_string(columns), value == columns);
    }
    for (int rows = 1; rows <= 8; ++rows) {
        this->window.commandHost()->setCommandChecked("view.rows-" + std::to_string(rows), value == -rows);
    }
    const int pairOffset = this->window.canvas()->pairOffset();
    for (int offset = 0; offset <= 1; ++offset) {
        this->window.commandHost()->setCommandChecked("view.pair-offset-" + std::to_string(offset), pairOffset == offset);
    }
}
