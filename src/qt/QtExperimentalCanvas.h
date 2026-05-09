/*
 * VertexNote
 *
 * Experimental Qt canvas bootstrap.
 */

#pragma once

#include <memory>
#include <vector>

#include <QWidget>

#include "QtExperimentalDocumentController.h"
#include "QtExperimentalDocumentSession.h"
#include "QtInputAdapter.h"
#include "ui/common/ICanvasHost.h"
#include "ui/input/UiInputEvents.h"
#include "view/render/Renderers.h"

class QtExperimentalCanvas: public QWidget, public vn::ui::common::ICanvasHost, public vn::ui::input::IInputEventSink {
    Q_OBJECT

public:
    explicit QtExperimentalCanvas(QWidget* parent = nullptr);

public:
    void invalidateCanvas() override;
    void invalidateRect(double x, double y, double width, double height) override;
    void setCanvasCursor(vn::ui::common::CanvasCursor cursor) override;
    [[nodiscard]] auto viewport() const -> vn::ui::common::CanvasViewport override;
    void handlePointerEvent(const vn::ui::input::PointerEvent& event) override;
    void handleKeyboardEvent(const vn::ui::input::KeyboardEvent& event) override;
    void handleTouchEvent(const vn::ui::input::TouchEvent& event) override;
    void setDocumentController(const QtExperimentalDocumentController* documentController);
    void newBlankDocument();
    void setViewportState(double zoom, double scrollX, double scrollY);
    [[nodiscard]] auto sessionViewportState() const -> QtExperimentalViewportState;
    void zoomIn();
    void zoomOut();
    void resetViewport();
    void fitPage(bool edited = true);
    void panBy(double dx, double dy);

protected:
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void wheelEvent(QWheelEvent* event) override;
    void tabletEvent(QTabletEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;
    void keyReleaseEvent(QKeyEvent* event) override;
    bool event(QEvent* event) override;

Q_SIGNALS:
    void viewportStateChanged();
    void statusHintChanged(const QString& text);
    void documentEdited();

private:
    void updateDebugOverlay(QString summary);
    void emitViewportUpdate(bool edited = true);
    void zoomAroundScreenPoint(double factor, const QPointF& screenPoint);
    [[nodiscard]] auto pageRects() const -> std::vector<QRectF>;
    [[nodiscard]] auto documentSceneBounds() const -> QRectF;
    void drawPageContents(QPainter& painter, const QRectF& rect, const QtExperimentalPageInfo& pageInfo,
                          std::size_t pageIndex) const;
    void beginPan(const QPointF& position);
    void endPan();

private:
    std::unique_ptr<QtInputAdapter> inputAdapter;
    std::unique_ptr<vn::view::render::BackgroundRenderer> backgroundRenderer;
    QString lastEventSummary;
    const QtExperimentalDocumentController* documentController = nullptr;
    double zoomFactor = 1.0;
    double scrollX = 0.0;
    double scrollY = 0.0;
    bool spaceHeld = false;
    bool panning = false;
    QPointF lastPanScreenPosition;
};
