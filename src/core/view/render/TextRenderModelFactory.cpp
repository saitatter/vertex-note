/*
 * VertexNote
 *
 * Shared helpers for building text render models from core document data.
 */

#include "TextRenderModelFactory.h"

#include "model/Element.h"
#include "model/Layer.h"
#include "model/NotePage.h"
#include "model/Text.h"

namespace vn::view::render {

auto TextRenderModelFactory::fromText(const Text& text) -> TextRenderModel {
    return {.content = text.getText(),
            .fontName = text.getFontName(),
            .fontSize = text.getFontSize(),
            .color = text.getColor(),
            .x = text.getX(),
            .y = text.getY(),
            .inEditing = text.isInEditing()};
}

auto TextRenderModelFactory::fromPage(ConstPageRef page) -> std::vector<TextRenderModel> {
    std::vector<TextRenderModel> texts;
    if (!page) {
        return texts;
    }

    for (const Layer* layer: page->getLayersView()) {
        if (!layer || !layer->isVisible()) {
            continue;
        }

        for (const Element* element: layer->getElementsView()) {
            if (!element || element->getType() != ELEMENT_TEXT) {
                continue;
            }

            const auto* text = dynamic_cast<const Text*>(element);
            if (!text || text->getText().empty()) {
                continue;
            }

            texts.push_back(fromText(*text));
        }
    }

    return texts;
}

}  // namespace vn::view::render
