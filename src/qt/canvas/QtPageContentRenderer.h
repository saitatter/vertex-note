/*
 * VertexNote
 *
 * Shared page content renderer that dispatches drawables
 * through the backend-neutral render contracts.
 */

#pragma once

#include "view/render/Renderers.h"

namespace vn::view::render {

/**
 * Routes page rendering through the abstract renderer interfaces.
 *
 * Owns a set of concrete renderer implementations and provides a single
 * `drawPage` call that dispatches each drawable to the correct renderer.
 * This avoids duplicating the variant dispatch in every view widget.
 */
class PageContentRenderer {
public:
    PageContentRenderer(StrokeRenderer* stroke, TextRenderer* text, ImageRenderer* image,
                        BackgroundRenderer* background, GeometryRenderer* geometry);

    /**
     * Draw all content for a single page: background, then drawables in z-order.
     */
    void drawPage(const PageRenderSnapshot& page, const RenderRect& rect, RenderContext& context) const;

    /**
     * Draw a single stroke through the stroke renderer (e.g. for active stroke preview).
     */
    void drawStroke(const StrokeRenderModel& stroke, RenderContext& context) const;

private:
    StrokeRenderer* strokeRenderer = nullptr;
    TextRenderer* textRenderer = nullptr;
    ImageRenderer* imageRenderer = nullptr;
    BackgroundRenderer* backgroundRenderer = nullptr;
    GeometryRenderer* geometryRenderer = nullptr;
};

}  // namespace vn::view::render
