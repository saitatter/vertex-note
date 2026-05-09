/*
 * VertexNote
 *
 * Displays a pdf background
 *
 * @author VertexNote Team
 * https://github.com/saitatter/vertex-note
 *
 * @license GNU GPLv2 or later
 */

#pragma once

#include <cstddef>  // for size_t

#include <cairo.h>  // for cairo_t

#include "view/ViewNamespaceAliases.h"
#include "BackgroundView.h"  // for BackgroundView

class PdfCache;

namespace vn::view {

class PdfBackgroundView: public BackgroundView {
public:
    PdfBackgroundView(double pageWidth, double pageHeight, size_t pageNo, PdfCache* pdfCache = nullptr);
    virtual ~PdfBackgroundView() = default;

    /**
     * @brief Draws the background on the entire mask represented by the cairo context cr
     */
    void draw(cairo_t* cr) const override;

private:
    size_t pageNo;
    PdfCache* pdfCache = nullptr;
};

};  // namespace vn::view
