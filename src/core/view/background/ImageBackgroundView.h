/*
 * VertexNote
 *
 * Displays a background image
 *
 * @author VertexNote Team
 * https://github.com/saitatter/vertex-note
 *
 * @license GNU GPLv2 or later
 */

#pragma once

#include <cairo.h>  // for cairo_t

#include "BackgroundView.h"  // for BackgroundView

struct BackgroundImage;

namespace xoj::view {
class ImageBackgroundView: public BackgroundView {
public:
    ImageBackgroundView(double pageWidth, double pageHeight, const BackgroundImage& image);

    /**
     * @brief Draws the background on the entire mask represented by the cairo context ctx.cr
     */
    virtual void draw(cairo_t* cr) const override;

private:
    const BackgroundImage& image;
};
};  // namespace vn::view
