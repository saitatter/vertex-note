/*
 * VertexNote
 *
 * Shared helpers for building geometry render models from core document data.
 */

#include "GeometryRenderModelFactory.h"

#include "model/Point.h"
#include "vertexnote/geometry/GeometryElement.h"

namespace vn::view::render {

auto GeometryRenderModelFactory::fromGeometryElement(const vn::geom::GeometryElement& geometry) -> GeometryRenderModel {
    GeometryRenderModel model;
    model.color = geometry.getColor();
    model.strokeWidth = geometry.getStrokeWidth();

    const auto& object = geometry.geometry();
    model.edges.reserve(object.edges().size());
    for (const auto& edge: object.edges()) {
        const auto* start = object.vertex(edge.start);
        const auto* end = object.vertex(edge.end);
        if (!start || !end) {
            continue;
        }

        GeometryEdgeRenderModel renderEdge;
        renderEdge.kind = edge.kind;
        renderEdge.start = Point(start->position.x, start->position.y);
        renderEdge.end = Point(end->position.x, end->position.y);
        renderEdge.closedLoop = edge.start == edge.end;
        renderEdge.controls.reserve(edge.controls.size());
        for (const auto controlId: edge.controls) {
            const auto* control = object.vertex(controlId);
            if (!control) {
                continue;
            }
            renderEdge.controls.emplace_back(control->position.x, control->position.y);
        }

        model.edges.push_back(std::move(renderEdge));
    }

    return model;
}

}  // namespace vn::view::render
