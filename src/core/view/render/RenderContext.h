/*
 * VertexNote
 *
 * Backend-neutral interactive render context interfaces.
 */

#pragma once

namespace vn::view::render {

enum class RenderBackend { QtPainter };

class RenderContext {
public:
    virtual ~RenderContext() = default;

    [[nodiscard]] virtual auto backend() const -> RenderBackend = 0;
    [[nodiscard]] virtual auto scaleFactor() const -> double = 0;
};

}  // namespace vn::view::render
