/*
 * VertexNote
 *
 * Cairo render context for the backend-neutral interactive render seam.
 */

#pragma once

#include <cairo.h>

#include "RenderContext.h"

namespace vn::view::render {

class CairoRenderContext: public RenderContext {
public:
    CairoRenderContext(cairo_t* cairo, double scaleFactor = 1.0): cairo(cairo), factor(scaleFactor) {}

    [[nodiscard]] auto backend() const -> RenderBackend override { return RenderBackend::Cairo; }
    [[nodiscard]] auto scaleFactor() const -> double override { return this->factor; }
    [[nodiscard]] auto native() const -> cairo_t* { return this->cairo; }

private:
    cairo_t* cairo = nullptr;
    double factor = 1.0;
};

}  // namespace vn::view::render
