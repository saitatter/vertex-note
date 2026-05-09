/*
 * VertexNote
 *
 * Undo action for insert (write text, draw stroke...)
 *
 * @author VertexNote Team
 * https://github.com/saitatter/vertex-note
 *
 * @license GNU GPLv2 or later
 */

#pragma once

#include <string>  // for string

#include "model/PageRef.h"  // for PageRef

#include "UndoAction.h"  // for UndoAction

class Element;
class Layer;
class Control;

using ElementPtr = std::unique_ptr<Element>;

class TextBoxUndoAction: public UndoAction {
public:
    TextBoxUndoAction(const PageRef& page, Layer* layer, Element* element, ElementPtr oldelement);
    ~TextBoxUndoAction() override;

public:
    auto undo(Control* control) -> bool override;
    auto redo(Control* control) -> bool override;

    auto getText() -> std::string override;

private:
    Layer* layer;
    Element* element;
    ElementPtr oldelement;
};
