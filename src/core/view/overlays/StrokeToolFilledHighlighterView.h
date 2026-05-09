/*
 * VertexNote
 *
 * View active stroke tool -- for filled highlighter only
 *      In this case, the mask needs to be wiped at every iteration, and repainted to avoid artefacts like in
 *          https://github.com/saitatter/vertex-note/issues/3709
 *
 * @author VertexNote Team
 * https://github.com/saitatter/vertex-note
 *
 * @license GNU GPLv2 or later
 */
#pragma once

#include "StrokeToolFilledView.h"

namespace vn::view {
class StrokeToolFilledHighlighterView: public StrokeToolFilledView {
public:
    StrokeToolFilledHighlighterView(const StrokeHandler* strokeHandler, const Stroke& stroke, Repaintable* parent);
    virtual ~StrokeToolFilledHighlighterView() noexcept;

    void draw(cairo_t* cr) const override;
};
};  // namespace vn::view
