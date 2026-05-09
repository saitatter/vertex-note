/*
 * VertexNote
 *
 * Image view
 *
 * @author VertexNote Team
 * https://github.com/saitatter/vertex-note
 *
 * @license GNU GPLv2 or later
 */

#pragma once

#include <cairo.h>  // for cairo_t

#include "model/BackgroundImage.h"  // for BackgroundImage
#include "util/Util.h"              // for npos

#include "BaseElementView.h"  // for BaseElementView

class BackgroundSelectDialogBase;


class ImageElementView: public BaseElementView {
public:
    ImageElementView(size_t id, BackgroundSelectDialogBase* dlg);
    ~ImageElementView() override;

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

    /**
     * Will be called before getContentWidth() / getContentHeight(), can be overwritten
     */
    void calcSize() override;

private:
    double zoom = 1;

    BackgroundImage backgroundImage;
    int width = -1;
    int height = -1;

    friend class ImagesDialog;
};
