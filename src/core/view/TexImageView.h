/*
 * VertexNote
 *
 * Displays a TexImage Element
 *
 * @author VertexNote Team
 * https://github.com/saitatter/vertex-note
 *
 * @license GNU GPLv2 or later
 */

#pragma once

#include "View.h"

class TexImage;

class vn::view::TexImageView: public vn::view::ElementView {
public:
    TexImageView(const TexImage* texImage);
    virtual ~TexImageView();

    /**
     * Draws a TexImage model
     */
    void draw(const Context& ctx) const override;

private:
    const TexImage* texImage;
};
