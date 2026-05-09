/*
 * VertexNote
 *
 * Undo action for rescale (EditSelection)
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

class Control;
class Document;
class Element;

class ScaleUndoAction: public UndoAction {
public:
    ScaleUndoAction(const PageRef& page, std::vector<Element*>* elements, double x0, double y0, double fx, double fy,
                    double rotation, bool restoreLineWidth);
    ~ScaleUndoAction() override;

public:
    bool undo(Control* control) override;
    bool redo(Control* control) override;
    std::string getText() override;

private:
    void applyScale(double fx, double fy, bool restoreLineWidth, Document* doc);

private:
    std::vector<Element*> elements;

    double x0;
    double y0;
    double fx;
    double fy;
    double rotation;
    bool restoreLineWidth;
};
