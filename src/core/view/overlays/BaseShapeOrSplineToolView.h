/*
 * VertexNote
 *
 * Base view for shapes or spline tools
 *
 * @author VertexNote Team
 * https://github.com/saitatter/vertex-note
 *
 * @license GNU GPLv2 or later
 */
#pragma once

#include <cairo.h>

#include "util/Range.h"
#include "view/Mask.h"

#include "BaseStrokeToolView.h"

class InputHandler;

namespace vn::view {
class Repaintable;

class BaseShapeOrSplineToolView: public BaseStrokeToolView {

public:
    BaseShapeOrSplineToolView(const InputHandler* handler, Repaintable* parent);
    ~BaseShapeOrSplineToolView() noexcept override;

protected:
    cairo_t* prepareContext(cairo_t* cr) const;

    void commitDrawing(cairo_t* cr) const;

    const double fillingAlpha;

    // The mask is only for filled highlighter strokes, to avoid artefacts as in
    // https://github.com/saitatter/vertex-note/issues/3709
    mutable Mask mask;
    const bool needMask;
    /// @brief The part of the mask that needs to be wiped to ensure the filling is correctly drawn.
    mutable Range maskWipeExtent;
};
};  // namespace vn::view
