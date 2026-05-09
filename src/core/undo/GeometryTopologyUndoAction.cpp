/*
 * VertexNote
 *
 * Undo action for topology edits on an object-based geometry element.
 */

#include "GeometryTopologyUndoAction.h"

#include <utility>

#include "control/Control.h"
#include "model/Document.h"
#include "model/NotePage.h"
#include "vertexnote/geometry/GeometryElement.h"

GeometryTopologyUndoAction::GeometryTopologyUndoAction(PageRef page, vn::geom::GeometryElement* element,
                                                       vn::geom::GeometryObject before, vn::geom::GeometryObject after,
                                                       std::string text):
        UndoAction("GeometryTopologyUndoAction"),
        element(element),
        before(std::move(before)),
        after(std::move(after)),
        text(std::move(text)) {
    this->page = std::move(page);
}

auto GeometryTopologyUndoAction::undo(Control* control) -> bool {
    Document* doc = control->getDocument();
    doc->lock();
    const bool changed = apply(this->before);
    doc->unlock();
    if (changed) {
        this->page->fireElementChanged(this->element);
    }
    this->undone = true;
    return changed;
}

auto GeometryTopologyUndoAction::redo(Control* control) -> bool {
    Document* doc = control->getDocument();
    doc->lock();
    const bool changed = apply(this->after);
    doc->unlock();
    if (changed) {
        this->page->fireElementChanged(this->element);
    }
    this->undone = false;
    return changed;
}

auto GeometryTopologyUndoAction::getText() -> std::string { return this->text; }

auto GeometryTopologyUndoAction::getPages() -> std::vector<PageRef> { return {this->page}; }

auto GeometryTopologyUndoAction::apply(const vn::geom::GeometryObject& state) -> bool {
    if (!this->element) {
        return false;
    }

    this->element->replaceGeometry(state);
    return true;
}
