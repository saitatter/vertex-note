/*
 * VertexNote
 *
 * Qt painter render context for the backend-neutral interactive render seam.
 */

#pragma once

#include "RenderContext.h"

class QPainter;

namespace vn::view::render {

class QtPainterRenderContext: public RenderContext {
public:
    QtPainterRenderContext(QPainter* painter, double scaleFactor = 1.0): painter(painter), factor(scaleFactor) {}

    [[nodiscard]] auto backend() const -> RenderBackend override { return RenderBackend::QtPainter; }
    [[nodiscard]] auto scaleFactor() const -> double override { return this->factor; }
    [[nodiscard]] auto native() const -> QPainter* { return this->painter; }

private:
    QPainter* painter = nullptr;
    double factor = 1.0;
};

}  // namespace vn::view::render
