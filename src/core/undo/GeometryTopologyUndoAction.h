/*
 * VertexNote
 *
 * Undo action for topology edits on an object-based geometry element.
 */

#pragma once

#include "UndoAction.h"
#include "vertexnote/geometry/GeometryObject.h"

namespace vn::geom {
class GeometryElement;
}

class GeometryTopologyUndoAction final: public UndoAction {
public:
    GeometryTopologyUndoAction(PageRef page, vn::geom::GeometryElement* element, vn::geom::GeometryObject before,
                               vn::geom::GeometryObject after, std::string text);

    auto undo(Control* control) -> bool override;
    auto redo(Control* control) -> bool override;
    auto getText() -> std::string override;
    auto getPages() -> std::vector<PageRef> override;

private:
    auto apply(const vn::geom::GeometryObject& state) -> bool;

private:
    vn::geom::GeometryElement* element = nullptr;
    vn::geom::GeometryObject before;
    vn::geom::GeometryObject after;
    std::string text;
};
