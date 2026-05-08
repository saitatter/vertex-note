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
        vertices({vertex}),
        oldPositions({oldPosition}),
        newPositions({newPosition}) {
    this->page = std::move(page);
}

GeometryVertexMoveUndoAction::GeometryVertexMoveUndoAction(PageRef page, vn::geom::GeometryElement* element,
                                                           std::vector<vn::geom::VertexId> vertices,
                                                           std::vector<vn::geom::Vec2> oldPositions,
                                                           std::vector<vn::geom::Vec2> newPositions):
        UndoAction("GeometryVertexMoveUndoAction"),
        page(std::move(page)),
        element(element),
        vertices(std::move(vertices)),
        oldPositions(std::move(oldPositions)),
        newPositions(std::move(newPositions)) {}

auto GeometryVertexMoveUndoAction::undo(Control* control) -> bool {
    Document* doc = control->getDocument();
    doc->lock();
    const bool changed = apply(this->oldPositions);
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
    const bool changed = apply(this->newPositions);
    doc->unlock();
    if (changed) {
        this->page->fireElementChanged(this->element);
    }
    this->undone = false;
    return changed;
}

auto GeometryVertexMoveUndoAction::getPages() -> std::vector<PageRef> { return {this->page}; }

auto GeometryVertexMoveUndoAction::getText() -> std::string {
    return this->vertices.size() > 1U ? _("Move geometry vertices") : _("Move geometry vertex");
}

auto GeometryVertexMoveUndoAction::apply(const std::vector<vn::geom::Vec2>& positions) -> bool {
    if (!this->element || this->vertices.size() != positions.size()) {
        return false;
    }

    bool changed = false;
    for (std::size_t i = 0; i < this->vertices.size(); ++i) {
        changed = this->element->setVertexPosition(this->vertices[i], positions[i]) || changed;
    }
    return changed;
}
