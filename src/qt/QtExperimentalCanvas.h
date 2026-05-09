/*
 * VertexNote
 *
 * Experimental Qt canvas bootstrap.
 */

#pragma once

#include <QWidget>

#include "ui/common/ICanvasHost.h"

class QtExperimentalCanvas: public QWidget, public vn::ui::common::ICanvasHost {
public:
    explicit QtExperimentalCanvas(QWidget* parent = nullptr);

public:
    void invalidateCanvas() override;
    void invalidateRect(double x, double y, double width, double height) override;
    void setCanvasCursor(vn::ui::common::CanvasCursor cursor) override;
    [[nodiscard]] auto viewport() const -> vn::ui::common::CanvasViewport override;

protected:
    void paintEvent(QPaintEvent* event) override;
};
