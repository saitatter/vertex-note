/*
 * VertexNote
 *
 * Qt app shell command-state synchronization.
 */

#include "QtAppShell.h"

#include <QStatusBar>
#include <QString>
void QtAppShell::updateEditCommandStates() {
    this->window.commandHost()->setCommandEnabled("edit.undo-geometry", this->window.canvas()->canUndo());
    this->window.commandHost()->setCommandEnabled("edit.redo-geometry", this->window.canvas()->canRedo());
    const auto currentPage = this->window.canvas()->currentPageIndex();
    const bool hasDocument = this->documentController.hasDocument();
    const bool hasSelection = this->documentController.selectionBounds().has_value();
    const auto pageCount = this->documentController.pageCount();
    this->window.commandHost()->setCommandEnabled("edit.cut", hasSelection);
    this->window.commandHost()->setCommandEnabled("edit.copy", hasSelection);
    this->window.commandHost()->setCommandEnabled("edit.delete", hasSelection);
    this->window.commandHost()->setCommandEnabled("edit.bring-to-front", hasSelection);
    this->window.commandHost()->setCommandEnabled("edit.bring-forward", hasSelection);
    this->window.commandHost()->setCommandEnabled("edit.send-backward", hasSelection);
    this->window.commandHost()->setCommandEnabled("edit.send-to-back", hasSelection);
    this->window.commandHost()->setCommandEnabled("page.add-before", hasDocument);
    this->window.commandHost()->setCommandEnabled("page.add", hasDocument);
    this->window.commandHost()->setCommandEnabled("page.add-end", hasDocument);
    this->window.commandHost()->setCommandEnabled("page.duplicate", hasDocument && pageCount > 0U);
    this->window.commandHost()->setCommandEnabled("page.move-up", hasDocument && currentPage > 0U);
    this->window.commandHost()->setCommandEnabled("page.move-down", hasDocument && currentPage + 1U < pageCount);
    this->window.commandHost()->setCommandEnabled("page.delete", hasDocument && pageCount > 1U);
    this->window.commandHost()->setCommandEnabled("journal.append-new-pdf-pages", hasDocument);
    this->window.commandHost()->setCommandEnabled("page.format", this->documentController.canResizePage(currentPage));
    this->window.commandHost()->setCommandEnabled("edit.move-selection-layer-up",
                                                  this->documentController.canMoveSelectionToAdjacentLayer(+1));
    this->window.commandHost()->setCommandEnabled("edit.move-selection-layer-down",
                                                  this->documentController.canMoveSelectionToAdjacentLayer(-1));
    updateToolCommandStates();
    updateAudioCommandStates();
}

void QtAppShell::setGeometrySnapEnabled(bool enabled) {
    this->window.canvas()->setGeometrySnapEnabled(enabled);
    this->window.commandHost()->setCommandChecked("view.toggle-geometry-snap", enabled);
    this->window.statusBar()->showMessage(
            enabled ? QStringLiteral("Snap to vertex enabled") : QStringLiteral("Snap to vertex disabled"), 2500);
}

void QtAppShell::setGridSnapEnabled(bool enabled) {
    this->window.canvas()->setGridSnapEnabled(enabled);
    this->window.commandHost()->setCommandChecked("view.toggle-grid-snap", enabled);
    this->window.statusBar()->showMessage(
            enabled ? QStringLiteral("Snap to grid enabled") : QStringLiteral("Snap to grid disabled"), 2500);
}

void QtAppShell::setGeometrySelectionMode(QtGeometrySelectionMode mode) {
    auto& toolState = this->window.canvas()->toolState();
    toolState.geometrySelectionMode = mode;
    this->currentSettings.geometrySelectionModeDefault = mode;
    if (toolState.activeTool != QtToolType::SelectObject && toolState.activeTool != QtToolType::SelectRect) {
        this->window.canvas()->setActiveTool(QtToolType::SelectObject);
        this->window.toolPalette()->syncFromToolState(this->window.canvas()->toolState());
    }
    updateToolCommandStates();
    QString label = QStringLiteral("Vertex");
    if (mode == QtGeometrySelectionMode::Edge) {
        label = QStringLiteral("Edge");
    } else if (mode == QtGeometrySelectionMode::Object) {
        label = QStringLiteral("Object");
    }
    this->window.statusBar()->showMessage(QStringLiteral("Geometry selection: %1 mode").arg(label), 2500);
}

void QtAppShell::selectTool(QtToolType tool) {
    this->window.canvas()->setActiveTool(tool);
    updateToolCommandStates();
    this->window.toolPalette()->syncFromToolState(this->window.canvas()->toolState());
    syncToolbarWidgets();
    this->window.statusBar()->showMessage(
            QString::fromStdString("Tool: " + this->window.canvas()->toolState().activeToolName()), 2500);
}

void QtAppShell::toggleDrawingTool(QtToolType tool) {
    selectTool(this->window.canvas()->activeTool() == tool ? QtToolType::Pen : tool);
}

