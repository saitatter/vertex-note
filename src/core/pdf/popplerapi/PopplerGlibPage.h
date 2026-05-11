/*
 * VertexNote
 *
 * PDF Page GLib Implementation
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

#include <poppler.h>  // for PopplerPage
#include <poppler/cpp/poppler-document.h>

#include "pdf/base/PdfPage.h"  // for PdfRectangle (ptr only), XojPdfP...


class PopplerGlibPage: public PdfPage {
public:
    PopplerGlibPage(PopplerPage* page, PopplerDocument* doc);
    PopplerGlibPage(PopplerPage* page, PopplerDocument* doc, std::shared_ptr<poppler::document> renderDocument);
    PopplerGlibPage(const PopplerGlibPage& other);
    virtual ~PopplerGlibPage();
    PopplerGlibPage& operator=(const PopplerGlibPage& other);

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
    PopplerPage* page;
    PopplerDocument* document;
    std::shared_ptr<poppler::document> renderDocument;
};
