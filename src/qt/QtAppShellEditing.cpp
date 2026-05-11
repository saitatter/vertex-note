/*
 * VertexNote
 *
 * Qt app shell editing, page/layer, and tool style actions.
 */

#include "QtAppShell.h"

#include <algorithm>
#include <array>
#include <utility>
#include <vector>

#include <QAction>
#include <QFont>
#include <QFontDialog>
#include <QInputDialog>
#include <QLineEdit>
#include <QMessageBox>
#include <QSignalBlocker>
#include <QStatusBar>
#include <QString>

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
    this->window.statusBar()->showMessage(QStringLiteral("Copied %1 element(s)").arg(this->elementClipboard.size()), 3000);
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
    this->window.statusBar()->showMessage(QStringLiteral("Cut %1 element(s)").arg(this->elementClipboard.size()), 3000);
}

void QtAppShell::pasteClipboard() {
    if (this->elementClipboard.empty()) {
        this->window.statusBar()->showMessage(QStringLiteral("Clipboard is empty"), 3000);
        return;
    }
    if (!this->documentController.hasDocument()) {
        return;
    }

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
    markSessionDirty();
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
    markSessionDirty();
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
    const QString newName = QInputDialog::getText(&this->window, QStringLiteral("Rename Layer"),
                                                  QStringLiteral("Layer name:"), QLineEdit::Normal,
                                                  QString::fromStdString(infos[layerIndex].name), &ok);
    if (ok && !newName.isEmpty()) {
        this->documentController.renameLayer(pageIndex, layerIndex, newName.toStdString());
        this->window.layerPanel()->refresh();
        markSessionDirty();
        this->window.statusBar()->showMessage(QStringLiteral("Layer renamed"), 3000);
    }
}

void QtAppShell::addPageBefore() {
    if (!this->documentController.hasDocument()) {
        return;
    }
    const auto pageIndex = this->window.canvas()->currentPageIndex();
    this->documentController.addPageBefore(pageIndex);
    this->window.canvas()->update();
    this->window.pageSidebar()->refresh();
    updateEditCommandStates();
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
    updateEditCommandStates();
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
    updateEditCommandStates();
    markSessionDirty();
    this->window.statusBar()->showMessage(QStringLiteral("Page moved down"), 3000);
}

void QtAppShell::bringToFront() {
    if (this->documentController.bringSelectionToFront()) {
        this->window.canvas()->update();
        markSessionDirty();
        this->window.statusBar()->showMessage(QStringLiteral("Brought to front"), 3000);
    }
}

void QtAppShell::sendToBack() {
    if (this->documentController.sendSelectionToBack()) {
        this->window.canvas()->update();
        markSessionDirty();
        this->window.statusBar()->showMessage(QStringLiteral("Sent to back"), 3000);
    }
}

void QtAppShell::bringForward() {
    if (this->documentController.bringSelectionForward()) {
        this->window.canvas()->update();
        markSessionDirty();
        this->window.statusBar()->showMessage(QStringLiteral("Brought forward"), 3000);
    }
}

void QtAppShell::sendBackward() {
    if (this->documentController.sendSelectionBackward()) {
        this->window.canvas()->update();
        markSessionDirty();
        this->window.statusBar()->showMessage(QStringLiteral("Sent backward"), 3000);
    }
}

void QtAppShell::setPenLineStyle(const std::string& style) {
    auto& ts = this->window.canvas()->toolState();
    ts.penLineStyle = style;
    this->window.commandHost()->setCommandChecked("pen.line-solid", style == "plain");
    this->window.commandHost()->setCommandChecked("pen.line-dash", style == "dash");
    this->window.commandHost()->setCommandChecked("pen.line-dashdot", style == "dashdot");
    this->window.commandHost()->setCommandChecked("pen.line-dot", style == "dot");
    this->window.statusBar()->showMessage(QStringLiteral("Line style: %1").arg(QString::fromStdString(style)), 3000);
}

