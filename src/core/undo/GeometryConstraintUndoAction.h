/*
 * VertexNote
 *
 * Undo action for creating, editing, or deleting a geometry constraint.
 */

#pragma once

#include <optional>
#include <string>
#include <vector>

#include "model/PageRef.h"
#include "undo/UndoAction.h"
#include "vertexnote/geometry/GeometryTypes.h"

namespace vn::geom {
class GeometryElement;
}

class GeometryConstraintUndoAction final: public UndoAction {
public:
    GeometryConstraintUndoAction(PageRef page, vn::geom::GeometryElement* element,
                                 std::optional<vn::geom::Constraint> before,
                                 std::optional<vn::geom::Constraint> after);

    bool undo(Control* control) override;
    bool redo(Control* control) override;
    std::vector<PageRef> getPages() override;
    std::string getText() override;

private:
    bool apply(const std::optional<vn::geom::Constraint>& state);

private:
    PageRef page;
    vn::geom::GeometryElement* element = nullptr;
    std::optional<vn::geom::Constraint> before;
    std::optional<vn::geom::Constraint> after;
};
