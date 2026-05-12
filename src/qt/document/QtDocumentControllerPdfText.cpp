/*
 * VertexNote
 *
 * Qt document controller PDF text selection and marker helpers.
 */

#include "QtDocumentController.h"

#include <algorithm>
#include <cmath>
#include <utility>
#include <vector>

#include "model/Layer.h"
#include "model/NotePage.h"
#include "model/Point.h"
#include "model/Stroke.h"

auto QtDocumentController::beginPdfTextSelection(std::size_t pageIndex, double x, double y,
                                                 PdfPageSelectionStyle style) -> bool {
    if (!this->document || pageIndex >= this->document->getPageCount()) {
        return false;
    }

    this->document->lock_shared();
    auto page = this->document->getPage(pageIndex);
    const bool hasPdf = page && page->getPdfPageNr() != npos && this->document->getPdfPage(page->getPdfPageNr());
    this->document->unlock_shared();
    if (!hasPdf) {
        return false;
    }

    this->activePdfTextSelection = QtPdfTextSelectionState{.pageIndex = pageIndex,
                                                           .style = style,
                                                           .bounds = PdfRectangle(x, y, x, y),
                                                           .previewRects = {},
                                                           .selectedText = {},
                                                           .finalized = false};
    return true;
}

auto QtDocumentController::updatePdfTextSelection(double x, double y) -> bool {
    if (!this->document || !this->activePdfTextSelection) {
        return false;
    }

    auto& selection = *this->activePdfTextSelection;
    this->document->lock_shared();
    auto page =
            selection.pageIndex < this->document->getPageCount() ? this->document->getPage(selection.pageIndex) : nullptr;
    auto pdfPage = page && page->getPdfPageNr() != npos ? this->document->getPdfPage(page->getPdfPageNr()) : nullptr;
    selection.bounds.x2 = x;
    selection.bounds.y2 = y;
    selection.previewRects.clear();
    if (pdfPage) {
        auto preview = pdfPage->selectTextLines(selection.bounds, selection.style);
        selection.previewRects = std::move(preview.rects);
    }
    this->document->unlock_shared();
    return true;
}

auto QtDocumentController::finalizePdfTextSelection() -> std::string {
    if (!this->document || !this->activePdfTextSelection) {
        return {};
    }

    auto selection = *this->activePdfTextSelection;
    this->document->lock_shared();
    auto page =
            selection.pageIndex < this->document->getPageCount() ? this->document->getPage(selection.pageIndex) : nullptr;
    auto pdfPage = page && page->getPdfPageNr() != npos ? this->document->getPdfPage(page->getPdfPageNr()) : nullptr;
    if (pdfPage) {
        auto finalized = pdfPage->selectTextLines(selection.bounds, selection.style);
        selection.previewRects = std::move(finalized.rects);
        selection.selectedText = pdfPage->selectText(selection.bounds, selection.style);
        selection.finalized = true;
    }
    this->document->unlock_shared();
    this->activePdfTextSelection = std::move(selection);
    return this->activePdfTextSelection->selectedText;
}

void QtDocumentController::cancelPdfTextSelection() { this->activePdfTextSelection.reset(); }

auto QtDocumentController::pdfTextSelection() const -> const std::optional<QtPdfTextSelectionState>& {
    return this->activePdfTextSelection;
}

auto QtDocumentController::createPdfTextMarkerStrokes(std::size_t pageIndex, const std::vector<PdfRectangle>& rects,
                                                      QtPdfTextMarkerKind kind, int opacity, Color color) -> int {
    if (!this->document || pageIndex >= this->document->getPageCount() || rects.empty()) {
        return 0;
    }

    const int markerOpacity = std::clamp(opacity, 0, 255);
    std::vector<ElementPtr> strokes;
    strokes.reserve(rects.size());
    for (const auto& rect: rects) {
        const double middleOfLine = (rect.y1 + rect.y2) / 2.0;
        const double bottomOfLine = std::max(rect.y1, rect.y2);
        const double textHeight = std::max(1.0, std::abs(rect.y2 - rect.y1));
        const double y = kind == QtPdfTextMarkerKind::Underline ? bottomOfLine : middleOfLine;
        const double width = kind == QtPdfTextMarkerKind::Highlight ? textHeight : 1.0;

        auto stroke = std::make_unique<Stroke>();
        stroke->setColor(color);
        stroke->setFill(markerOpacity);
        stroke->setToolType(StrokeTool::HIGHLIGHTER);
        stroke->setWidth(width);
        stroke->setStrokeCapStyle(StrokeCapStyle::BUTT);
        stroke->addPoint(Point(rect.x1, y, Point::NO_PRESSURE));
        stroke->addPoint(Point(rect.x2, y, Point::NO_PRESSURE));
        strokes.push_back(std::move(stroke));
    }

    this->document->lock();
    auto page = this->document->getPage(pageIndex);
    auto* layer = page ? page->getSelectedLayer() : nullptr;
    if (!layer) {
        this->document->unlock();
        return 0;
    }

    QtInsertElementsHistoryEntry entry{.pageIndex = pageIndex,
                                       .elements = {},
                                       .ownedElements = {},
                                       .text = "Mark PDF text"};
    entry.elements.reserve(strokes.size());
    for (auto& stroke: strokes) {
        entry.elements.push_back(stroke.get());
        layer->addElement(std::move(stroke));
    }
    const int inserted = static_cast<int>(entry.elements.size());
    if (inserted > 0) {
        pushHistory(QtHistoryEntry{std::move(entry)});
    }
    this->document->unlock();

    if (inserted > 0) {
        rebuildPageSnapshots();
    }
    return inserted;
}

auto QtDocumentController::createPdfTextMarkerStrokesForSelection(QtPdfTextMarkerKind kind, int opacity, Color color)
        -> int {
    if (!this->activePdfTextSelection || this->activePdfTextSelection->previewRects.empty()) {
        return 0;
    }

    const auto pageIndex = this->activePdfTextSelection->pageIndex;
    const auto rects = this->activePdfTextSelection->previewRects;
    return createPdfTextMarkerStrokes(pageIndex, rects, kind, opacity, color);
}