void QtAppShell::setStrokeFill(int fillOpacity) {
    auto& ts = this->window.canvas()->toolState();
    ts.fillOpacity = fillOpacity;
    ts.fillEnabled = fillOpacity > 0;
    if (this->toolbarFillAction) {
        const QSignalBlocker blocker(this->toolbarFillAction);
        this->toolbarFillAction->setChecked(ts.fillEnabled);
    }
    this->window.statusBar()->showMessage(QStringLiteral("Fill opacity: %1").arg(fillOpacity), 2500);
}

void QtAppShell::setPdfTextMarkerOpacity(int opacity) {
    auto& ts = this->window.canvas()->toolState();
    ts.pdfTextMarkerOpacity = std::clamp(opacity, 0, 255);
    this->window.canvas()->update();
    this->window.statusBar()->showMessage(QStringLiteral("PDF text marker opacity: %1").arg(ts.pdfTextMarkerOpacity),
                                          2500);
}

void QtAppShell::highlightPdfTextSelection() {
    auto& ts = this->window.canvas()->toolState();
    const int inserted = this->documentController.createPdfTextMarkerStrokesForSelection(
            QtPdfTextMarkerKind::Highlight, ts.pdfTextMarkerOpacity, ts.highlighterColor);
    if (inserted <= 0) {
        this->window.statusBar()->showMessage(QStringLiteral("No active PDF text selection to highlight"), 3000);
        return;
    }

    this->documentController.cancelPdfTextSelection();
    this->window.canvas()->update();
    updateEditCommandStates();
    markSessionDirty();
    this->window.statusBar()->showMessage(QStringLiteral("Highlighted selected PDF text"), 3000);
}

void QtAppShell::selectFont() {
    auto& ts = this->window.canvas()->toolState();

    bool ok = false;
    QFont current;
    current.setFamily(QString::fromStdString(ts.fontName));
    current.setPointSizeF(ts.fontSize);

    QFont selected = QFontDialog::getFont(&ok, current, &this->window, QStringLiteral("Select Font"));
    if (ok) {
        ts.fontName = selected.family().toStdString();
        ts.fontSize = selected.pointSizeF();
        this->window.statusBar()->showMessage(
                QStringLiteral("Font: %1 %2pt").arg(selected.family()).arg(selected.pointSizeF()), 3000);
    }
}

namespace {
constexpr std::array<double, 5> PEN_SIZES = {0.40, 0.85, 1.41, 3.54, 5.00};
constexpr std::array<double, 5> ERASER_SIZES = {3.00, 5.00, 8.50, 14.00, 20.00};
constexpr std::array<double, 5> HIGHLIGHTER_SIZES = {3.00, 5.00, 8.50, 14.00, 20.00};

const std::array<const char*, 5> PEN_SIZE_IDS = {
        "pen.size-very-fine", "pen.size-fine", "pen.size-medium", "pen.size-thick", "pen.size-very-thick"};
const std::array<const char*, 5> ERASER_SIZE_IDS = {
        "eraser.size-very-fine", "eraser.size-fine", "eraser.size-medium", "eraser.size-thick", "eraser.size-very-thick"};
const std::array<const char*, 5> HIGHLIGHTER_SIZE_IDS = {
        "highlighter.size-very-fine", "highlighter.size-fine", "highlighter.size-medium", "highlighter.size-thick",
        "highlighter.size-very-thick"};
}  // namespace

void QtAppShell::setPenSize(int sizeIndex) {
    if (sizeIndex < 0 || sizeIndex >= 5) {
        return;
    }
    this->window.canvas()->toolState().penWidth = PEN_SIZES[static_cast<std::size_t>(sizeIndex)];
    for (int i = 0; i < 5; ++i) {
        this->window.commandHost()->setCommandChecked(PEN_SIZE_IDS[static_cast<std::size_t>(i)], i == sizeIndex);
    }
    this->window.toolPalette()->syncFromToolState(this->window.canvas()->toolState());
}

void QtAppShell::setEraserSize(int sizeIndex) {
    if (sizeIndex < 0 || sizeIndex >= 5) {
        return;
    }
    this->window.canvas()->toolState().eraserWidth = ERASER_SIZES[static_cast<std::size_t>(sizeIndex)];
    for (int i = 0; i < 5; ++i) {
        this->window.commandHost()->setCommandChecked(ERASER_SIZE_IDS[static_cast<std::size_t>(i)], i == sizeIndex);
    }
    this->window.toolPalette()->syncFromToolState(this->window.canvas()->toolState());
}

