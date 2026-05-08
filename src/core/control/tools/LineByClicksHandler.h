/*
 * VertexNote
 *
 * Click-based line creation.
 */

#pragma once

#include <memory>
#include <optional>

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

namespace xoj::view {
class LineByClicksView;
class OverlayView;
class Repaintable;
}  // namespace xoj::view

class LineByClicksHandler final: public InputHandler {
public:
    LineByClicksHandler(Control* control, const PageRef& page);
    ~LineByClicksHandler() override;

    bool onMotionNotifyEvent(const PositionInputData& pos, double zoom) override;
    bool onKeyPressEvent(const KeyEvent& event) override;
    bool onKeyReleaseEvent(const KeyEvent& event) override;
    void onButtonReleaseEvent(const PositionInputData& pos, double zoom) override;
    void onButtonPressEvent(const PositionInputData& pos, double zoom) override;
    void onButtonDoublePressEvent(const PositionInputData& pos, double zoom) override;
    void onSequenceCancelEvent() override;
    std::unique_ptr<xoj::view::OverlayView> createView(xoj::view::Repaintable* parent) const override;
    bool isDone() const override;
    bool acceptsAdditionalPress() const override;

    [[nodiscard]] auto hasPreview() const -> bool;
    [[nodiscard]] auto getStartPoint() const -> Point;
    [[nodiscard]] auto getCurrentPoint() const -> Point;
    [[nodiscard]] auto getCurrentSnapKind() const -> std::optional<vn::snap::SnapKind>;
    [[nodiscard]] auto getStrokeWidth() const -> double;
    [[nodiscard]] auto getStrokeColor() const -> Color;

    [[nodiscard]] auto getViewPool() const
            -> const std::shared_ptr<xoj::util::DispatchPool<xoj::view::LineByClicksView>>&;

private:
    [[nodiscard]] auto previewRange() const -> Range;
    void updateCurrentPoint(const PositionInputData& pos, double zoom);
    [[nodiscard]] auto snapPoint(const Point& pagePoint, bool alt, double zoom) -> Point;
    void finalizeLine();
    void cancel();

private:
    SnapToGridInputHandler snappingHandler;
    vn::snap::SnapEngine snapEngine;
    std::optional<Point> startPoint;
    Point currentPoint;
    std::optional<vn::snap::SnapKind> currentSnapKind;
    bool geometrySnapEnabled = true;
    bool gridSnapEnabled = true;
    bool done = false;
    double strokeWidth = 1.0;
    Color strokeColor = Colors::black;
    std::shared_ptr<xoj::util::DispatchPool<xoj::view::LineByClicksView>> viewPool;
};
