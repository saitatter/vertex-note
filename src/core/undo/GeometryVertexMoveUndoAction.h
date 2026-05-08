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
    GeometryVertexMoveUndoAction(PageRef page, vn::geom::GeometryElement* element,
                                 std::vector<vn::geom::VertexId> vertices,
                                 std::vector<vn::geom::Vec2> oldPositions,
                                 std::vector<vn::geom::Vec2> newPositions);

    bool undo(Control* control) override;
    bool redo(Control* control) override;
    std::vector<PageRef> getPages() override;
    std::string getText() override;

private:
    bool apply(const std::vector<vn::geom::Vec2>& positions);

private:
    PageRef page;
    vn::geom::GeometryElement* element = nullptr;
    std::vector<vn::geom::VertexId> vertices;
    std::vector<vn::geom::Vec2> oldPositions;
    std::vector<vn::geom::Vec2> newPositions;
};
