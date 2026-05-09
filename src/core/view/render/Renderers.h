/*
 * VertexNote
 *
 * Backend-neutral interactive renderer interfaces.
 */

#pragma once

#include <cstddef>
#include <string>
#include <variant>
#include <vector>

#include "model/Point.h"
#include "model/PageType.h"
#include "util/Color.h"
#include "RenderContext.h"

class Image;
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
    std::string rasterContentPng;
};

struct StrokeRenderModel {
    std::vector<Point> points;
    Color color{};
    double width = 0.0;
    std::vector<double> dashPattern;
    int fill = -1;
    bool highlighter = false;
    bool pressureSensitive = false;
    int capStyle = 0;
};

struct TextRenderModel {
    std::string content;
    std::string fontName;
    double fontSize = 0.0;
    Color color{};
    double x = 0.0;
    double y = 0.0;
    bool inEditing = false;
};

struct ImageRenderModel {
    std::string encodedBytes;
    double x = 0.0;
    double y = 0.0;
    double width = 0.0;
    double height = 0.0;
};

using PageDrawableRenderModel = std::variant<StrokeRenderModel, TextRenderModel, ImageRenderModel>;

class StrokeRenderer {
public:
    virtual ~StrokeRenderer() = default;
    virtual void draw(const StrokeRenderModel& stroke, RenderContext& context) const = 0;
};

class TextRenderer {
public:
    virtual ~TextRenderer() = default;
    virtual void draw(const TextRenderModel& text, RenderContext& context) const = 0;
};

class ImageRenderer {
public:
    virtual ~ImageRenderer() = default;
    virtual void draw(const ImageRenderModel& image, RenderContext& context) const = 0;
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
