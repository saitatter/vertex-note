/*
 * VertexNote
 *
 * Undo action for moving one vertex of an object-based geometry element.
 */

#pragma once

#include <string>
#include <vector>

#include "model/PageRef.h"
#include "undo/UndoAction.h"
#include "vertexnote/geometry/GeometryTypes.h"

namespace vn::geom {
class GeometryElement;
}

class GeometryVertexMoveUndoAction final: public UndoAction {
public:
    GeometryVertexMoveUndoAction(PageRef page, vn::geom::GeometryElement* element, vn::geom::VertexId vertex,
                                 vn::geom::Vec2 oldPosition, vn::geom::Vec2 newPosition);

    bool undo(Control* control) override;
    bool redo(Control* control) override;
    std::vector<PageRef> getPages() override;
    std::string getText() override;

private:
    bool apply(vn::geom::Vec2 position);

private:
    vn::geom::GeometryElement* element = nullptr;
    vn::geom::VertexId vertex = vn::geom::InvalidVertexId;
    vn::geom::Vec2 oldPosition;
    vn::geom::Vec2 newPosition;
};
