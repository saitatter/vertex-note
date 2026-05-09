/*
 * VertexNote
 *
 * Poppler GLib Implementation
 *
 * @author VertexNote Team
 * https://github.com/saitatter/vertex-note
 *
 * @license GNU GPLv2 or later
 */

#pragma once

#include <cstddef>  // for size_t
#include <string>   // for string

#include <glib.h>     // for GError, gpointer, gsize
#include <poppler.h>  // for PopplerDocument

#include "pdf/base/PdfDocumentInterface.h"  // for PdfDocumentInterface
#include "pdf/base/PdfPage.h"               // for PdfPagePtr

#include "filesystem.h"  // for path

class PdfBookmarkIterator;

class PopplerGlibDocument: public PdfDocumentInterface {
public:
    PopplerGlibDocument();
    PopplerGlibDocument(const PopplerGlibDocument& doc);
    ~PopplerGlibDocument() override;

public:
    void assign(PdfDocumentInterface* doc) override;
    bool equals(PdfDocumentInterface* doc) const override;

public:
    bool save(fs::path const& filepath, GError** error) const override;
    bool load(fs::path const& filepath, std::string password, GError** error) override;
    bool load(std::unique_ptr<std::string> data, std::string password, GError** error) override;
    bool isLoaded() const override;
    void reset() override;

    PdfPagePtr getPage(size_t page) const override;
    size_t getPageCount() const override;
    PdfBookmarkIterator* getContentsIter() const override;

private:
    PopplerDocument* document = nullptr;
};
