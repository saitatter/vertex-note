/*
 * VertexNote
 *
 * Undo action resize
 *
 * @author VertexNote Team
 * https://github.com/saitatter/vertex-note
 *
 * @license GNU GPLv2 or later
 */

#pragma once

#include <string>  // for string
#include <vector>  // for vector

#include "model/PageRef.h"  // for PageRef

#include "UndoAction.h"  // for UndoAction

class Layer;
class FillUndoActionEntry;
class Stroke;
class Control;

class FillUndoAction: public UndoAction {
public:
    FillUndoAction(const PageRef& page, Layer* layer);
    ~FillUndoAction() override;

public:
    bool undo(Control* control) override;
    bool redo(Control* control) override;
    std::string getText() override;

    void addStroke(Stroke* s, int originalFill, int newFill);

private:
    std::vector<FillUndoActionEntry*> data;

    Layer* layer;
};
