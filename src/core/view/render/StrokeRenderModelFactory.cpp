/*
 * VertexNote
 *
 * Shared helpers for building stroke render models from core document data.
 */

#include "StrokeRenderModelFactory.h"

#include <vector>

#include "model/Element.h"
#include "model/Layer.h"
#include "model/NotePage.h"
#include "model/Stroke.h"

namespace vn::view::render {

auto StrokeRenderModelFactory::fromStroke(const Stroke& stroke) -> StrokeRenderModel {
    return {.points = stroke.getPointVector(),
            .color = stroke.getColor(),
            .width = stroke.getWidth(),
            .dashPattern = stroke.getLineStyle().getDashes(),
            .fill = stroke.getFill(),
            .highlighter = stroke.getToolType() == StrokeTool::HIGHLIGHTER,
            .pressureSensitive = stroke.hasPressure() && stroke.getToolType() != StrokeTool::HIGHLIGHTER,
            .capStyle = static_cast<int>(stroke.getStrokeCapStyle())};
}

auto StrokeRenderModelFactory::fromPage(ConstPageRef page) -> std::vector<StrokeRenderModel> {
    std::vector<StrokeRenderModel> strokes;
    if (!page) {
        return strokes;
    }

    for (const Layer* layer: page->getLayersView()) {
        if (!layer || !layer->isVisible()) {
            continue;
        }

        for (const Element* element: layer->getElementsView()) {
            if (!element || element->getType() != ELEMENT_STROKE) {
                continue;
            }

            const auto* stroke = dynamic_cast<const Stroke*>(element);
            if (!stroke || stroke->getPointCount() < 2) {
                continue;
            }

            strokes.push_back(fromStroke(*stroke));
        }
    }

    return strokes;
}

}  // namespace vn::view::render