void QtAppShell::updateToolCommandStates() {
    const auto active = this->window.canvas()->activeTool();
    this->window.commandHost()->setCommandChecked("tool.hand", active == QtToolType::Hand);
    this->window.commandHost()->setCommandChecked("tool.pen", active == QtToolType::Pen);
    this->window.commandHost()->setCommandChecked("tool.laser-pointer-pen", active == QtToolType::LaserPointerPen);
    this->window.commandHost()->setCommandChecked("tool.laser-pointer-highlighter",
                                                  active == QtToolType::LaserPointerHighlighter);
    this->window.commandHost()->setCommandChecked("tool.setsquare", active == QtToolType::Setsquare);
    this->window.commandHost()->setCommandChecked("tool.compass", active == QtToolType::Compass);
    this->window.commandHost()->setCommandChecked("tool.eraser", active == QtToolType::Eraser);
    this->window.commandHost()->setCommandChecked("tool.highlighter", active == QtToolType::Highlighter);
    this->window.commandHost()->setCommandChecked("tool.select", active == QtToolType::SelectRect);
    this->window.commandHost()->setCommandChecked("tool.select-pdf-text-linear", active == QtToolType::PdfTextLinear);
    this->window.commandHost()->setCommandChecked("tool.select-pdf-text-rect", active == QtToolType::PdfTextRect);
    this->window.commandHost()->setCommandChecked("tool.select-region", active == QtToolType::SelectRegion);
    this->window.commandHost()->setCommandChecked("tool.select-multilayer-rect", active == QtToolType::SelectMultiLayerRect);
    this->window.commandHost()->setCommandChecked("tool.select-multilayer-region", active == QtToolType::SelectMultiLayerRegion);
    this->window.commandHost()->setCommandChecked("tool.select-object", active == QtToolType::SelectObject);
    this->window.commandHost()->setCommandChecked("tool.vertical-space", active == QtToolType::VerticalSpace);
    this->window.commandHost()->setCommandChecked("tool.text", active == QtToolType::Text);
    this->window.commandHost()->setCommandChecked("tool.draw-line", active == QtToolType::DrawLine);
    this->window.commandHost()->setCommandChecked("tool.draw-rectangle", active == QtToolType::DrawRectangle);
    this->window.commandHost()->setCommandChecked("tool.draw-circle", active == QtToolType::DrawCircle);
    this->window.commandHost()->setCommandChecked("tool.draw-ellipse", active == QtToolType::DrawEllipse);
    this->window.commandHost()->setCommandChecked("tool.draw-arrow", active == QtToolType::DrawArrow);
    this->window.commandHost()->setCommandChecked("tool.draw-double-arrow", active == QtToolType::DrawDoubleArrow);
    this->window.commandHost()->setCommandChecked("tool.draw-coordinate-system", active == QtToolType::DrawCoordinateSystem);
    this->window.commandHost()->setCommandChecked("tool.draw-spline", active == QtToolType::DrawSpline);
    this->window.commandHost()->setCommandChecked("tool.draw-shape-recognizer", active == QtToolType::ShapeRecognizer);
    this->window.commandHost()->setCommandChecked("tool.draw-arc", active == QtToolType::DrawArc);
    this->window.commandHost()->setCommandChecked("tool.draw-edge", active == QtToolType::DrawEdge);
    this->window.commandHost()->setCommandChecked("tool.draw-polyline", active == QtToolType::DrawPolyline);
    this->window.commandHost()->setCommandChecked("tool.draw-construction-line", active == QtToolType::DrawConstructionLine);
    this->window.commandHost()->setCommandChecked("tool.draw-construction-circle", active == QtToolType::DrawConstructionCircle);
    const auto geometrySelectionMode = this->window.canvas()->toolState().geometrySelectionMode;
    this->window.commandHost()->setCommandChecked("geometry.selection-mode-vertex",
                                                  geometrySelectionMode == QtGeometrySelectionMode::Vertex);
    this->window.commandHost()->setCommandChecked("geometry.selection-mode-edge",
                                                  geometrySelectionMode == QtGeometrySelectionMode::Edge);
    this->window.commandHost()->setCommandChecked("geometry.selection-mode-object",
                                                  geometrySelectionMode == QtGeometrySelectionMode::Object);
    const bool hasGeometrySelection = this->documentController.selectedGeometry().has_value();
    this->window.commandHost()->setCommandEnabled("geometry.translate-vertices", hasGeometrySelection);
    this->window.commandHost()->setCommandEnabled("geometry.rotate-selection", hasGeometrySelection);
    syncToolbarWidgets();
}

void QtAppShell::updateAudioCommandStates() {
    const bool audioAvailable = this->audioController.isAudioAvailable();
    const bool canPlayTarget =
            audioAvailable && this->audioController.canStartPlayback(selectedElementsForAudioPlayback(),
                                                                    this->documentController.sourcePath());
    const bool canSeekOrStop = audioAvailable &&
                               (this->audioController.hasCurrentPlayback() || this->audioController.isPlaying() ||
                                this->audioController.isPaused());

    this->window.commandHost()->setCommandEnabled("audio.record", audioAvailable);
    this->window.commandHost()->setCommandChecked("audio.record", this->audioController.isRecording());
    this->window.commandHost()->setCommandEnabled("audio.pause-playback", audioAvailable && canPlayTarget);
    this->window.commandHost()->setCommandChecked("audio.pause-playback", this->audioController.isPaused());
    this->window.commandHost()->setCommandEnabled("audio.play-object", audioAvailable && canPlayTarget);
    this->window.commandHost()->setCommandEnabled("audio.seek-backwards", canSeekOrStop);
    this->window.commandHost()->setCommandEnabled("audio.seek-forwards", canSeekOrStop);
    this->window.commandHost()->setCommandEnabled("audio.stop-playback", canSeekOrStop);
}
