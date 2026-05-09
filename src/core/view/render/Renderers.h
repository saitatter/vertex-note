/*
 * VertexNote
 *
 * Backend-neutral interactive renderer interfaces.
 */

#pragma once

#include "RenderContext.h"

class Image;
class NotePage;
class Stroke;
class Text;

namespace vn::view {
class OverlayView;
}

namespace vn::view::render {

class StrokeRenderer {
public:
    virtual ~StrokeRenderer() = default;
    virtual void draw(const Stroke& stroke, RenderContext& context) const = 0;
};

class TextRenderer {
public:
    virtual ~TextRenderer() = default;
    virtual void draw(const Text& text, RenderContext& context) const = 0;
};

class ImageRenderer {
public:
    virtual ~ImageRenderer() = default;
    virtual void draw(const Image& image, RenderContext& context) const = 0;
};

class BackgroundRenderer {
public:
    virtual ~BackgroundRenderer() = default;
    virtual void draw(const NotePage& page, RenderContext& context) const = 0;
};

class OverlayRenderer {
public:
    virtual ~OverlayRenderer() = default;
    virtual void draw(const vn::view::OverlayView& overlay, RenderContext& context) const = 0;
};

}  // namespace vn::view::render
