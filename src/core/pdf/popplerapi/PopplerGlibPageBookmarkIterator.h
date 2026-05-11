/*
 * VertexNote
 *
 * PDF Bookmark iterator interface
 *
 * @author VertexNote Team
 * https://github.com/saitatter/vertex-note
 *
 * @license GNU GPLv2 or later
 */

#pragma once

#include <cstddef>
#include <memory>
#include <vector>

#include "pdf/base/PdfBookmarkIterator.h"  // for PdfBookmarkIterator

class OutlineItem;
class PDFDoc;
class PdfAction;


class PopplerGlibPageBookmarkIterator: public PdfBookmarkIterator {
public:
    PopplerGlibPageBookmarkIterator(const std::vector<OutlineItem*>* items, std::shared_ptr<PDFDoc> document);
    ~PopplerGlibPageBookmarkIterator() override;

public:
    bool next() override;
    bool isOpen() override;
    PdfBookmarkIterator* getChildIter() override;
    PdfAction* getAction() override;

private:
    [[nodiscard]] auto current() const -> OutlineItem*;

private:
    const std::vector<OutlineItem*>* items = nullptr;
    std::shared_ptr<PDFDoc> document;
    std::size_t index = 0;
};
