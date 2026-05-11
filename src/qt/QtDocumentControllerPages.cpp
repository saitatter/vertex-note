/*
 * VertexNote
 *
 * Qt document controller page management helpers.
 */

#include "QtDocumentController.h"

#include <algorithm>
#include <cstddef>
#include <memory>

#include "model/Element.h"
#include "model/Layer.h"
#include "model/NotePage.h"

void QtDocumentController::addPageAfter(std::size_t afterPageIndex) {
    if (!this->document) {
        return;
    }
    this->document->lock();
    double width = 595.0;
    double height = 842.0;
    if (afterPageIndex < this->document->getPageCount()) {
        auto ref = this->document->getPage(afterPageIndex);
        if (ref) {
            width = ref->getWidth();
            height = ref->getHeight();
        }
    }
    auto newPage = std::make_shared<NotePage>(width, height);
    const auto insertPos = std::min(afterPageIndex + 1, this->document->getPageCount());
    this->document->insertPage(newPage, insertPos);
    this->document->unlock();
    rebuildPageSnapshots();
}

void QtDocumentController::duplicatePage(std::size_t pageIndex) {
    if (!this->document || pageIndex >= this->document->getPageCount()) {
        return;
    }
    this->document->lock();
    auto srcPage = this->document->getPage(pageIndex);
    if (!srcPage) {
        this->document->unlock();
        return;
    }
    auto newPage = std::make_shared<NotePage>(srcPage->getWidth(), srcPage->getHeight());
    newPage->setBackgroundType(srcPage->getBackgroundType());
    newPage->setBackgroundColor(srcPage->getBackgroundColor());

    for (auto* srcLayer: srcPage->getLayers()) {
        if (!srcLayer) {
            continue;
        }
        Layer* dstLayer = nullptr;
        if (newPage->getLayers().empty()) {
            this->document->unlock();
            return;
        }
        dstLayer = newPage->getLayers().back();

        for (const auto& elem: srcLayer->getElements()) {
            if (elem) {
                dstLayer->addElement(elem->clone());
            }
        }
    }

    this->document->insertPage(newPage, pageIndex + 1);
    this->document->unlock();
    rebuildPageSnapshots();
}

void QtDocumentController::deletePage(std::size_t pageIndex) {
    if (!this->document || pageIndex >= this->document->getPageCount()) {
        return;
    }
    if (this->document->getPageCount() <= 1) {
        return;
    }
    this->document->lock();
    this->document->deletePage(pageIndex);
    this->document->unlock();
    rebuildPageSnapshots();
}

void QtDocumentController::addPageBefore(std::size_t beforePageIndex) {
    if (!this->document) {
        return;
    }
    this->document->lock();
    double width = 595.0;
    double height = 842.0;
    if (beforePageIndex < this->document->getPageCount()) {
        auto ref = this->document->getPage(beforePageIndex);
        if (ref) {
            width = ref->getWidth();
            height = ref->getHeight();
        }
    }
    auto newPage = std::make_shared<NotePage>(width, height);
    const auto insertPos = std::min(beforePageIndex, this->document->getPageCount());
    this->document->insertPage(newPage, insertPos);
    this->document->unlock();
    rebuildPageSnapshots();
}

void QtDocumentController::movePageTowards(std::size_t pageIndex, int direction) {
    if (!this->document) {
        return;
    }
    const auto count = this->document->getPageCount();
    if (pageIndex >= count) {
        return;
    }
    const auto target = static_cast<std::ptrdiff_t>(pageIndex) + direction;
    if (target < 0 || static_cast<std::size_t>(target) >= count) {
        return;
    }
    this->document->lock();
    auto pageA = this->document->getPage(pageIndex);
    auto pageB = this->document->getPage(static_cast<std::size_t>(target));
    if (pageA && pageB) {
        this->document->deletePage(pageIndex);
        this->document->insertPage(pageA, static_cast<std::size_t>(target));
    }
    this->document->unlock();
    rebuildPageSnapshots();
}

auto QtDocumentController::appendNewPdfPages() -> int {
    if (!this->document || this->document->getPdfPageCount() == 0U) {
        return -1;
    }

    std::size_t currentPdfPageCount = 0U;
    for (std::size_t index = 0; index < this->document->getPageCount(); ++index) {
        auto page = this->document->getPage(index);
        if (page && page->getBackgroundType().isPdfPage()) {
            currentPdfPageCount = std::max(currentPdfPageCount, page->getPdfPageNr() + 1U);
        }
    }

    const std::size_t pdfPageCount = this->document->getPdfPageCount();
    if (currentPdfPageCount >= pdfPageCount) {
        return 0;
    }

    int inserted = 0;
    this->document->lock();
    for (std::size_t pdfIndex = currentPdfPageCount; pdfIndex < pdfPageCount; ++pdfIndex) {
        const auto pdfPage = this->document->getPdfPage(pdfIndex);
        if (!pdfPage) {
            continue;
        }
        auto page = std::make_shared<NotePage>(pdfPage->getWidth(), pdfPage->getHeight());
        page->setBackgroundPdfPageNr(pdfIndex);
        this->document->addPage(page);
        ++inserted;
    }
    this->document->unlock();

    if (inserted > 0) {
        clearInteractiveGeometryState();
        rebuildPageSnapshots();
    }
    return inserted;
}
