/*
 * VertexNote
 *
 * Undo action for moving one vertex of an object-based geometry element.
 */

#include "GeometryVertexMoveUndoAction.h"

#include <utility>

#include "control/Control.h"
#include "model/Document.h"
#include "model/XojPage.h"
#include "util/i18n.h"
#include "vertexnote/geometry/GeometryElement.h"

GeometryVertexMoveUndoAction::GeometryVertexMoveUndoAction(PageRef page, vn::geom::GeometryElement* element,
                                                           vn::geom::VertexId vertex,
                                                           vn::geom::Vec2 oldPosition,
                                                           vn::geom::Vec2 newPosition):
        UndoAction("GeometryVertexMoveUndoAction"),
        element(element),
        vertex(vertex),
        oldPosition(oldPosition),
        newPosition(newPosition) {
    this->page = std::move(page);
}

auto GeometryVertexMoveUndoAction::undo(Control* control) -> bool {
    Document* doc = control->getDocument();
    doc->lock();
    const bool changed = apply(this->oldPosition);
    doc->unlock();
    if (changed) {
        this->page->fireElementChanged(this->element);
    }
    this->undone = true;
    return changed;
}

auto GeometryVertexMoveUndoAction::redo(Control* control) -> bool {
    Document* doc = control->getDocument();
    doc->lock();
    const bool changed = apply(this->newPosition);
    doc->unlock();
    if (changed) {
        this->page->fireElementChanged(this->element);
    }
    this->undone = false;
    return changed;
}

auto GeometryVertexMoveUndoAction::getPages() -> std::vector<PageRef> { return {this->page}; }

auto GeometryVertexMoveUndoAction::getText() -> std::string { return _("Move geometry vertex"); }

auto GeometryVertexMoveUndoAction::apply(vn::geom::Vec2 position) -> bool {
    if (!this->element || !this->element->setVertexPosition(this->vertex, position)) {
        return false;
    }

    return true;
}
