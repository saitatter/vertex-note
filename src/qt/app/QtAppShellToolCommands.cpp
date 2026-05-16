/*
 * VertexNote
 *
 * Qt app shell command registration.
 */

#include "QtAppShell.h"

#include <string>

#include <QDesktopServices>
#include <QDialog>
#include <QDialogButtonBox>
#include <QInputDialog>
#include <QLabel>
#include <QListWidget>
#include <QStatusBar>
#include <QString>
#include <QUrl>
#include <QVBoxLayout>
void QtAppShell::registerToolCommands() {
    auto* ch = this->window.commandHost();

    // =====================================================================
    // Menu 6: Tools
    // =====================================================================
    // Drawing tools
    ch->registerCommand(
            {.id = "tool.pen", .text = "Pen", .tooltip = "Draw freehand strokes", .shortcut = "Ctrl+Shift+P",
             .menu = "Tools", .checkable = true, .checked = this->window.canvas()->activeTool() == QtToolType::Pen},
            [this]() { selectTool(QtToolType::Pen); });
    ch->registerCommand(
            {.id = "tool.eraser", .text = "Eraser", .tooltip = "Erase strokes", .shortcut = "Ctrl+Shift+E",
             .menu = "Tools", .checkable = true, .checked = this->window.canvas()->activeTool() == QtToolType::Eraser},
            [this]() { selectTool(QtToolType::Eraser); });
    ch->registerCommand(
            {.id = "tool.highlighter", .text = "Highlighter", .tooltip = "Draw highlight strokes", .shortcut = "Ctrl+Shift+H",
             .menu = "Tools", .checkable = true, .checked = this->window.canvas()->activeTool() == QtToolType::Highlighter},
            [this]() { selectTool(QtToolType::Highlighter); });
    ch->registerCommand(
            {.id = "tool.laser-pointer-pen", .text = "Laser Pointer Pen", .tooltip = "Draw temporary laser pen strokes",
             .menu = "Tools", .checkable = true},
            [this]() { selectTool(QtToolType::LaserPointerPen); });
    ch->registerCommand(
            {.id = "tool.laser-pointer-highlighter", .text = "Laser Pointer Highlighter",
             .tooltip = "Draw temporary laser highlighter strokes", .menu = "Tools", .checkable = true},
            [this]() { selectTool(QtToolType::LaserPointerHighlighter); });
    ch->registerCommand(
            {.id = "tool.setsquare", .text = "Setsquare", .tooltip = "Draw guided straight strokes with a setsquare",
             .menu = "Tools", .checkable = true},
            [this]() { selectTool(QtToolType::Setsquare); });
    ch->registerCommand(
            {.id = "tool.compass", .text = "Compass", .tooltip = "Draw guided arcs and radius strokes with a compass",
             .menu = "Tools", .checkable = true},
            [this]() { selectTool(QtToolType::Compass); });
    ch->registerCommand(
            {.id = "tool.text", .text = "Text", .tooltip = "Insert or edit text", .shortcut = "Ctrl+Shift+T",
             .menu = "Tools", .checkable = true, .checked = this->window.canvas()->activeTool() == QtToolType::Text},
            [this]() { selectTool(QtToolType::Text); });
    ch->registerCommand(
            {.id = "tool.math-tex", .text = "Math TeX", .tooltip = "Insert a LaTeX formula", .shortcut = "Ctrl+Shift+X",
             .menu = "Tools"},
            [this]() { insertMathTex(); });
    ch->registerCommand(
            {.id = "tool.select-pdf-text-linear", .text = "Select Linear PDF Text",
             .tooltip = "Select PDF text along dragged glyphs", .menu = "Tools", .checkable = true},
            [this]() { selectTool(QtToolType::PdfTextLinear); });
    ch->registerCommand(
            {.id = "tool.select-pdf-text-rect", .text = "Select Area PDF Text",
             .tooltip = "Select PDF text inside a dragged rectangle", .menu = "Tools", .checkable = true},
            [this]() { selectTool(QtToolType::PdfTextRect); });
    ch->registerCommand(
            {.id = "tool.pdf-text-highlight", .text = "Highlight Selected PDF Text",
             .tooltip = "Create highlighter strokes over the active PDF text selection", .menu = "Tools"},
            [this]() { highlightPdfTextSelection(); });
    ch->registerCommand(
            {.id = "tool.select-pdf-text-marker-opacity", .text = "PDF Text Marker Opacity",
             .tooltip = "Set PDF text highlight marker opacity", .menu = "Tools"},
            [this]() {
                bool ok = false;
                const int opacity =
                        QInputDialog::getInt(&this->window, QStringLiteral("PDF Text Marker Opacity"),
                                             QStringLiteral("Opacity:"), this->window.canvas()->toolState().pdfTextMarkerOpacity,
                                             0, 255, 8, &ok);
                if (ok) {
                    setPdfTextMarkerOpacity(opacity);
                }
            });
    ch->registerCommand(
            {.id = "edit.insert-image", .text = "Image", .tooltip = "Insert image from file", .shortcut = "Ctrl+Shift+I", .menu = "Tools"},
            [this]() { insertImage(); });
    ch->registerCommand(
            {.id = "audio.record", .text = "Audio Record", .tooltip = "Start or stop audio recording",
             .menu = "Tools", .checkable = true},
            [this]() { toggleAudioRecording(); });
    ch->registerCommand(
            {.id = "audio.pause-playback", .text = "Audio Play / Pause", .tooltip = "Play, pause, or resume audio",
             .menu = "Tools", .checkable = true},
            [this]() { toggleAudioPausePlayback(); });
    ch->registerCommand(
            {.id = "audio.seek-backwards", .text = "Audio Back", .tooltip = "Seek backwards in the active clip",
             .menu = "Tools"},
            [this]() { seekAudioBackwards(); });
    ch->registerCommand(
            {.id = "audio.seek-forwards", .text = "Audio Forward", .tooltip = "Seek forwards in the active clip",
             .menu = "Tools"},
            [this]() { seekAudioForwards(); });
    ch->registerCommand(
            {.id = "audio.stop-playback", .text = "Audio Stop", .tooltip = "Stop audio playback",
             .menu = "Tools"},
            [this]() { stopAudioPlayback(); });
    ch->registerCommand(
            {.id = "audio.play-object", .text = "Play Object", .tooltip = "Play audio attached to the selected object",
             .menu = "Tools"},
            [this]() { toggleAudioPausePlayback(); });
    ch->addMenuSeparator("Tools");

    // Stroke Drawing submenu
    ch->registerCommand(
            {.id = "tool.draw-rectangle", .text = "Draw Rectangle", .tooltip = "Draw a rectangle", .shortcut = "Ctrl+2",
             .menu = "Tools>Stroke Drawing", .checkable = true, .checked = this->window.canvas()->activeTool() == QtToolType::DrawRectangle},
            [this]() { toggleDrawingTool(QtToolType::DrawRectangle); });
    ch->registerCommand(
            {.id = "tool.draw-ellipse", .text = "Draw Ellipse", .tooltip = "Draw an ellipse", .shortcut = "Ctrl+3",
             .menu = "Tools>Stroke Drawing", .checkable = true},
            [this]() { toggleDrawingTool(QtToolType::DrawEllipse); });
    ch->registerCommand(
            {.id = "tool.draw-arrow", .text = "Draw Arrow", .tooltip = "Draw an arrow", .shortcut = "Ctrl+4",
             .menu = "Tools>Stroke Drawing", .checkable = true},
            [this]() { toggleDrawingTool(QtToolType::DrawArrow); });
    ch->registerCommand(
            {.id = "tool.draw-double-arrow", .text = "Draw Double Arrow", .tooltip = "Draw a double-headed arrow", .shortcut = "Ctrl+5",
             .menu = "Tools>Stroke Drawing", .checkable = true},
            [this]() { toggleDrawingTool(QtToolType::DrawDoubleArrow); });
    ch->registerCommand(
            {.id = "tool.draw-coordinate-system", .text = "Draw Coordinate System", .tooltip = "Draw X-Y axes", .shortcut = "Ctrl+6",
             .menu = "Tools>Stroke Drawing", .checkable = true},
            [this]() { toggleDrawingTool(QtToolType::DrawCoordinateSystem); });
    ch->registerCommand(
            {.id = "tool.draw-line", .text = "Draw Line", .tooltip = "Draw a stroke-based straight line", .shortcut = "Ctrl+7",
             .menu = "Tools>Stroke Drawing", .checkable = true, .checked = this->window.canvas()->activeTool() == QtToolType::DrawLine},
            [this]() { toggleDrawingTool(QtToolType::DrawLine); });
    ch->registerCommand(
            {.id = "tool.draw-spline", .text = "Draw Spline", .tooltip = "Draw a smooth spline curve", .shortcut = "Ctrl+8",
             .menu = "Tools>Stroke Drawing", .checkable = true},
            [this]() { toggleDrawingTool(QtToolType::DrawSpline); });
    ch->registerCommand(
            {.id = "tool.draw-shape-recognizer", .text = "Shape Recognizer",
             .tooltip = "Recognize strokes as clean geometric shapes", .menu = "Tools>Stroke Drawing", .checkable = true},
            [this]() { toggleDrawingTool(QtToolType::ShapeRecognizer); });
    ch->addMenuSeparator("Tools");
    // Vertex Drawing submenu
    ch->registerCommand(
            {.id = "tool.draw-edge", .text = "Draw Edge", .tooltip = "Draw a vertex geometry edge", .shortcut = "Ctrl+Shift+7",
             .menu = "Tools>Vertex Drawing", .checkable = true, .checked = this->window.canvas()->activeTool() == QtToolType::DrawEdge},
            [this]() { toggleDrawingTool(QtToolType::DrawEdge); });
    ch->registerCommand(
            {.id = "tool.draw-circle", .text = "Draw Vertex Circle", .tooltip = "Draw a geometry circle", .shortcut = "Ctrl+9",
             .menu = "Tools>Vertex Drawing", .checkable = true, .checked = this->window.canvas()->activeTool() == QtToolType::DrawCircle},
            [this]() { toggleDrawingTool(QtToolType::DrawCircle); });
    ch->registerCommand(
            {.id = "tool.draw-arc", .text = "Draw Vertex Arc", .tooltip = "Draw a geometry arc", .shortcut = "Ctrl+0",
             .menu = "Tools>Vertex Drawing", .checkable = true, .checked = this->window.canvas()->activeTool() == QtToolType::DrawArc},
            [this]() { toggleDrawingTool(QtToolType::DrawArc); });
    ch->registerCommand(
            {.id = "tool.draw-construction-line", .text = "Draw Construction Line", .tooltip = "Draw a construction guide line", .shortcut = "Ctrl+Shift+0",
             .menu = "Tools>Vertex Drawing", .checkable = true, .checked = this->window.canvas()->activeTool() == QtToolType::DrawConstructionLine},
            [this]() { toggleDrawingTool(QtToolType::DrawConstructionLine); });
    ch->registerCommand(
            {.id = "tool.draw-construction-circle", .text = "Draw Construction Circle", .tooltip = "Draw a construction guide circle",
             .menu = "Tools>Vertex Drawing", .checkable = true, .checked = this->window.canvas()->activeTool() == QtToolType::DrawConstructionCircle},
            [this]() { toggleDrawingTool(QtToolType::DrawConstructionCircle); });
    ch->registerCommand(
            {.id = "tool.draw-polyline", .text = "Draw Polyline", .tooltip = "Draw a multi-segment line",
             .menu = "Tools>Vertex Drawing", .checkable = true, .checked = this->window.canvas()->activeTool() == QtToolType::DrawPolyline},
            [this]() { toggleDrawingTool(QtToolType::DrawPolyline); });
    ch->addMenuSeparator("Tools");

    // Selection tools
    ch->registerCommand(
            {.id = "tool.select", .text = "Select Rectangle", .tooltip = "Rectangle selection", .shortcut = "Ctrl+Shift+R",
             .menu = "Tools", .checkable = true, .checked = this->window.canvas()->activeTool() == QtToolType::SelectRect},
            [this]() { selectTool(QtToolType::SelectRect); });
    ch->registerCommand(
            {.id = "tool.select-region", .text = "Select Region", .tooltip = "Free-form selection", .shortcut = "Ctrl+Shift+G",
             .menu = "Tools", .checkable = true},
            [this]() { selectTool(QtToolType::SelectRegion); });
    ch->registerCommand(
            {.id = "tool.select-multilayer-rect", .text = "Select Multi-Layer Rect", .tooltip = "Rectangle selection across layers",
             .menu = "Tools", .checkable = true},
            [this]() { selectTool(QtToolType::SelectMultiLayerRect); });
    ch->registerCommand(
            {.id = "tool.select-multilayer-region", .text = "Select Multi-Layer Region", .tooltip = "Free-form selection across layers",
             .menu = "Tools", .checkable = true},
            [this]() { selectTool(QtToolType::SelectMultiLayerRegion); });
    ch->registerCommand(
            {.id = "tool.select-object", .text = "Select Object", .tooltip = "Select individual objects", .shortcut = "Ctrl+Shift+O",
             .menu = "Tools", .checkable = true},
            [this]() { selectTool(QtToolType::SelectObject); });
    ch->registerCommand(
            {.id = "tool.vertical-space", .text = "Vertical Space", .tooltip = "Insert vertical space", .shortcut = "Ctrl+Shift+V",
             .menu = "Tools", .checkable = true},
            [this]() { selectTool(QtToolType::VerticalSpace); });
    ch->registerCommand(
            {.id = "tool.hand", .text = "Hand Tool", .tooltip = "Pan the canvas", .shortcut = "Ctrl+Shift+A",
             .menu = "Tools", .checkable = true, .checked = this->window.canvas()->activeTool() == QtToolType::Hand},
            [this]() { selectTool(QtToolType::Hand); });
    ch->registerCommand(
            {.id = "tool.default-preset", .text = "Default Tool", .tooltip = "Restore the default pen preset and select it",
             .menu = "Tools"},
            [this]() {
                auto& ts = this->window.canvas()->toolState();
                ts.activeTool = QtToolType::Pen;
                ts.penWidth = this->currentSettings.defaultPenWidth;
                ts.highlighterWidth = this->currentSettings.defaultHighlighterWidth;
                ts.eraserWidth = this->currentSettings.defaultEraserWidth;
                ts.pressureSensitive = this->currentSettings.defaultPressureSensitive;
                ts.eraserMode = this->currentSettings.defaultEraserMode;
                ts.penLineStyle = "plain";
                ts.fillEnabled = false;
                this->window.canvas()->setActiveTool(QtToolType::Pen);
                this->window.toolPalette()->syncFromToolState(ts);
                syncToolbarWidgets();
                this->window.statusBar()->showMessage(QStringLiteral("Default pen preset restored"), 2500);
            });
    ch->addMenuSeparator("Tools");

    // Pen Options submenu
    ch->registerCommand(
            {.id = "pen.size-very-fine", .text = "Very Fine", .tooltip = "Very fine pen", .menu = "Tools>Pen Options", .checkable = true},
            [this]() { setPenSize(0); });
    ch->registerCommand(
            {.id = "pen.size-fine", .text = "Fine", .tooltip = "Fine pen", .menu = "Tools>Pen Options", .checkable = true, .checked = true},
            [this]() { setPenSize(1); });
    ch->registerCommand(
            {.id = "pen.size-medium", .text = "Medium", .tooltip = "Medium pen", .menu = "Tools>Pen Options", .checkable = true},
            [this]() { setPenSize(2); });
    ch->registerCommand(
            {.id = "pen.size-thick", .text = "Thick", .tooltip = "Thick pen", .menu = "Tools>Pen Options", .checkable = true},
            [this]() { setPenSize(3); });
    ch->registerCommand(
            {.id = "pen.size-very-thick", .text = "Very Thick", .tooltip = "Very thick pen", .menu = "Tools>Pen Options", .checkable = true},
            [this]() { setPenSize(4); });
    ch->addMenuSeparator("Tools>Pen Options");
    ch->registerCommand(
            {.id = "pen.line-solid", .text = "Standard", .tooltip = "Solid line", .menu = "Tools>Pen Options", .checkable = true, .checked = true},
            [this]() { setPenLineStyle("plain"); });
    ch->registerCommand(
            {.id = "pen.line-dash", .text = "Dashed", .tooltip = "Dashed line", .menu = "Tools>Pen Options", .checkable = true},
            [this]() { setPenLineStyle("dash"); });
    ch->registerCommand(
            {.id = "pen.line-dashdot", .text = "Dash-Dotted", .tooltip = "Dash-dot line", .menu = "Tools>Pen Options", .checkable = true},
            [this]() { setPenLineStyle("dashdot"); });
    ch->registerCommand(
            {.id = "pen.line-dot", .text = "Dotted", .tooltip = "Dotted line", .menu = "Tools>Pen Options", .checkable = true},
            [this]() { setPenLineStyle("dot"); });
    ch->addMenuSeparator("Tools>Pen Options");
    ch->registerCommand(
            {.id = "pen.fill-toggle", .text = "Fill", .tooltip = "Toggle fill", .menu = "Tools>Pen Options", .checkable = true},
            [this]() {
                auto& ts = this->window.canvas()->toolState();
                ts.fillEnabled = !ts.fillEnabled;
                this->window.commandHost()->setCommandChecked("pen.fill-toggle", ts.fillEnabled);
            });

    // Eraser Options submenu
    ch->registerCommand(
            {.id = "eraser.size-very-fine", .text = "Very Fine", .tooltip = "Very fine eraser", .menu = "Tools>Eraser Options", .checkable = true},
            [this]() { setEraserSize(0); });
    ch->registerCommand(
            {.id = "eraser.size-fine", .text = "Fine", .tooltip = "Fine eraser", .menu = "Tools>Eraser Options", .checkable = true},
            [this]() { setEraserSize(1); });
    ch->registerCommand(
            {.id = "eraser.size-medium", .text = "Medium", .tooltip = "Medium eraser", .menu = "Tools>Eraser Options", .checkable = true, .checked = true},
            [this]() { setEraserSize(2); });
    ch->registerCommand(
            {.id = "eraser.size-thick", .text = "Thick", .tooltip = "Thick eraser", .menu = "Tools>Eraser Options", .checkable = true},
            [this]() { setEraserSize(3); });
    ch->registerCommand(
            {.id = "eraser.size-very-thick", .text = "Very Thick", .tooltip = "Very thick eraser", .menu = "Tools>Eraser Options", .checkable = true},
            [this]() { setEraserSize(4); });
    ch->addMenuSeparator("Tools>Eraser Options");
    ch->registerCommand(
            {.id = "eraser.type-standard", .text = "Standard", .tooltip = "Standard eraser", .menu = "Tools>Eraser Options", .checkable = true, .checked = true},
            [this]() { setEraserType(QtEraserMode::Standard); });
    ch->registerCommand(
            {.id = "eraser.type-whiteout", .text = "Whiteout", .tooltip = "White-out eraser", .menu = "Tools>Eraser Options", .checkable = true},
            [this]() { setEraserType(QtEraserMode::Whiteout); });
    ch->registerCommand(
            {.id = "eraser.type-delete-stroke", .text = "Delete Strokes", .tooltip = "Delete entire strokes", .menu = "Tools>Eraser Options", .checkable = true},
            [this]() { setEraserType(QtEraserMode::DeleteStroke); });

    // Highlighter Options submenu
    ch->registerCommand(
            {.id = "highlighter.size-very-fine", .text = "Very Fine", .tooltip = "Very fine highlighter", .menu = "Tools>Highlighter Options", .checkable = true},
            [this]() { setHighlighterSize(0); });
    ch->registerCommand(
            {.id = "highlighter.size-fine", .text = "Fine", .tooltip = "Fine highlighter", .menu = "Tools>Highlighter Options", .checkable = true},
            [this]() { setHighlighterSize(1); });
    ch->registerCommand(
            {.id = "highlighter.size-medium", .text = "Medium", .tooltip = "Medium highlighter", .menu = "Tools>Highlighter Options", .checkable = true, .checked = true},
            [this]() { setHighlighterSize(2); });
    ch->registerCommand(
            {.id = "highlighter.size-thick", .text = "Thick", .tooltip = "Thick highlighter", .menu = "Tools>Highlighter Options", .checkable = true},
            [this]() { setHighlighterSize(3); });
    ch->registerCommand(
            {.id = "highlighter.size-very-thick", .text = "Very Thick", .tooltip = "Very thick highlighter", .menu = "Tools>Highlighter Options", .checkable = true},
            [this]() { setHighlighterSize(4); });
    ch->addMenuSeparator("Tools>Highlighter Options");
    ch->registerCommand(
            {.id = "highlighter.fill-toggle", .text = "Fill", .tooltip = "Toggle highlighter fill", .menu = "Tools>Highlighter Options", .checkable = true},
            [this]() {
                auto& ts = this->window.canvas()->toolState();
                ts.highlighterFillEnabled = !ts.highlighterFillEnabled;
                this->window.commandHost()->setCommandChecked("highlighter.fill-toggle", ts.highlighterFillEnabled);
            });
    ch->addMenuSeparator("Tools");

    // Other tool items
    ch->registerCommand(
            {.id = "edit.select-font", .text = "Text Font...", .tooltip = "Select font for text tool", .shortcut = "Ctrl+Shift+F", .menu = "Tools"},
            [this]() { selectFont(); });
    ch->addMenuSeparator("Tools");

    // Geometry editing
    ch->registerCommand(
            {.id = "geometry.selection-mode-vertex", .text = "Vertex Mode",
             .tooltip = "Select and transform geometry vertices",
             .menu = "Tools>Geometry Selection", .checkable = true,
             .checked = this->window.canvas()->toolState().geometrySelectionMode == QtGeometrySelectionMode::Vertex},
            [this]() { setGeometrySelectionMode(QtGeometrySelectionMode::Vertex); });
    ch->registerCommand(
            {.id = "geometry.selection-mode-edge", .text = "Edge Mode",
             .tooltip = "Select and transform geometry edges",
             .menu = "Tools>Geometry Selection", .checkable = true,
             .checked = this->window.canvas()->toolState().geometrySelectionMode == QtGeometrySelectionMode::Edge},
            [this]() { setGeometrySelectionMode(QtGeometrySelectionMode::Edge); });
    ch->registerCommand(
            {.id = "geometry.selection-mode-face", .text = "Face Mode",
             .tooltip = "Select and edit filled geometry faces",
             .menu = "Tools>Geometry Selection", .checkable = true,
             .checked = this->window.canvas()->toolState().geometrySelectionMode == QtGeometrySelectionMode::Face},
            [this]() { setGeometrySelectionMode(QtGeometrySelectionMode::Face); });
    ch->registerCommand(
            {.id = "geometry.selection-mode-object", .text = "Object Mode",
             .tooltip = "Select and transform whole geometry objects",
             .menu = "Tools>Geometry Selection", .checkable = true,
             .checked = this->window.canvas()->toolState().geometrySelectionMode == QtGeometrySelectionMode::Object},
            [this]() { setGeometrySelectionMode(QtGeometrySelectionMode::Object); });
    ch->addMenuSeparator("Tools>Geometry Selection");
    ch->registerCommand(
            {.id = "view.toggle-geometry-snap", .text = "Snap to Vertex",
             .tooltip = "Snap geometry drawing and edits to vertices, edge points, and guides",
             .menu = "Tools>Snapping", .checkable = true, .checked = this->window.canvas()->isGeometrySnapEnabled()},
            [this]() { setGeometrySnapEnabled(!this->window.canvas()->isGeometrySnapEnabled()); });
    ch->registerCommand(
            {.id = "view.toggle-touch-drawing", .text = "Touch Drawing", .tooltip = "Toggle finger drawing on touch devices",
             .menu = "Tools>Snapping", .checkable = true, .checked = this->window.canvas()->isTouchDrawingEnabled()},
            [this]() {
                const bool enabled = !this->window.canvas()->isTouchDrawingEnabled();
                this->window.canvas()->setTouchDrawingEnabled(enabled);
                this->window.commandHost()->setCommandChecked("view.toggle-touch-drawing", enabled);
                this->window.statusBar()->showMessage(
                        enabled ? QStringLiteral("Touch drawing enabled")
                                : QStringLiteral("Touch drawing disabled"),
                        2500);
            });
    ch->registerCommand(
            {.id = "geometry.translate-vertices", .text = "Translate Selected Geometry...",
             .tooltip = "Move selected vertices or the selected geometry object by an exact delta",
             .menu = "Tools>Vertex Transform", .enabled = this->documentController.selectedGeometry().has_value()},
            [this]() { translateSelectedVertices(); });
    ch->registerCommand(
            {.id = "geometry.rotate-selection", .text = "Rotate Selected Geometry...",
             .tooltip = "Rotate selected geometry vertices or the selected geometry object",
             .menu = "Tools>Vertex Transform", .enabled = this->documentController.selectedGeometry().has_value()},
            [this]() { rotateSelectedGeometry(); });
    ch->registerCommand(
            {.id = "geometry.scale-selection", .text = "Scale Selected Geometry...",
             .tooltip = "Scale selected geometry vertices or the selected geometry object",
             .menu = "Tools>Vertex Transform", .enabled = this->documentController.selectedGeometry().has_value()},
            [this]() { scaleSelectedGeometry(); });
    ch->registerCommand(
            {.id = "geometry.create-3d-box", .text = "3D Box",
             .tooltip = "Create an isometric 3D wireframe box on the current page",
             .menu = "Tools>3D Geometry"},
            [this]() {
                if (this->window.canvas()->createWireframeBox3D()) {
                    this->window.statusBar()->showMessage(QStringLiteral("Created 3D wireframe box"), 3000);
                    updateEditCommandStates();
                }
            });
    ch->registerCommand(
            {.id = "geometry.project-3d-isometric", .text = "3D Isometric",
             .tooltip = "Project selected 3D geometry to an isometric view",
             .menu = "Tools>3D Geometry", .checkable = true,
             .enabled = this->documentController.selectedGeometry().has_value(),
             .checked = this->window.canvas()->isGeometryProjectionIsometric()},
            [this]() {
                if (this->window.canvas()->projectSelectedGeometry3DIsometric()) {
                    this->window.statusBar()->showMessage(
                            QStringLiteral("3D projection: isometric view of the same geometry"), 4000);
                    updateEditCommandStates();
                }
            });
    ch->registerCommand(
            {.id = "geometry.project-3d-front", .text = "3D Front",
             .tooltip = "Project selected 3D geometry to a front view",
             .menu = "Tools>3D Geometry", .checkable = true,
             .enabled = this->documentController.selectedGeometry().has_value(),
             .checked = this->window.canvas()->isGeometryProjectionFront()},
            [this]() {
                if (this->window.canvas()->projectSelectedGeometry3DFront()) {
                    this->window.statusBar()->showMessage(
                            QStringLiteral("3D projection: front view of the same geometry"), 4000);
                    updateEditCommandStates();
                }
            });
    ch->registerCommand(
            {.id = "geometry.project-3d-top", .text = "3D Top",
             .tooltip = "Project selected 3D geometry to a top view",
             .menu = "Tools>3D Geometry", .checkable = true,
             .enabled = this->documentController.selectedGeometry().has_value(),
             .checked = this->window.canvas()->isGeometryProjectionTop()},
            [this]() {
                if (this->window.canvas()->projectSelectedGeometry3DTop()) {
                    this->window.statusBar()->showMessage(
                            QStringLiteral("3D projection: top view of the same geometry"), 4000);
                    updateEditCommandStates();
                }
            });
    ch->registerCommand(
            {.id = "geometry.nudge-z-up", .text = "Push Z",
             .tooltip = "Move selected geometry vertices forward in 3D depth",
             .shortcut = "Ctrl+Alt+Up", .menu = "Tools>3D Geometry",
             .enabled = this->documentController.selectedGeometry().has_value()},
            [this]() {
                if (this->window.canvas()->nudgeSelectedGeometryZ(12.0)) {
                    this->window.statusBar()->showMessage(QStringLiteral("Depth Z increased; projection refreshed"),
                                                          3000);
                    updateEditCommandStates();
                }
            });
    ch->registerCommand(
            {.id = "geometry.nudge-z-down", .text = "Pull Z",
             .tooltip = "Move selected geometry vertices backward in 3D depth",
             .shortcut = "Ctrl+Alt+Down", .menu = "Tools>3D Geometry",
             .enabled = this->documentController.selectedGeometry().has_value()},
            [this]() {
                if (this->window.canvas()->nudgeSelectedGeometryZ(-12.0)) {
                    this->window.statusBar()->showMessage(QStringLiteral("Depth Z decreased; projection refreshed"),
                                                          3000);
                    updateEditCommandStates();
                }
            });
    ch->addMenuSeparator("Tools>3D Geometry");
    ch->addMenuSeparator("Tools");
    ch->registerCommand(
            {.id = "constraint.coincident", .text = "Coincident", .tooltip = "Merge vertices", .shortcut = "Ctrl+Alt+C",
             .menu = "Tools>Geometry Constraints"},
            [this]() { applyConstraint(vn::geom::ConstraintKind::Coincident); });
    ch->registerCommand(
            {.id = "constraint.horizontal", .text = "Horizontal", .tooltip = "Force horizontal", .shortcut = "Ctrl+Alt+H",
             .menu = "Tools>Geometry Constraints"},
            [this]() { applyConstraint(vn::geom::ConstraintKind::Horizontal); });
    ch->registerCommand(
            {.id = "constraint.vertical", .text = "Vertical", .tooltip = "Force vertical", .shortcut = "Ctrl+Alt+V",
             .menu = "Tools>Geometry Constraints"},
            [this]() { applyConstraint(vn::geom::ConstraintKind::Vertical); });
    ch->registerCommand(
            {.id = "constraint.fixed-length", .text = "Fixed Length", .tooltip = "Set fixed edge length", .shortcut = "Ctrl+Alt+L",
             .menu = "Tools>Geometry Constraints"},
            [this]() { applyConstraint(vn::geom::ConstraintKind::FixedLength); });
    ch->registerCommand(
            {.id = "constraint.equal-length", .text = "Equal Length", .tooltip = "Match selected edge lengths", .shortcut = "Ctrl+Alt+Shift+L",
             .menu = "Tools>Geometry Constraints"},
            [this]() { applyConstraint(vn::geom::ConstraintKind::EqualLength); });
    ch->registerCommand(
            {.id = "constraint.edit-length", .text = "Edit Fixed Length...", .tooltip = "Edit constraint value", .shortcut = "Ctrl+Alt+E",
             .menu = "Tools>Geometry Constraints"},
            [this]() { editFixedLengthConstraint(); });
    ch->registerCommand(
            {.id = "constraint.fixed-angle", .text = "Fixed Angle", .tooltip = "Lock selected edge angle", .shortcut = "Ctrl+Alt+A",
             .menu = "Tools>Geometry Constraints"},
            [this]() { applyConstraint(vn::geom::ConstraintKind::FixedAngle); });
    ch->registerCommand(
            {.id = "constraint.radius", .text = "Radius", .tooltip = "Set fixed radius", .shortcut = "Ctrl+Alt+R",
             .menu = "Tools>Geometry Constraints"},
            [this]() { applyConstraint(vn::geom::ConstraintKind::Radius); });
    ch->registerCommand(
            {.id = "constraint.on-edge", .text = "On Edge", .tooltip = "Constrain vertex onto selected edge", .shortcut = "Ctrl+Alt+O",
             .menu = "Tools>Geometry Constraints"},
            [this]() { applyConstraint(vn::geom::ConstraintKind::OnEdge); });
    ch->registerCommand(
            {.id = "constraint.parallel", .text = "Parallel", .tooltip = "Force parallel edges", .shortcut = "Ctrl+Alt+P",
             .menu = "Tools>Geometry Constraints"},
            [this]() { applyConstraint(vn::geom::ConstraintKind::Parallel); });
    ch->registerCommand(
            {.id = "constraint.perpendicular", .text = "Perpendicular", .tooltip = "Force perpendicular", .shortcut = "Ctrl+Alt+Shift+P",
             .menu = "Tools>Geometry Constraints"},
            [this]() { applyConstraint(vn::geom::ConstraintKind::Perpendicular); });
    ch->registerCommand(
            {.id = "constraint.delete", .text = "Delete Constraints", .tooltip = "Remove constraints", .shortcut = "Ctrl+Alt+Delete",
             .menu = "Tools>Geometry Constraints"},
            [this]() { deleteConstraints(); });
    ch->addMenuSeparator("Tools");

    ch->registerCommand(
            {.id = "edit.insert-vertex", .text = "Insert Vertex on Edge", .tooltip = "Insert vertex on selected edge",
             .shortcut = "Insert", .menu = "Tools"},
            [this]() {
                if (this->window.canvas()->insertVertexOnSelectedEdge()) {
                    this->window.statusBar()->showMessage(QStringLiteral("Inserted geometry vertex"), 3000);
                    updateEditCommandStates();
                }
            });
    ch->registerCommand(
            {.id = "geometry.detach-selection", .text = "Detach Selected Geometry",
             .tooltip = "Separate selected geometry vertices or edges from connected topology",
             .shortcut = "Ctrl+Alt+D", .menu = "Tools",
             .enabled = this->documentController.selectedGeometry().has_value()},
            [this]() {
                if (this->window.canvas()->detachSelectedGeometry()) {
                    this->window.statusBar()->showMessage(QStringLiteral("Detached selected geometry"), 3000);
                    updateEditCommandStates();
                } else {
                    this->window.statusBar()->showMessage(QStringLiteral("No detachable geometry selected"), 3000);
                }
            });
    ch->registerCommand(
            {.id = "geometry.weld-selection", .text = "Weld Selected Vertices",
             .tooltip = "Merge selected geometry vertices into shared topology",
             .shortcut = "Ctrl+Alt+W", .menu = "Tools",
             .enabled = this->documentController.selectedGeometry().has_value()},
            [this]() {
                if (this->window.canvas()->weldSelectedGeometry()) {
                    this->window.statusBar()->showMessage(QStringLiteral("Welded selected vertices"), 3000);
                    updateEditCommandStates();
                } else {
                    this->window.statusBar()->showMessage(QStringLiteral("No weldable vertices selected"), 3000);
                }
            });
    ch->registerCommand(
            {.id = "geometry.fill-face", .text = "Fill Closed Geometry Face",
             .tooltip = "Create a filled surface from the selected closed geometry loop",
             .shortcut = "Ctrl+Alt+F", .menu = "Tools",
             .enabled = this->documentController.selectedGeometry().has_value()},
            [this]() {
                const int fillOpacity = this->window.canvas()->toolState().fillOpacity;
                if (this->window.canvas()->fillSelectedGeometryFace(fillOpacity)) {
                    this->window.statusBar()->showMessage(QStringLiteral("Filled geometry face"), 3000);
                    updateEditCommandStates();
                } else {
                    const auto status = this->documentController.selectedGeometryFaceLoopStatus();
                    this->window.statusBar()->showMessage(QString::fromStdString(status.message), 3000);
                }
            });
    ch->registerCommand(
            {.id = "geometry.delete-face", .text = "Delete Face",
             .tooltip = "Remove the selected filled geometry face without deleting its edges",
             .shortcut = "Ctrl+Alt+Backspace", .menu = "Tools>Face Editing",
             .enabled = !this->documentController.selectedFaceIds().empty()},
            [this]() {
                if (this->window.canvas()->deleteSelectedGeometryFace()) {
                    this->window.statusBar()->showMessage(QStringLiteral("Deleted geometry face"), 3000);
                    updateEditCommandStates();
                } else {
                    this->window.statusBar()->showMessage(QStringLiteral("Select a filled face before Delete Face"),
                                                          3000);
                }
            });
    ch->registerCommand(
            {.id = "geometry.split-face", .text = "Split Face",
             .tooltip = "Split the selected face with a diagonal",
             .menu = "Tools>Face Editing", .enabled = this->documentController.selectedFaceIds().size() == 1U},
            [this]() {
                const auto diagonals = this->documentController.selectedGeometryFaceSplitDiagonals();
                bool changed = false;
                if (diagonals.size() > 1U) {
                    QDialog dialog(&this->window);
                    dialog.setWindowTitle(QStringLiteral("Split Face"));
                    dialog.setObjectName(QStringLiteral("vertexNoteQtSplitFaceDialog"));
                    auto* layout = new QVBoxLayout(&dialog);
                    layout->setContentsMargins(10, 10, 10, 10);
                    layout->setSpacing(8);
                    auto* label = new QLabel(QStringLiteral("Diagonal"), &dialog);
                    auto* list = new QListWidget(&dialog);
                    list->setObjectName(QStringLiteral("vertexNoteQtSplitFaceDiagonalList"));
                    for (std::size_t index = 0U; index < diagonals.size(); ++index) {
                        const auto& diagonal = diagonals[index];
                        list->addItem(QStringLiteral("v%1 - v%2").arg(diagonal.lhsIndex + 1U).arg(diagonal.rhsIndex + 1U));
                    }
                    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
                    layout->addWidget(label);
                    layout->addWidget(list);
                    layout->addWidget(buttons);
                    QObject::connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
                    QObject::connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
                    QObject::connect(list, &QListWidget::currentRowChanged, &dialog, [this, diagonals](int row) {
                        if (row >= 0 && static_cast<std::size_t>(row) < diagonals.size()) {
                            this->window.canvas()->setGeometryFaceSplitPreview(diagonals[static_cast<std::size_t>(row)]);
                        }
                    });
                    list->setCurrentRow(0);
                    if (dialog.exec() != QDialog::Accepted) {
                        this->window.canvas()->clearGeometryFaceSplitPreview();
                        return;
                    }
                    const auto chosenIndex = list->currentRow();
                    this->window.canvas()->clearGeometryFaceSplitPreview();
                    if (chosenIndex >= 0 && static_cast<std::size_t>(chosenIndex) < diagonals.size()) {
                        const auto& diagonal = diagonals[static_cast<std::size_t>(chosenIndex)];
                        changed = this->window.canvas()->splitSelectedGeometryFace(diagonal.lhsIndex,
                                                                                   diagonal.rhsIndex);
                    }
                } else {
                    changed = this->window.canvas()->splitSelectedGeometryFace();
                }

                if (changed) {
                    this->window.statusBar()->showMessage(QStringLiteral("Split geometry face"), 3000);
                    updateEditCommandStates();
                } else {
                    this->window.statusBar()->showMessage(
                            QStringLiteral("Split needs one selected face with four or more vertices"), 3000);
                }
            });
    ch->registerCommand(
            {.id = "geometry.triangulate-face", .text = "Triangulate Face",
             .tooltip = "Triangulate the selected face into editable triangle faces",
             .menu = "Tools>Face Editing", .enabled = this->documentController.selectedFaceIds().size() == 1U},
            [this]() {
                if (this->window.canvas()->triangulateSelectedGeometryFace()) {
                    this->window.statusBar()->showMessage(QStringLiteral("Triangulated geometry face"), 3000);
                    updateEditCommandStates();
                } else {
                    this->window.statusBar()->showMessage(
                            QStringLiteral("Triangulate needs one selected filled face"), 3000);
                }
            });
    ch->registerCommand(
            {.id = "edit.delete-geometry", .text = "Delete Selected Geometry", .tooltip = "Delete selected geometry",
             .menu = "Tools"},
            [this]() {
                if (this->window.canvas()->deleteSelectedGeometry()) {
                    this->window.statusBar()->showMessage(QStringLiteral("Deleted selected geometry"), 3000);
                    updateEditCommandStates();
                }
            });

}
