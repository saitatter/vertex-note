/*
 * VertexNote
 *
 * Handles input to draw an coordinate system
 *
 * @author VertexNote Team
 * https://github.com/saitatter/vertex-note
 *
 * @license GNU GPLv2 or later
 */

#pragma once

#include <vector>  // for vector

#include "model/PageRef.h"  // for PageRef

#include "BaseShapeHandler.h"  // for BaseShapeHandler

class VertexNoteView;

class CoordinateSystemHandler: public BaseShapeHandler {
public:
    CoordinateSystemHandler(Control* control, const PageRef& page, bool flipShift = false, bool flipControl = false);
    ~CoordinateSystemHandler() override;

private:
    auto createShape(bool isAltDown, bool isShiftDown, bool isControlDown)
            -> std::pair<std::vector<Point>, Range> override;
};