void QtAppShell::setEraserType(QtEraserMode mode) {
    this->window.canvas()->toolState().eraserMode = mode;
    this->window.commandHost()->setCommandChecked("eraser.type-standard", mode == QtEraserMode::Standard);
    this->window.commandHost()->setCommandChecked("eraser.type-whiteout", mode == QtEraserMode::Whiteout);
    this->window.commandHost()->setCommandChecked("eraser.type-delete-stroke", mode == QtEraserMode::DeleteStroke);
}

void QtAppShell::setHighlighterSize(int sizeIndex) {
    if (sizeIndex < 0 || sizeIndex >= 5) {
        return;
    }
    this->window.canvas()->toolState().highlighterWidth = HIGHLIGHTER_SIZES[static_cast<std::size_t>(sizeIndex)];
    for (int i = 0; i < 5; ++i) {
        this->window.commandHost()->setCommandChecked(HIGHLIGHTER_SIZE_IDS[static_cast<std::size_t>(i)], i == sizeIndex);
    }
    this->window.toolPalette()->syncFromToolState(this->window.canvas()->toolState());
}

void QtAppShell::addPageAtEnd() {
    if (!this->documentController.hasDocument()) {
        return;
    }
    const auto pageCount = this->documentController.pageCount();
    this->documentController.addPageAfter(pageCount > 0 ? pageCount - 1 : 0);
    this->window.canvas()->update();
    this->window.pageSidebar()->refresh();
    updateEditCommandStates();
    markSessionDirty();
    this->window.statusBar()->showMessage(QStringLiteral("Page added at end"), 3000);
}

void QtAppShell::appendNewPdfPages() {
    const int inserted = this->documentController.appendNewPdfPages();
    if (inserted < 0) {
        QMessageBox::information(&this->window, QStringLiteral("Append PDF Pages"),
                                 QStringLiteral("No PDF is attached to this document."));
        return;
    }
    if (inserted == 0) {
        this->window.statusBar()->showMessage(QStringLiteral("No new PDF pages to append"), 3000);
        return;
    }
    this->window.canvas()->update();
    this->window.pageSidebar()->refresh();
    syncFooterWidgets();
    updateEditCommandStates();
    markSessionDirty();
    this->window.statusBar()->showMessage(QStringLiteral("Appended %1 PDF page%2")
                                                  .arg(inserted)
                                                  .arg(inserted == 1 ? QString() : QStringLiteral("s")),
                                          3000);
}

void QtAppShell::deleteLayer() {
    if (!this->documentController.hasDocument()) {
        return;
    }
    const auto pageIndex = this->window.canvas()->currentPageIndex();
    const auto layerCount = this->documentController.layerCount(pageIndex);
    if (layerCount <= 1) {
        this->window.statusBar()->showMessage(QStringLiteral("Cannot delete the only layer"), 3000);
        return;
    }
    const auto layerIndex = this->documentController.selectedLayerIndex(pageIndex);
    this->documentController.removeLayer(pageIndex, layerIndex);
    this->window.canvas()->update();
    this->window.layerPanel()->refresh();
    markSessionDirty();
    this->window.statusBar()->showMessage(QStringLiteral("Layer deleted"), 3000);
}

void QtAppShell::moveSelectionLayerUp() {
    if (!this->documentController.moveSelectionToAdjacentLayer(+1)) {
        return;
    }
    this->window.canvas()->update();
    this->window.layerPanel()->refresh();
    this->window.pageSidebar()->refresh();
    updateEditCommandStates();
    markSessionDirty();
    this->window.statusBar()->showMessage(QStringLiteral("Moved selection up one layer"), 3000);
}

void QtAppShell::moveSelectionLayerDown() {
    if (!this->documentController.moveSelectionToAdjacentLayer(-1)) {
        return;
    }
    this->window.canvas()->update();
    this->window.layerPanel()->refresh();
    this->window.pageSidebar()->refresh();
    updateEditCommandStates();
    markSessionDirty();
    this->window.statusBar()->showMessage(QStringLiteral("Moved selection down one layer"), 3000);
}
