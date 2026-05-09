/*
 * VertexNote
 *
 * Handles input of the ruler
 *
 * @author VertexNote Team
 * https://github.com/saitatter/vertex-note
 *
 * @license GNU GPLv2 or later
 */

#pragma once

#include <vector>

#include "model/PageRef.h"  // for PageRef

#include "BaseShapeHandler.h"  // for BaseShapeHandler

class Point;
class Control;

class RulerHandler: public BaseShapeHandler {
public:
    RulerHandler(Control* control, const PageRef& page);
    ~RulerHandler() override;

private:
    auto createShape(bool isAltDown, bool isShiftDown, bool isControlDown)
            -> std::pair<std::vector<Point>, Range> override;
};
