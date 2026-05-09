/*
 * VertexNote
 *
 * Handles input to draw a rectangle
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

class Point;
class Control;

class RectangleHandler: public BaseShapeHandler {
public:
    RectangleHandler(Control* control, const PageRef& page, bool flipShift = false, bool flipControl = false);
    ~RectangleHandler() override;

private:
    auto createShape(bool isAltDown, bool isShiftDown, bool isControlDown)
            -> std::pair<std::vector<Point>, Range> override;
};
