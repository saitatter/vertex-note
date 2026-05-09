/*
 * VertexNote
 *
 * Undo action for insert page / delete page
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

class Control;


class InsertDeletePageUndoAction: public UndoAction {
public:
    InsertDeletePageUndoAction(const PageRef& page, size_t pagePos, bool inserted);
    ~InsertDeletePageUndoAction() override;

public:
    bool undo(Control* control) override;
    bool redo(Control* control) override;

    std::string getText() override;

private:
    bool insertPage(Control* control);
    bool deletePage(Control* control);

private:
    bool inserted;
    size_t pagePos;
};
