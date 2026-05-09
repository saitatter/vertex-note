/*
 * VertexNote
 *
 * Backend-neutral interactive renderer interfaces.
 */

#pragma once

#include <cstddef>
#include <string>

#include "model/PageType.h"
#include "RenderContext.h"

class Image;
class Stroke;
class Text;

namespace vn::view {
class OverlayView;
}

namespace vn::view::render {

struct RenderRect {
    double x = 0.0;
    double y = 0.0;
    double width = 0.0;
    double height = 0.0;
};

struct PageBackgroundRenderModel {
    PageTypeFormat backgroundFormat = PageTypeFormat::Plain;
    bool annotated = false;
    bool hasBackgroundName = false;
    std::string backgroundName;
    std::size_t layerCount = 0;
    std::size_t pdfPageNumber = 0;
};

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
    virtual void draw(const PageBackgroundRenderModel& page, const RenderRect& rect, RenderContext& context) const = 0;
};

class OverlayRenderer {
public:
    virtual ~OverlayRenderer() = default;
    virtual void draw(const vn::view::OverlayView& overlay, RenderContext& context) const = 0;
};

}  // namespace vn::view::render
