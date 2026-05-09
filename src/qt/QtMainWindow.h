/*
 * VertexNote
 *
 * Qt main window bootstrap.
 */

#pragma once

#include <QLabel>
#include <QMainWindow>

#include "QtCanvas.h"
#include "QtCommandHost.h"
#include "QtLayerPanel.h"
#include "QtPageSidebar.h"
#include "QtToolPalette.h"

class QToolBar;

class QtMainWindow: public QMainWindow {
public:
    QtMainWindow();

    [[nodiscard]] auto canvas() -> QtCanvas*;
    [[nodiscard]] auto commandHost() -> QtCommandHost*;
    [[nodiscard]] auto mainToolBar() -> QToolBar*;
    [[nodiscard]] auto layerPanel() -> QtLayerPanel*;
    [[nodiscard]] auto pageSidebar() -> QtPageSidebar*;
    [[nodiscard]] auto toolPalette() -> QtToolPalette*;
    [[nodiscard]] auto pageStatusLabel() -> QLabel*;
    [[nodiscard]] auto layerStatusLabel() -> QLabel*;
    [[nodiscard]] auto zoomStatusLabel() -> QLabel*;

private:
    QtCanvas* canvasWidget = nullptr;
    QToolBar* toolBar = nullptr;
    QtLayerPanel* layerPanelWidget = nullptr;
    QtPageSidebar* pageSidebarWidget = nullptr;
    QtToolPalette* toolPaletteWidget = nullptr;
    QtCommandHost commandRegistry;
    QLabel* pageLabel = nullptr;
    QLabel* layerLabel = nullptr;
    QLabel* zoomLabel = nullptr;
};
