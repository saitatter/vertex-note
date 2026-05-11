/*
 * VertexNote
 *
 * PDF Page implementation
 *
 * @author VertexNote Team
 * https://github.com/saitatter/vertex-note
 *
 * @license GNU GPLv2 or later
 */

#pragma once

#include <memory>
#include <string>  // for string
#include <vector>  // for vector

#include <poppler/cpp/poppler-document.h>

#include "pdf/base/PdfPage.h"  // for PdfRectangle (ptr only), XojPdfP...


class PDFDoc;

class PopplerPdfPage: public PdfPage {
public:
    PopplerPdfPage(int pageIndex, std::shared_ptr<poppler::document> doc, std::shared_ptr<PDFDoc> linkDocument);
    PopplerPdfPage(const PopplerPdfPage& other);
    virtual ~PopplerPdfPage();
    PopplerPdfPage& operator=(const PopplerPdfPage& other);

public:
    double getWidth() const override;
    double getHeight() const override;

    vn::util::RasterImageData renderPreviewRaster(int pixelWidth, int pixelHeight, double pageWidth,
                                                  double pageHeight) const override;

    std::vector<PdfRectangle> findText(const std::string& text) override;

    std::string selectText(const PdfRectangle& rect, PdfPageSelectionStyle style) override;

    TextSelection selectTextLines(const PdfRectangle& rect, PdfPageSelectionStyle style) override;

    auto getLinks() -> std::vector<Link> override;

    int getPageId() const override;

    std::string getPageLabel() const override;

private:
    [[nodiscard]] auto createPage() const -> std::unique_ptr<poppler::page>;

private:
    int pageIndex = -1;
    std::shared_ptr<poppler::document> document;
    std::shared_ptr<PDFDoc> linkDocument;
};
