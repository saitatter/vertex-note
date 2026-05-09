/*
 * VertexNote
 *
 * Displays a Image Element
 *
 * @author VertexNote Team
 * https://github.com/saitatter/vertex-note
 *
 * @license GNU GPLv2 or later
 */

#pragma once

#include "View.h"

class Image;

class vn::view::ImageView: public vn::view::ElementView {
public:
    ImageView(const Image* image);
    virtual ~ImageView();

    /**
     * Draws an Image element
     */
    void draw(const Context& ctx) const override;

private:
    const Image* image;
};
