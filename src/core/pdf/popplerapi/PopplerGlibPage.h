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

#include <string>  // for string
#include <vector>  // for vector

#include <cairo.h>    // for cairo_t, cairo_region_t
#include <poppler.h>  // for PopplerPage

#include "pdf/base/PdfPage.h"  // for PdfRectangle (ptr only), XojPdfP...


class PopplerGlibPage: public PdfPage {
public:
    PopplerGlibPage(PopplerPage* page, PopplerDocument* doc);
    PopplerGlibPage(const PopplerGlibPage& other);
    virtual ~PopplerGlibPage();
    PopplerGlibPage& operator=(const PopplerGlibPage& other);

public:
    double getWidth() const override;
    double getHeight() const override;

    void render(cairo_t* cr) const override;
    void renderForPrinting(cairo_t* cr) const override;

    std::vector<PdfRectangle> findText(const std::string& text) override;

    std::string selectText(const PdfRectangle& rect, PdfPageSelectionStyle style) override;

    cairo_region_t* selectTextRegion(const PdfRectangle& rect, PdfPageSelectionStyle style) override;

    TextSelection selectTextLines(const PdfRectangle& rect, PdfPageSelectionStyle style) override;

    auto getLinks() -> std::vector<Link> override;

    int getPageId() const override;

    std::string getPageLabel() const override;

private:
    PopplerPage* page;
    PopplerDocument* document;
};
