/*
 * VertexNote
 *
 * PDF view
 *
 * @author VertexNote Team
 * https://github.com/saitatter/vertex-note
 *
 * @license GNU GPLv2 or later
 */

#pragma once

#include <cairo.h>  // for cairo_t

#include "pdf/base/PdfPage.h"  // for PdfPagePtr

#include "BaseElementView.h"  // for BaseElementView

class PdfPagesDialog;

class PdfElementView: public BaseElementView {
public:
    PdfElementView(size_t id, PdfPagePtr page, PdfPagesDialog* dlg);
    ~PdfElementView() override;

protected:
    /**
     * Paint the contents (without border / selection)
     */
    void paintContents(cairo_t* cr) override;

    /**
     * Get the width in pixel, without shadow / border
     */
    int getContentWidth() override;

    /**
     * Get the height in pixel, without shadow / border
     */
    int getContentHeight() override;

public:
    bool isUsed() const;
    void setUsed(bool used);
    void setHideIfUsed(bool hideIfUsed);

private:
    PdfPagePtr page;

    /**
     * This page is already used as background
     */
    bool used = false;
};
