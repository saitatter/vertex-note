/*
 * VertexNote
 *
 * Qt main window bootstrap.
 */

#pragma once

#include <QLabel>
#include <QMainWindow>
#include <vector>

#include "QtCanvas.h"
#include "QtCommandHost.h"
#include "QtGeometryPanel.h"
#include "QtLayerPanel.h"
#include "QtPageSidebar.h"
#include "QtToolPalette.h"

class QToolBar;
class QComboBox;
class QSlider;
class QSpinBox;

class QtMainWindow: public QMainWindow {
public:
    QtMainWindow();

    [[nodiscard]] auto canvas() -> QtCanvas*;
    [[nodiscard]] auto canvas() const -> const QtCanvas*;
    [[nodiscard]] auto commandHost() -> QtCommandHost*;
    [[nodiscard]] auto commandHost() const -> const QtCommandHost*;
    [[nodiscard]] auto mainToolBar() -> QToolBar*;
    [[nodiscard]] auto toolsToolBar() -> QToolBar*;
    [[nodiscard]] auto footerToolBar() -> QToolBar*;
    [[nodiscard]] auto leftPrimaryToolBar() -> QToolBar*;
    [[nodiscard]] auto leftSecondaryToolBar() -> QToolBar*;
    [[nodiscard]] auto rightPrimaryToolBar() -> QToolBar*;
    [[nodiscard]] auto floatingToolBars() const -> const std::vector<QToolBar*>&;
    [[nodiscard]] auto layerPanel() -> QtLayerPanel*;
    [[nodiscard]] auto pageSidebar() -> QtPageSidebar*;
    [[nodiscard]] auto geometryPanel() -> QtGeometryPanel*;
    [[nodiscard]] auto toolPalette() -> QtToolPalette*;
    [[nodiscard]] auto footerPageSpin() -> QSpinBox*;
    [[nodiscard]] auto footerLayerCombo() -> QComboBox*;
    [[nodiscard]] auto footerZoomSlider() -> QSlider*;
    [[nodiscard]] auto pageStatusLabel() -> QLabel*;
    [[nodiscard]] auto layerStatusLabel() -> QLabel*;
    [[nodiscard]] auto zoomStatusLabel() -> QLabel*;
    [[nodiscard]] auto geometryStatusLabel() -> QLabel*;
    void cascadeFloatingToolBars();
    void setGtkParitySidebarMode(bool enabled);
    void setSidebarPreferences(int width, bool onRight, int numberingStyle, int scrollbarHideType, bool scrollbarOnLeft,
                               bool disableScrollbarFadeout);

private:
    [[nodiscard]] auto preferredSidebarArea() const -> Qt::DockWidgetArea;

private:
    QtCanvas* canvasWidget = nullptr;
    QToolBar* documentToolBar = nullptr;
    QToolBar* toolsToolBarWidget = nullptr;
    QToolBar* footerToolBarWidget = nullptr;
    QToolBar* leftPrimaryToolBarWidget = nullptr;
    QToolBar* leftSecondaryToolBarWidget = nullptr;
    QToolBar* rightPrimaryToolBarWidget = nullptr;
    std::vector<QToolBar*> floatingToolBarWidgets;
    QtLayerPanel* layerPanelWidget = nullptr;
    QtPageSidebar* pageSidebarWidget = nullptr;
    QtGeometryPanel* geometryPanelWidget = nullptr;
    QtToolPalette* toolPaletteWidget = nullptr;
    QtCommandHost commandRegistry;
    QSpinBox* footerPageSpinWidget = nullptr;
    QComboBox* footerLayerComboWidget = nullptr;
    QSlider* footerZoomSliderWidget = nullptr;
    QLabel* pageLabel = nullptr;
    QLabel* layerLabel = nullptr;
    QLabel* zoomLabel = nullptr;
    QLabel* geometryLabel = nullptr;
    int sidebarPreferredWidth = 90;
    bool sidebarRightSide = false;
    int sidebarNumberingStyle = 1;
    int sidebarScrollbarHideType = 0;
    bool sidebarScrollbarOnLeft = false;
    bool sidebarDisableScrollbarFadeout = false;
};
