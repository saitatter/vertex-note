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


class PdfAction;

class PdfBookmarkIterator {
public:
    PdfBookmarkIterator();
    virtual ~PdfBookmarkIterator();

public:
    virtual bool next() = 0;
    virtual bool isOpen() = 0;
    virtual PdfBookmarkIterator* getChildIter() = 0;
    virtual PdfAction* getAction() = 0;

private:
};
