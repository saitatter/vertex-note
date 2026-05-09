/*
 * VertexNote
 *
 * Shared page content renderer implementation.
 */

#include "QtPageContentRenderer.h"

#include <type_traits>
#include <variant>

namespace vn::view::render {

PageContentRenderer::PageContentRenderer(StrokeRenderer* stroke, TextRenderer* text, ImageRenderer* image,
                                         BackgroundRenderer* background, GeometryRenderer* geometry):
        strokeRenderer(stroke), textRenderer(text), imageRenderer(image), backgroundRenderer(background),
        geometryRenderer(geometry) {}

void PageContentRenderer::drawPage(const PageRenderSnapshot& page, const RenderRect& rect,
                                   RenderContext& context) const {
    if (this->backgroundRenderer) {
        this->backgroundRenderer->draw(page.background, rect, context);
    }

    for (const auto& drawable: page.drawables) {
        std::visit(
                [this, &context](const auto& model) {
                    using Model = std::decay_t<decltype(model)>;
                    if constexpr (std::is_same_v<Model, StrokeRenderModel>) {
                        if (this->strokeRenderer) {
                            this->strokeRenderer->draw(model, context);
                        }
                    } else if constexpr (std::is_same_v<Model, TextRenderModel>) {
                        if (this->textRenderer) {
                            this->textRenderer->draw(model, context);
                        }
                    } else if constexpr (std::is_same_v<Model, ImageRenderModel>) {
                        if (this->imageRenderer) {
                            this->imageRenderer->draw(model, context);
                        }
                    } else if constexpr (std::is_same_v<Model, GeometryRenderModel>) {
                        if (this->geometryRenderer) {
                            this->geometryRenderer->draw(model, context);
                        }
                    }
                },
                drawable);
    }
}

void PageContentRenderer::drawStroke(const StrokeRenderModel& stroke, RenderContext& context) const {
    if (this->strokeRenderer) {
        this->strokeRenderer->draw(stroke, context);
    }
}

}  // namespace vn::view::render
