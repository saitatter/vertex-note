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

#include <poppler.h>  // for PopplerDocument, Popple...

#include "pdf/base/PdfBookmarkIterator.h"  // for PdfBookmarkIterator

class PdfAction;


class PopplerGlibPageBookmarkIterator: public PdfBookmarkIterator {
public:
    PopplerGlibPageBookmarkIterator(PopplerIndexIter* iter, PopplerDocument* document);
    ~PopplerGlibPageBookmarkIterator() override;

public:
    bool next() override;
    bool isOpen() override;
    PdfBookmarkIterator* getChildIter() override;
    PdfAction* getAction() override;

private:
    PopplerIndexIter* iter;
    PopplerDocument* document;
};
