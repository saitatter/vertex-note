/*
 * VertexNote
 *
 * Platform-neutral canvas/viewport contract.
 */

#pragma once

#include "UiTypes.h"

namespace vn::ui::common {

class ICanvasHost {
public:
    virtual ~ICanvasHost() = default;

    virtual void invalidateCanvas() = 0;
    virtual void invalidateRect(double x, double y, double width, double height) = 0;
    virtual void setCanvasCursor(CanvasCursor cursor) = 0;
    [[nodiscard]] virtual auto viewport() const -> CanvasViewport = 0;
};

}  // namespace vn::ui::common
