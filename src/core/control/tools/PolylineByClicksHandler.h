/*
 * VertexNote
 *
 * Click-based polyline creation.
 */

#pragma once

#include "view/ViewNamespaceAliases.h"

#include <memory>
#include <optional>
#include <vector>

#include "model/PageRef.h"
#include "model/Point.h"
#include "util/Color.h"
#include "util/Range.h"
#include "vertexnote/snapping/SnapEngine.h"

#include "InputHandler.h"
#include "SnapToGridInputHandler.h"

class Control;
class PositionInputData;

namespace xoj::util {
template <class T>
class DispatchPool;
}

namespace vn::view {
class OverlayView;
class PolylineByClicksView;
class Repaintable;
}  // namespace vn::view

class PolylineByClicksHandler final: public InputHandler {
public:
    PolylineByClicksHandler(Control* control, const PageRef& page);
    ~PolylineByClicksHandler() override;

    bool onMotionNotifyEvent(const PositionInputData& pos, double zoom) override;
    bool onKeyPressEvent(const KeyEvent& event) override;
    bool onKeyReleaseEvent(const KeyEvent& event) override;
    void onButtonReleaseEvent(const PositionInputData& pos, double zoom) override;
    void onButtonPressEvent(const PositionInputData& pos, double zoom) override;
    void onButtonDoublePressEvent(const PositionInputData& pos, double zoom) override;
    void onSequenceCancelEvent() override;
    std::unique_ptr<vn::view::OverlayView> createView(vn::view::Repaintable* parent) const override;
    bool isDone() const override;
    bool acceptsAdditionalPress() const override;

    [[nodiscard]] auto hasPreview() const -> bool;
    [[nodiscard]] auto getPoints() const -> const std::vector<Point>&;
    [[nodiscard]] auto getCurrentPoint() const -> Point;
    [[nodiscard]] auto getCurrentSnapKind() const -> std::optional<vn::snap::SnapKind>;
    [[nodiscard]] auto getStrokeWidth() const -> double;
    [[nodiscard]] auto getStrokeColor() const -> Color;

    [[nodiscard]] auto getViewPool() const
            -> const std::shared_ptr<xoj::util::DispatchPool<vn::view::PolylineByClicksView>>&;

private:
    [[nodiscard]] auto previewRange() const -> Range;
    void updateCurrentPoint(const PositionInputData& pos, double zoom);
    [[nodiscard]] auto snapPoint(const Point& pagePoint, bool alt, double zoom) -> Point;
    void addCurrentPoint();
    void finalizePolyline();
    void cancel();

private:
    SnapToGridInputHandler snappingHandler;
    vn::snap::SnapEngine snapEngine;
    std::vector<Point> points;
    Point currentPoint;
    std::optional<vn::snap::SnapKind> currentSnapKind;
    bool geometrySnapEnabled = true;
    bool gridSnapEnabled = true;
    bool done = false;
    double strokeWidth = 1.0;
    Color strokeColor = Colors::black;
    std::shared_ptr<xoj::util::DispatchPool<vn::view::PolylineByClicksView>> viewPool;
};
