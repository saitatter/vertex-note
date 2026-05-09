/*
 * VertexNote
 *
 * PDF Document Container Interface
 *
 * @author VertexNote Team
 * https://github.com/saitatter/vertex-note
 *
 * @license GNU GPLv2 or later
 */

#pragma once

#include <cstddef>  // for size_t
#include <string>   // for string

#include <glib.h>  // for GError, gpointer, gsize

#include "PdfPage.h"  // for PdfPagePtr
#include "filesystem.h"  // for path

class PdfBookmarkIterator;

class PdfDocumentInterface {
public:
    PdfDocumentInterface();
    virtual ~PdfDocumentInterface();

public:
    virtual void assign(PdfDocumentInterface* doc) = 0;
    virtual bool equals(PdfDocumentInterface* doc) const = 0;

public:
    virtual bool save(fs::path const& file, GError** error) const = 0;
    virtual bool load(fs::path const& file, std::string password, GError** error) = 0;
    virtual bool load(std::unique_ptr<std::string> data, std::string password, GError** error) = 0;
    virtual bool isLoaded() const = 0;
    virtual void reset() = 0;

    virtual PdfPagePtr getPage(size_t page) const = 0;
    virtual size_t getPageCount() const = 0;
    virtual PdfBookmarkIterator* getContentsIter() const = 0;

private:
};
