/*
 * VertexNote
 *
 * Handles text search on a PDF page and in Xournal Texts
 *
 * @author VertexNote Team
 * https://github.com/saitatter/vertex-note
 *
 * @license GNU GPLv2 or later
 */

#pragma once

#include <string>  // for string
#include <vector>  // for vector

#include "model/OverlayBase.h"
#include "model/PageRef.h"        // for PageRef
#include "model/Point.h"          // for Point
#include "pdf/base/PdfPage.h"  // for PdfPagePtr, PdfRectangle
#include "util/DispatchPool.h"

namespace xoj::view {
class OverlayView;
class Repaintable;
class SearchResultView;
};  // namespace xoj::view

class SearchControl: public OverlayBase {
public:
    SearchControl(const PageRef& page, PdfPagePtr pdf);
    virtual ~SearchControl();

    bool search(const std::string& text, size_t index, size_t* occurrences, PdfRectangle* UpperMostMatch);

    const std::vector<PdfRectangle>& getResults() const { return results; }

    const PdfRectangle* getHighlightRect() const { return highlightRect; }

    const std::shared_ptr<xoj::util::DispatchPool<xoj::view::SearchResultView>>& getViewPool() const {
        return viewPool;
    }

private:
    PageRef page;
    PdfPagePtr pdf;
    std::string currentText;
    PdfRectangle* highlightRect = nullptr;

    std::vector<PdfRectangle> results;
    std::shared_ptr<xoj::util::DispatchPool<xoj::view::SearchResultView>> viewPool;
};
