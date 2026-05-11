/*
 * VertexNote
 *
 * Qt document controller page background helpers.
 */

#include "QtDocumentController.h"

#include <cmath>
#include <cstddef>
#include <string>
#include <utility>

#include "model/Document.h"
#include "model/NotePage.h"

void QtDocumentController::setPageBackgroundColor(std::size_t pageIndex, Color color) {
    if (!this->document || pageIndex >= this->document->getPageCount()) {
        return;
    }
    this->document->lock();
    auto page = this->document->getPage(pageIndex);
    if (page) {
        page->setBackgroundColor(color);
    }
    this->document->unlock();
    rebuildPageSnapshots();
}

void QtDocumentController::setPageBackgroundType(std::size_t pageIndex, PageTypeFormat format) {
    if (!this->document || pageIndex >= this->document->getPageCount()) {
        return;
    }
    this->document->lock();
    auto page = this->document->getPage(pageIndex);
    if (page) {
        PageType type = page->getBackgroundType();
        type.format = format;
        page->setBackgroundType(type);
    }
    this->document->unlock();
    rebuildPageSnapshots();
}

void QtDocumentController::setPageBackgroundName(std::size_t pageIndex, const std::string& name) {
    if (!this->document || pageIndex >= this->document->getPageCount()) {
        return;
    }
    this->document->lock();
    auto page = this->document->getPage(pageIndex);
    if (page) {
        page->setBackgroundName(name);
    }
    this->document->unlock();
    rebuildPageSnapshots();
}

auto QtDocumentController::changePagePdfBackground(std::size_t pageIndex, ptrdiff_t pageNumber, bool relative,
                                                   std::string* errorMessage) -> bool {
    const auto setError = [errorMessage](std::string message) {
        if (errorMessage) {
            *errorMessage = std::move(message);
        }
    };

    if (!this->document || pageIndex >= this->document->getPageCount()) {
        setError("No active page");
        return false;
    }
    if (this->document->getPdfPageCount() == 0U) {
        setError("The current document has no PDF background");
        return false;
    }

    this->document->lock();
    auto page = this->document->getPage(pageIndex);
    if (!page) {
        this->document->unlock();
        setError("No active page");
        return false;
    }

    ptrdiff_t selected = pageNumber - 1;
    if (relative) {
        if (!page->getBackgroundType().isPdfPage()) {
            this->document->unlock();
            setError("Current page has no PDF background");
            return false;
        }
        selected = static_cast<ptrdiff_t>(page->getPdfPageNr()) + pageNumber;
    }

    if (selected < 0 || static_cast<std::size_t>(selected) >= this->document->getPdfPageCount()) {
        this->document->unlock();
        setError("PDF page number does not exist");
        return false;
    }

    const auto pdfPageIndex = static_cast<std::size_t>(selected);
    auto pdfPage = this->document->getPdfPage(pdfPageIndex);
    if (!pdfPage) {
        this->document->unlock();
        setError("PDF page could not be loaded");
        return false;
    }

    page->setBackgroundPdfPageNr(pdfPageIndex);
    Document::setPageSize(page, pdfPage->getWidth(), pdfPage->getHeight());
    this->document->unlock();
    rebuildPageSnapshots();
    return true;
}

auto QtDocumentController::canResizePage(std::size_t pageIndex) const -> bool {
    if (!this->document || pageIndex >= this->document->getPageCount()) {
        return false;
    }
    auto page = this->document->getPage(pageIndex);
    return page && !page->getBackgroundType().isPdfPage();
}

auto QtDocumentController::resizePage(std::size_t pageIndex, double width, double height) -> bool {
    if (!canResizePage(pageIndex) || width <= 0.0 || height <= 0.0) {
        return false;
    }

    this->document->lock();
    auto page = this->document->getPage(pageIndex);
    if (!page) {
        this->document->unlock();
        return false;
    }

    const double beforeWidth = page->getWidth();
    const double beforeHeight = page->getHeight();
    if (std::abs(beforeWidth - width) < 0.01 && std::abs(beforeHeight - height) < 0.01) {
        this->document->unlock();
        return false;
    }

    Document::setPageSize(page, width, height);
    this->document->unlock();

    pushHistory(QtHistoryEntry{QtPageSizeHistoryEntry{.pageIndex = pageIndex,
                                                      .beforeWidth = beforeWidth,
                                                      .beforeHeight = beforeHeight,
                                                      .afterWidth = width,
                                                      .afterHeight = height,
                                                      .text = "Change page size"}});
    rebuildPageSnapshots();
    return true;
}
