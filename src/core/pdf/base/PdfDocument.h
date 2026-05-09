/*
 * VertexNote
 *
 * PDF Document Container
 *
 * @author VertexNote Team
 * https://github.com/saitatter/vertex-note
 *
 * @license GNU GPLv2 or later
 */

#pragma once

#include <cstddef>  // for size_t
#include <string>   // for string
#include <optional>
#include <string>
#include <vector>

#include <glib.h>  // for GError, gpointer, gsize

#include "PdfDocumentInterface.h"  // for PdfDocumentInterface
#include "PdfPage.h"               // for PdfPagePtr
#include "filesystem.h"               // for path

class PdfBookmarkIterator;


class PdfDocument: PdfDocumentInterface {
public:
    PdfDocument();
    PdfDocument(const PdfDocument& doc);
    ~PdfDocument() override;

public:
    PdfDocument& operator=(const PdfDocument& doc);
    bool operator==(PdfDocument& doc) const;
    void assign(PdfDocumentInterface* doc) override;
    bool equals(PdfDocumentInterface* doc) const override;

public:
    bool save(fs::path const& file, GError** error) const override;
    bool load(fs::path const& file, std::string password, GError** error) override;
    bool load(std::unique_ptr<std::string> data, std::string password, GError** error) override;
    bool isLoaded() const override;
    void reset() override;

    PdfPagePtr getPage(size_t page) const override;
    size_t getPageCount() const override;
    PdfBookmarkIterator* getContentsIter() const override;

private:
    PdfDocumentInterface* doc;
};
