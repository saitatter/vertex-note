/*
 * VertexNote
 *
 * Qt app shell command registration.
 */

#include "QtAppShell.h"

#include <string>

#include <QDesktopServices>
#include <QInputDialog>
#include <QStatusBar>
#include <QString>
#include <QUrl>
void QtAppShell::registerViewCommands() {
    auto* ch = this->window.commandHost();

    // =====================================================================
    // Menu 3: View
    // =====================================================================
    ch->registerCommand(
            {.id = "view.paired-pages", .text = "Pair Pages", .tooltip = "Display pages side by side",
             .menu = "View", .checkable = true},
            [this]() { togglePairedPages(); });
    ch->registerCommand(
            {.id = "view.presentation", .text = "Presentation Mode", .tooltip = "Fullscreen presentation",
             .shortcut = "F5", .menu = "View", .checkable = true},
            [this]() { togglePresentationMode(); });
    ch->registerCommand(
            {.id = "view.fullscreen", .text = "Fullscreen", .tooltip = "Toggle fullscreen",
             .shortcut = "F11", .menu = "View", .checkable = true},
            [this]() { toggleFullscreen(); });
    ch->addMenuSeparator("View");
    ch->registerCommand(
            {.id = "view.show-toolbar", .text = "Show Toolbars", .tooltip = "Toggle toolbar visibility",
             .shortcut = "F9", .menu = "View", .checkable = true, .checked = true},
            [this]() { toggleToolbarVisibility(); });
    ch->registerCommand(
            {.id = "view.customize-toolbar", .text = "Customize Toolbars...", .tooltip = "Edit the Qt toolbar profile",
             .menu = "View>Toolbars"},
            [this]() { showToolbarCustomizeDialog(); });
    ch->registerCommand(
            {.id = "view.show-menubar", .text = "Show Menubar", .tooltip = "Toggle menubar visibility",
             .shortcut = "F10", .menu = "View", .checkable = true, .checked = true},
            [this]() { toggleMenubarVisibility(); });
    ch->registerCommand(
            {.id = "view.show-sidebar", .text = "Show Sidebar", .tooltip = "Toggle sidebar visibility",
             .shortcut = "F12", .menu = "View", .checkable = true, .checked = true},
            [this]() { toggleSidebarVisibility(); });
    ch->registerCommand(
            {.id = "view.show-geometry-panel", .text = "Workspace Panel", .tooltip = "Toggle the active workspace panel",
             .menu = "View", .checkable = true, .checked = true},
            [this]() {
                applyGeometryPanelVisibility(!this->window.geometryPanel()->isVisible());
                savePersistentUiState();
            });
    ch->addMenuSeparator("View");

    ch->registerCommand(
            {.id = "view.workspace-notes", .text = "Write Workspace",
             .tooltip = "Use the writing and note-taking workspace",
             .menu = "View>Workspace", .checkable = true,
             .checked = activeWorkspaceId() == "notes"},
            [this]() { applyWorkspace("notes"); });
    ch->registerCommand(
            {.id = "view.workspace-geometry", .text = "Geometry Workspace",
             .tooltip = "Use a focused geometry editing layout",
             .menu = "View>Workspace", .checkable = true,
             .checked = activeWorkspaceId() == "geometry"},
            [this]() { applyWorkspace("geometry"); });
    ch->registerCommand(
            {.id = "view.workspace-3d", .text = "3D Workspace",
             .tooltip = "Use a focused 3D geometry layout",
             .menu = "View>Workspace", .checkable = true,
             .checked = activeWorkspaceId() == "3d"},
            [this]() { applyWorkspace("3d"); });
    ch->addMenuSeparator("View");

    ch->registerCommand(
            {.id = "view.geometry-wireframe", .text = "Wireframe",
             .tooltip = "Show geometry faces as wireframe outlines",
             .menu = "View>Geometry View", .checkable = true,
             .checked = this->window.canvas()->isGeometryWireframeViewEnabled()},
            [this]() {
                setGeometryWireframeViewEnabled(!this->window.canvas()->isGeometryWireframeViewEnabled());
            });
    ch->registerCommand(
            {.id = "view.geometry-highlight-vertices", .text = "Vertex Handles",
             .tooltip = "Show geometry vertex handles without needing hover",
             .menu = "View>Geometry View", .checkable = true,
             .checked = this->window.canvas()->isGeometryVertexOverlayEnabled()},
            [this]() {
                setGeometryVertexOverlayEnabled(!this->window.canvas()->isGeometryVertexOverlayEnabled());
            });
    ch->registerCommand(
            {.id = "view.geometry-linked-markers", .text = "Linked Markers",
             .tooltip = "Show markers for welded or shared geometry vertices",
             .menu = "View>Geometry View", .checkable = true,
             .checked = this->window.canvas()->isGeometryLinkedVertexOverlayEnabled()},
            [this]() {
                setGeometryLinkedVertexOverlayEnabled(!this->window.canvas()->isGeometryLinkedVertexOverlayEnabled());
            });
    ch->registerCommand(
            {.id = "view.geometry-face-fills", .text = "Face Fills",
             .tooltip = "Show filled geometry mesh and surface faces",
             .menu = "View>Geometry View", .checkable = true,
             .checked = this->window.canvas()->isGeometryFaceFillVisible()},
            [this]() { setGeometryFaceFillVisible(!this->window.canvas()->isGeometryFaceFillVisible()); });
    ch->addMenuSeparator("View");

    // Layout submenu
    ch->registerCommand(
            {.id = "view.layout-horizontal", .text = "Horizontal", .tooltip = "Horizontal page layout",
             .menu = "View>Layout", .checkable = true, .checked = true},
            [this]() { setLayoutVertical(false); });
    ch->registerCommand(
            {.id = "view.layout-vertical", .text = "Vertical", .tooltip = "Vertical page layout",
             .menu = "View>Layout", .checkable = true},
            [this]() { setLayoutVertical(true); });
    ch->addMenuSeparator("View>Layout");
    ch->registerCommand(
            {.id = "view.layout-ltr", .text = "Left to Right", .tooltip = "Left to right reading order",
             .menu = "View>Layout", .checkable = true, .checked = true},
            [this]() { setLayoutRtl(false); });
    ch->registerCommand(
            {.id = "view.layout-rtl", .text = "Right to Left", .tooltip = "Right to left reading order",
             .menu = "View>Layout", .checkable = true},
            [this]() { setLayoutRtl(true); });
    ch->addMenuSeparator("View>Layout");
    ch->registerCommand(
            {.id = "view.layout-ttb", .text = "Top to Bottom", .tooltip = "Top to bottom page order",
             .menu = "View>Layout", .checkable = true, .checked = true},
            [this]() { setLayoutBtt(false); });
    ch->registerCommand(
            {.id = "view.layout-btt", .text = "Bottom to Top", .tooltip = "Bottom to top page order",
             .menu = "View>Layout", .checkable = true},
            [this]() { setLayoutBtt(true); });
    ch->addMenuSeparator("View>Layout");
    for (int columns = 1; columns <= 8; ++columns) {
        ch->registerCommand(
                {.id = "view.columns-" + std::to_string(columns),
                 .text = std::to_string(columns) + (columns == 1 ? " Column" : " Columns"),
                 .tooltip = "Use a fixed number of page columns",
                 .menu = "View>Layout>Columns",
                 .checkable = true},
                [this, columns]() { setLayoutColumns(columns); });
    }
    for (int rows = 1; rows <= 8; ++rows) {
        ch->registerCommand(
                {.id = "view.rows-" + std::to_string(rows),
                 .text = std::to_string(rows) + (rows == 1 ? " Row" : " Rows"),
                 .tooltip = "Use a fixed number of page rows",
                 .menu = "View>Layout>Rows",
                 .checkable = true},
                [this, rows]() { setLayoutRows(rows); });
    }
    ch->addMenuSeparator("View>Layout");
    for (int offset = 0; offset <= 1; ++offset) {
        ch->registerCommand(
                {.id = "view.pair-offset-" + std::to_string(offset),
                 .text = "Pair Offset " + std::to_string(offset),
                 .tooltip = "Offset paired pages before grouping",
                 .menu = "View>Layout>Pair Offset",
                 .checkable = true},
                [this, offset]() { setPairOffset(offset); });
    }

    ch->addMenuSeparator("View");
    ch->registerCommand(
            {.id = "view.zoom-in", .text = "Zoom In", .tooltip = "Zoom in", .shortcut = "Ctrl+=", .menu = "View"},
            [this]() { this->window.canvas()->zoomIn(); });
    ch->registerCommand(
            {.id = "view.zoom-out", .text = "Zoom Out", .tooltip = "Zoom out", .shortcut = "Ctrl+-", .menu = "View"},
            [this]() { this->window.canvas()->zoomOut(); });
    ch->registerCommand(
            {.id = "view.zoom-100", .text = "Normal Size", .tooltip = "Zoom to 100%", .shortcut = "Ctrl+1", .menu = "View"},
            [this]() { this->window.canvas()->zoomToActualSize(); this->window.canvas()->update(); });
    ch->registerCommand(
            {.id = "view.fit-page", .text = "Zoom to Fit", .tooltip = "Fit page in window", .shortcut = "Ctrl+9", .menu = "View"},
            [this]() { this->window.canvas()->fitPage(); });
    ch->registerCommand(
            {.id = "view.fit-width", .text = "Fit Width", .tooltip = "Fit page width", .shortcut = "Ctrl+8", .menu = "View"},
            [this]() { this->window.canvas()->fitWidth(); this->window.canvas()->update(); });
    ch->registerCommand(
            {.id = "view.zoom-reset", .text = "Reset View", .tooltip = "Reset viewport", .shortcut = "Ctrl+0", .menu = "View"},
            [this]() { this->window.canvas()->resetViewport(); });

}
