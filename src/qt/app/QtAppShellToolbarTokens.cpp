/*
 * VertexNote
 *
 * Qt app shell toolbar token dispatch.
 */

#include "QtAppShell.h"

#include <initializer_list>
#include <string>
#include <string_view>
#include <vector>

#include <QComboBox>
#include <QDoubleSpinBox>
#include <QFontComboBox>
#include <QSlider>
#include <QSpinBox>
#include <QToolBar>
#include <QToolButton>

#include "QtIconResources.h"

auto QtAppShell::toolbarItemsFor(std::string_view key, std::initializer_list<std::string_view> fallback) const
        -> std::vector<std::string> {
    if (this->activeToolbarProfile) {
        if (const auto* items = this->activeToolbarProfile->itemsFor(key)) {
            return *items;
        }

        return {};
    }

    std::vector<std::string> items;
    items.reserve(fallback.size());
    for (const auto token: fallback) {
        items.emplace_back(token);
    }
    return items;
}

void QtAppShell::addToolbarToken(QToolBar* toolbar, std::string_view rawToken) {
    const std::string token(rawToken);
    if (token == "SEPARATOR") {
        toolbar->addSeparator();
        return;
    }
    if (token == "SPACER") {
        addStretchToolbarSpacer(toolbar);
        return;
    }
    if (token == "GROUP_FILE") { addToolbarGroupLabel(toolbar, "File"); return; }
    if (token == "GROUP_EDIT") { addToolbarGroupLabel(toolbar, "Edit"); return; }
    if (token == "GROUP_NAV") { addToolbarGroupLabel(toolbar, "Nav"); return; }
    if (token == "GROUP_PAGE") { addToolbarGroupLabel(toolbar, "Page"); return; }
    if (token == "GROUP_TOOLS") { addToolbarGroupLabel(toolbar, "Tools"); return; }
    if (token == "GROUP_INSERT") { addToolbarGroupLabel(toolbar, "Insert"); return; }
    if (token == "GROUP_DRAW") { addToolbarGroupLabel(toolbar, "Draw"); return; }
    if (token == "GROUP_GEOMETRY") { addToolbarGroupLabel(toolbar, "Geometry"); return; }
    if (token == "GROUP_TOPOLOGY") { addToolbarGroupLabel(toolbar, "Topology"); return; }
    if (token == "GROUP_FACES") { addToolbarGroupLabel(toolbar, "Faces"); return; }
    if (token == "GROUP_3D") { addToolbarGroupLabel(toolbar, "3D"); return; }
    if (token == "GROUP_VIEW") { addToolbarGroupLabel(toolbar, "View"); return; }
    if (token == "GROUP_SNAP") { addToolbarGroupLabel(toolbar, "Snap"); return; }
    if (token == "GROUP_STYLE") { addToolbarGroupLabel(toolbar, "Style"); return; }
    if (token == "GROUP_COLOR") { addToolbarGroupLabel(toolbar, "Color"); return; }
    if (token == "WORKSPACE") {
        toolbar->addWidget(ensureWorkspaceCombo());
        return;
    }
    if (token == "SAVE") { addToolbarCommand(toolbar, "file.save"); return; }
    if (token == "NEW") { addToolbarCommand(toolbar, "app.new"); return; }
    if (token == "OPEN") { addToolbarCommand(toolbar, "app.open"); return; }
    if (token == "SAVEPDF") { addToolbarCommand(toolbar, "export.pdf"); return; }
    if (token == "PRINT") { addToolbarCommand(toolbar, "file.print"); return; }
    if (token == "CUT") { addToolbarCommand(toolbar, "edit.cut"); return; }
    if (token == "COPY") { addToolbarCommand(toolbar, "edit.copy"); return; }
    if (token == "PASTE") { addToolbarCommand(toolbar, "edit.paste"); return; }
    if (token == "SEARCH") { addToolbarCommand(toolbar, "edit.find"); return; }
    if (token == "DELETE") { addToolbarCommand(toolbar, "edit.delete"); return; }
    if (token == "UNDO") { addToolbarCommand(toolbar, "edit.undo-geometry"); return; }
    if (token == "REDO") { addToolbarCommand(toolbar, "edit.redo-geometry"); return; }
    if (token == "GOTO_FIRST") { addToolbarCommand(toolbar, "nav.first-page"); return; }
    if (token == "GOTO_BACK") { addToolbarCommand(toolbar, "nav.prev-page"); return; }
    if (token == "NAVIGATE_BACK") { addToolbarCommand(toolbar, "nav.back"); return; }
    if (token == "NAVIGATE_FORWARD") { addToolbarCommand(toolbar, "nav.forward"); return; }
    if (token == "GOTO_NEXT_ANNOTATED_PAGE") { addToolbarCommand(toolbar, "nav.next-annotated"); return; }
    if (token == "GOTO_NEXT") { addToolbarCommand(toolbar, "nav.next-page"); return; }
    if (token == "GOTO_LAST") { addToolbarCommand(toolbar, "nav.last-page"); return; }
    if (token == "INSERT_NEW_PAGE") { addToolbarCommand(toolbar, "page.add"); return; }
    if (token == "DELETE_CURRENT_PAGE") { addToolbarCommand(toolbar, "page.delete"); return; }
    if (token == "FULLSCREEN") { addToolbarCommand(toolbar, "view.fullscreen"); return; }
    if (token == "AUDIO_RECORDING") { addToolbarCommand(toolbar, "audio.record"); return; }
    if (token == "AUDIO_SEEK_BACKWARDS") { addToolbarCommand(toolbar, "audio.seek-backwards"); return; }
    if (token == "AUDIO_PAUSE_PLAYBACK") { addToolbarCommand(toolbar, "audio.pause-playback"); return; }
    if (token == "AUDIO_SEEK_FORWARDS") { addToolbarCommand(toolbar, "audio.seek-forwards"); return; }
    if (token == "AUDIO_STOP_PLAYBACK") { addToolbarCommand(toolbar, "audio.stop-playback"); return; }
    if (token == "SELECT_FONT") {
        ensureFontToolbarWidgets();
        toolbar->addWidget(this->fontFamilyCombo);
        toolbar->addWidget(this->fontSizeSpinner);
        return;
    }
    if (token == "PEN") { addToolbarCommand(toolbar, "tool.pen"); return; }
    if (token == "PLAIN") { addToolbarCommand(toolbar, "pen.line-solid"); return; }
    if (token == "DASHED") { addToolbarCommand(toolbar, "pen.line-dash"); return; }
    if (token == "DASH-/ DOTTED" || token == "DASH-DOTTED") { addToolbarCommand(toolbar, "pen.line-dashdot"); return; }
    if (token == "DOTTED") { addToolbarCommand(toolbar, "pen.line-dot"); return; }
    if (token == "ERASER") { addToolbarCommand(toolbar, "tool.eraser"); return; }
    if (token == "HIGHLIGHTER" || token == "HILIGHTER") { addToolbarCommand(toolbar, "tool.highlighter"); return; }
    if (token == "LASER_POINTER") {
        toolbar->addWidget(ensureLaserToolButton());
        return;
    }
    if (token == "IMAGE") { addToolbarCommand(toolbar, "edit.insert-image"); return; }
    if (token == "TEXT") { addToolbarCommand(toolbar, "tool.text"); return; }
    if (token == "MATH_TEX") {
        addToolbarCommand(toolbar, "tool.math-tex");
        return;
    }
    if (token == "DRAW") {
        toolbar->addWidget(createStrokeDrawingToolButton());
        toolbar->addWidget(createVertexDrawingToolButton());
        return;
    }
    if (token == "DRAW_STROKE") { toolbar->addWidget(createStrokeDrawingToolButton()); return; }
    if (token == "DRAW_VERTEX") { toolbar->addWidget(createVertexDrawingToolButton()); return; }
    if (token == "GEOMETRY_TRANSFORM" || token == "VERTEX_TRANSFORM") {
        toolbar->addWidget(createGeometryTransformToolButton());
        return;
    }
    if (token == "ROTATION_SNAPPING") {
        addToolbarCommand(toolbar, "view.toggle-rotation-snap");
        return;
    }
    if (token == "GRID_SNAPPING") {
        addToolbarCommand(toolbar, "view.toggle-grid-snap");
        return;
    }
    if (token == "VERTEXNOTE_GEOMETRY_SNAPPING") { addToolbarCommand(toolbar, "view.toggle-geometry-snap"); return; }
    if (token == "VERTEXNOTE_GRID_SNAPPING") { addToolbarCommand(toolbar, "view.toggle-grid-snap"); return; }
    if (token == "SHOW_SIDEBAR") { addToolbarCommand(toolbar, "view.show-sidebar"); return; }
    if (token == "TOGGLE_TOUCH_DRAWING") {
        addToolbarCommand(toolbar, "view.toggle-touch-drawing");
        return;
    }
    if (token == "SELECT") { toolbar->addWidget(ensureSelectionToolButton()); return; }
    if (token == "VERTICAL_SPACE") { addToolbarCommand(toolbar, "tool.vertical-space"); return; }
    if (token == "HAND") { addToolbarCommand(toolbar, "tool.hand"); return; }
    if (token == "SETSQUARE") {
        addToolbarCommand(toolbar, "tool.setsquare");
        return;
    }
    if (token == "COMPASS") {
        addToolbarCommand(toolbar, "tool.compass");
        return;
    }
    if (token == "DEFAULT_TOOL") {
        addToolbarCommand(toolbar, "tool.default-preset");
        return;
    }
    if (token == "MANAGE_TOOLBAR") {
        addToolbarCommand(toolbar, "app.settings");
        return;
    }
    if (token == "CUSTOMIZE_TOOLBAR") {
        addToolbarCommand(toolbar, "view.customize-toolbar");
        return;
    }
    if (token == "GOTO_PAGE") { addToolbarCommand(toolbar, "nav.goto-page"); return; }
    if (token == "SELECT_PDF_TEXT_LINEAR") {
        toolbar->addWidget(ensurePdfToolButton());
        return;
    }
    if (token == "PDF_TOOL") {
        toolbar->addWidget(ensurePdfToolButton());
        return;
    }
    if (token == "SELECT_PDF_TEXT_RECT") {
        toolbar->addWidget(ensurePdfToolButton());
        return;
    }
    if (token == "SHAPE_RECOGNIZER") {
        addToolbarCommand(toolbar, "tool.draw-shape-recognizer");
        return;
    }
    if (token == "DRAW_RECTANGLE") { addToolbarCommand(toolbar, "tool.draw-rectangle"); return; }
    if (token == "DRAW_ELLIPSE") { addToolbarCommand(toolbar, "tool.draw-ellipse"); return; }
    if (token == "DRAW_ARROW") { addToolbarCommand(toolbar, "tool.draw-arrow"); return; }
    if (token == "DRAW_DOUBLE_ARROW") { addToolbarCommand(toolbar, "tool.draw-double-arrow"); return; }
    if (token == "DRAW_COORDINATE_SYSTEM") { addToolbarCommand(toolbar, "tool.draw-coordinate-system"); return; }
    if (token == "DRAW_LINE") { addToolbarCommand(toolbar, "tool.draw-line"); return; }
    if (token == "DRAW_EDGE") { addToolbarCommand(toolbar, "tool.draw-edge"); return; }
    if (token == "RULER") {
        addToolbarCommand(toolbar, "tool.draw-line");
        return;
    }
    if (token == "DRAW_SPLINE") { addToolbarCommand(toolbar, "tool.draw-spline"); return; }
    if (token == "DRAW_POLYLINE") { addToolbarCommand(toolbar, "tool.draw-polyline"); return; }
    if (token == "SELECT_REGION") { addToolbarCommand(toolbar, "tool.select-region"); return; }
    if (token == "SELECT_RECTANGLE") { addToolbarCommand(toolbar, "tool.select"); return; }
    if (token == "SELECT_MULTILAYER_REGION") { addToolbarCommand(toolbar, "tool.select-multilayer-region"); return; }
    if (token == "SELECT_MULTILAYER_RECTANGLE") { addToolbarCommand(toolbar, "tool.select-multilayer-rect"); return; }
    if (token == "SELECT_OBJECT") { addToolbarCommand(toolbar, "tool.select-object"); return; }
    if (token == "PLAY_OBJECT") { addToolbarCommand(toolbar, "audio.play-object"); return; }
    if (token == "GOTO_PREVIOUS_LAYER") { addToolbarCommand(toolbar, "layer.goto-prev"); return; }
    if (token == "GOTO_NEXT_LAYER") { addToolbarCommand(toolbar, "layer.goto-next"); return; }
    if (token == "GOTO_TOP_LAYER") { addToolbarCommand(toolbar, "layer.goto-top"); return; }
    if (token == "FILL_OPACITY" || token == "PEN_FILL_OPACITY") {
        toolbar->addWidget(createStaticIconWidget(toolbar, "xopp-fill-opacity.svg", "Fill opacity"));
        toolbar->addWidget(ensureFillOpacityWidget());
        return;
    }
    if (token == "CONSTRAINT_COINCIDENT") { addToolbarCommand(toolbar, "constraint.coincident"); return; }
    if (token == "CONSTRAINT_HORIZONTAL") { addToolbarCommand(toolbar, "constraint.horizontal"); return; }
    if (token == "CONSTRAINT_VERTICAL") { addToolbarCommand(toolbar, "constraint.vertical"); return; }
    if (token == "CONSTRAINT_FIXED_LENGTH") { addToolbarCommand(toolbar, "constraint.fixed-length"); return; }
    if (token == "CONSTRAINT_EDIT_FIXED_LENGTH") { addToolbarCommand(toolbar, "constraint.edit-length"); return; }
    if (token == "CONSTRAINT_PARALLEL") { addToolbarCommand(toolbar, "constraint.parallel"); return; }
    if (token == "CONSTRAINT_PERPENDICULAR") { addToolbarCommand(toolbar, "constraint.perpendicular"); return; }
    if (token == "CONSTRAINT_DELETE") { addToolbarCommand(toolbar, "constraint.delete"); return; }
    if (token == "GEOMETRY_SELECT_VERTEX") { addToolbarCommand(toolbar, "geometry.selection-mode-vertex"); return; }
    if (token == "GEOMETRY_SELECT_EDGE") { addToolbarCommand(toolbar, "geometry.selection-mode-edge"); return; }
    if (token == "GEOMETRY_SELECT_FACE") { addToolbarCommand(toolbar, "geometry.selection-mode-face"); return; }
    if (token == "GEOMETRY_SELECT_OBJECT") { addToolbarCommand(toolbar, "geometry.selection-mode-object"); return; }
    if (token == "GEOMETRY_TRANSLATE") { addToolbarCommand(toolbar, "geometry.translate-vertices"); return; }
    if (token == "GEOMETRY_ROTATE") { addToolbarCommand(toolbar, "geometry.rotate-selection"); return; }
    if (token == "GEOMETRY_SCALE") { addToolbarCommand(toolbar, "geometry.scale-selection"); return; }
    if (token == "GEOMETRY_WELD") { addToolbarCommand(toolbar, "geometry.weld-selection"); return; }
    if (token == "GEOMETRY_DETACH") { addToolbarCommand(toolbar, "geometry.detach-selection"); return; }
    if (token == "GEOMETRY_FILL_FACE") { addToolbarCommand(toolbar, "geometry.fill-face"); return; }
    if (token == "GEOMETRY_DELETE_FACE") { addToolbarCommand(toolbar, "geometry.delete-face"); return; }
    if (token == "GEOMETRY_SPLIT_FACE") { addToolbarCommand(toolbar, "geometry.split-face"); return; }
    if (token == "GEOMETRY_TRIANGULATE_FACE") { addToolbarCommand(toolbar, "geometry.triangulate-face"); return; }
    if (token == "GEOMETRY_3D_BOX") { addToolbarCommand(toolbar, "geometry.create-3d-box"); return; }
    if (token == "GEOMETRY_PROJECT_ISO") { addToolbarCommand(toolbar, "geometry.project-3d-isometric"); return; }
    if (token == "GEOMETRY_PROJECT_FRONT") { addToolbarCommand(toolbar, "geometry.project-3d-front"); return; }
    if (token == "GEOMETRY_PROJECT_TOP") { addToolbarCommand(toolbar, "geometry.project-3d-top"); return; }
    if (token == "GEOMETRY_Z_UP") { addToolbarCommand(toolbar, "geometry.nudge-z-up"); return; }
    if (token == "GEOMETRY_Z_DOWN") { addToolbarCommand(toolbar, "geometry.nudge-z-down"); return; }
    if (token == "GEOMETRY_PANEL") { addToolbarCommand(toolbar, "view.show-geometry-panel"); return; }
    if (token == "GEOMETRY_VIEW_WIREFRAME") { addToolbarCommand(toolbar, "view.geometry-wireframe"); return; }
    if (token == "GEOMETRY_VIEW_VERTICES") { addToolbarCommand(toolbar, "view.geometry-highlight-vertices"); return; }
    if (token == "GEOMETRY_VIEW_LINKED") { addToolbarCommand(toolbar, "view.geometry-linked-markers"); return; }
    if (token == "GEOMETRY_VIEW_FACES") { addToolbarCommand(toolbar, "view.geometry-face-fills"); return; }
    if (token == "VERY_FINE") { addGenericSizeToolbarAction(toolbar, "Very Fine", "xopp-thickness-finer.svg", 0); return; }
    if (token == "FINE") { addGenericSizeToolbarAction(toolbar, "Fine", "xopp-thickness-fine.svg", 1); return; }
    if (token == "MEDIUM") { addGenericSizeToolbarAction(toolbar, "Medium", "xopp-thickness-medium.svg", 2); return; }
    if (token == "THICK") { addGenericSizeToolbarAction(toolbar, "Thick", "xopp-thickness-thick.svg", 3); return; }
    if (token == "VERY_THICK") { addGenericSizeToolbarAction(toolbar, "Very Thick", "xopp-thickness-thicker.svg", 4); return; }
    if (token == "TOOL_FILL") { addFillToolbarAction(toolbar); return; }
    if (token == "PAGE_SPIN") {
        toolbar->addWidget(createStaticIconWidget(toolbar, "xopp-page-spinner.svg", "Page number"));
        toolbar->addWidget(this->window.footerPageSpin());
        return;
    }
    if (token == "LAYER") {
        toolbar->addWidget(createStaticIconWidget(toolbar, "xopp-combo-layer.svg", "Layer selector"));
        toolbar->addWidget(this->window.footerLayerCombo());
        return;
    }
    if (token == "PAIRED_PAGES") { addToolbarCommand(toolbar, "view.paired-pages"); return; }
    if (token == "PRESENTATION_MODE") { addToolbarCommand(toolbar, "view.presentation"); return; }
    if (token == "ZOOM_100") { addToolbarCommand(toolbar, "view.zoom-100"); return; }
    if (token == "ZOOM_FIT") { addToolbarCommand(toolbar, "view.fit-page"); return; }
    if (token == "ZOOM_OUT") { addToolbarCommand(toolbar, "view.zoom-out"); return; }
    if (token == "ZOOM_SLIDER") {
        toolbar->addWidget(createStaticIconWidget(toolbar, "xopp-zoom-slider.svg", "Zoom"));
        toolbar->addWidget(this->window.footerZoomSlider());
        return;
    }
    if (token == "ZOOM_IN") { addToolbarCommand(toolbar, "view.zoom-in"); return; }
    if (token == "COLOR_SELECT") {
        toolbar->addWidget(ensureToolbarColorSelectButton());
        return;
    }
    if (token.rfind("COLOR(", 0) == 0 && token.back() == ')') {
        const auto number = token.substr(6, token.size() - 7);
        const auto index = std::stoi(number);
        if (index >= 0) {
            toolbar->addWidget(makeToolbarColorButton(index));
        }
        return;
    }
}
