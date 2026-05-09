/*
 * VertexNote
 *
 * Undo action for creating, editing, or deleting a geometry constraint.
 */

#include "GeometryConstraintUndoAction.h"

#include <utility>

#include "control/Control.h"
#include "model/Document.h"
#include "model/NotePage.h"
#include "util/i18n.h"
#include "vertexnote/geometry/GeometryElement.h"

GeometryConstraintUndoAction::GeometryConstraintUndoAction(PageRef page, vn::geom::GeometryElement* element,
                                                           std::optional<vn::geom::Constraint> before,
                                                           std::optional<vn::geom::Constraint> after):
        UndoAction("GeometryConstraintUndoAction"),
        page(std::move(page)),
        element(element),
        before(std::move(before)),
        after(std::move(after)) {}

auto GeometryConstraintUndoAction::undo(Control* control) -> bool {
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

auto GeometryConstraintUndoAction::redo(Control* control) -> bool {
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

auto GeometryConstraintUndoAction::getPages() -> std::vector<PageRef> { return {this->page}; }

auto GeometryConstraintUndoAction::getText() -> std::string {
    if (!this->before && this->after) {
        return _("Create geometry constraint");
    }
    if (this->before && !this->after) {
        return _("Delete geometry constraint");
    }
    return _("Edit geometry constraint");
}

auto GeometryConstraintUndoAction::apply(const std::optional<vn::geom::Constraint>& state) -> bool {
    if (!this->element) {
        return false;
    }

    if (!state) {
        const auto id = this->before ? this->before->id : this->after ? this->after->id : vn::geom::InvalidConstraintId;
        return this->element->geometry().removeConstraint(id);
    }

    return this->element->geometry().replaceConstraint(*state);
}
