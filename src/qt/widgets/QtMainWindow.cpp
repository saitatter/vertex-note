/*
 * VertexNote
 *
 * Qt main window bootstrap.
 */

#include "QtMainWindow.h"

#include "config-paths.h"

#include <algorithm>

#include <QComboBox>
#include <QIcon>
#include <QMenuBar>
#include <QPoint>
#include <QSlider>
#include <QSpinBox>
#include <QStatusBar>
#include <QStyle>
#include <QToolBar>

QtMainWindow::QtMainWindow(): commandRegistry(this) {
    setObjectName("vertexNoteQtMainWindow");
    setWindowTitle("VertexNote");
    setWindowIcon(QIcon(QString::fromUtf8(PROJECT_SOURCE_DIR) +
                        QStringLiteral("/ui/pixmaps/app.vertexnote.VertexNote.svg")));
    resize(1440, 900);
    setDockOptions(QMainWindow::AnimatedDocks | QMainWindow::AllowTabbedDocks | QMainWindow::GroupedDragging);
    menuBar()->setNativeMenuBar(false);
    const QString menuCheckIconPath =
            QString::fromUtf8(PROJECT_SOURCE_DIR) + QStringLiteral("/ui/iconsCommon/menu-check.svg");
    setStyleSheet(QStringLiteral(
            "#vertexNoteQtMainWindow { background: #d6d2c9; }"
            "QMenuBar { background: #f7f7f7; border-bottom: 1px solid #d8d8d8; }"
            "QMenuBar::item { padding: 2px 7px; }"
            "QMenu { background: #ffffff; border: 1px solid #bdbdbd; }"
            "QMenu::item { background: transparent; padding: 4px 26px 4px 26px; min-height: 17px; }"
            "QMenu::item:selected { background: #e7f0ff; color: #111111; }"
            "QMenu::item:checked { background: transparent; }"
            "QMenu#vertexNoteQtToolFamilyMenu::item:checked { background: #eef5ff; }"
            "QMenu::item:disabled { color: #a9a9a9; }"
            "QMenu::separator { height: 1px; background: #e2e2e2; margin: 4px 0px; }"
            "QMenu::indicator { width: 13px; height: 13px; left: 5px; border: 1px solid #555555; background: #ffffff; }"
            "QMenu::indicator:checked { background: #ffffff; image: url(\"%1\"); }"
            "QMenu::indicator:unchecked { background: #ffffff; image: none; }"
            "QMenu::indicator:disabled { border: 1px solid #b8b8b8; background: #ffffff; }"
            "QToolBar#vertexNoteQtDocumentToolBar, QToolBar#vertexNoteQtToolsToolBar, QToolBar#vertexNoteQtFooterToolBar,"
            " QToolBar#vertexNoteQtLeftPrimaryToolBar, QToolBar#vertexNoteQtLeftSecondaryToolBar,"
            " QToolBar#vertexNoteQtRightPrimaryToolBar, QToolBar[vertexFloatToolbar=\"true\"] {"
            " background: #f7f7f7; border: none; border-bottom: 1px solid #d8d8d8; spacing: 0px; padding: 2px 3px; }"
            "QToolBar#vertexNoteQtFooterToolBar { border-top: 1px solid #d8d8d8; border-bottom: none; }"
            "QToolBar#vertexNoteQtLeftPrimaryToolBar, QToolBar#vertexNoteQtLeftSecondaryToolBar {"
            " border-right: 1px solid #d8d8d8; border-bottom: none; padding: 3px 1px; }"
            "QToolBar#vertexNoteQtRightPrimaryToolBar {"
            " border-left: 1px solid #d8d8d8; border-bottom: none; padding: 3px 1px; }"
            "QToolBar[vertexFloatToolbar=\"true\"] { border: 1px solid #d8d8d8; padding: 3px; }"
            "QToolBar::separator { background: #d8d8d8; width: 1px; margin: 2px 4px; }"
            "QToolBar QToolButton { margin: 0px; padding: 3px; min-width: 30px; min-height: 30px; border: 1px solid transparent; border-radius: 2px; }"
            "QToolBar QToolButton:hover { background: #ececec; border-color: #c8c8c8; }"
            "QToolBar QToolButton:checked { background: #dce8ff; border-color: #8db0ff; }"
            "QToolBar QToolButton#vertexNoteQtFamilyToolButton {"
            " background: transparent; padding-right: 10px; min-width: 34px; min-height: 30px;"
            " border: 1px solid transparent; border-radius: 2px; }"
            "QToolBar QToolButton#vertexNoteQtFamilyToolButton:hover { background: #ececec; border-color: #c8c8c8; }"
            "QToolBar QToolButton#vertexNoteQtFamilyToolButton:pressed { background: #e1e1e1; border-color: #bcbcbc; }"
            "QToolBar QToolButton#vertexNoteQtFamilyToolButton:checked,"
            " QToolBar QToolButton#vertexNoteQtFamilyToolButton:on,"
            " QToolBar QToolButton#vertexNoteQtFamilyToolButton:open { background: #dce8ff; border-color: #8db0ff; }"
            "QToolBar QToolButton#vertexNoteQtFamilyToolButton::menu-button {"
            " background: transparent; border: none; width: 12px; }"
            "QToolBar QToolButton#vertexNoteQtFamilyToolButton::menu-button:hover,"
            " QToolBar QToolButton#vertexNoteQtFamilyToolButton::menu-button:pressed,"
            " QToolBar QToolButton#vertexNoteQtFamilyToolButton::menu-button:open { background: transparent; }"
            "QToolBar QWidget#vertexNoteQtFamilySplitWidget { margin: 0px 1px; padding: 0px; background: transparent; }"
            "QToolBar QToolButton#vertexNoteQtFamilyActionButton {"
            " background: transparent; padding: 3px; min-width: 30px; min-height: 30px;"
            " border: 1px solid transparent; border-top-left-radius: 2px; border-bottom-left-radius: 2px;"
            " border-top-right-radius: 0px; border-bottom-right-radius: 0px; }"
            "QToolBar QToolButton#vertexNoteQtFamilyActionButton:hover { background: #ececec; border-color: #c8c8c8; }"
            "QToolBar QToolButton#vertexNoteQtFamilyActionButton:pressed { background: #e1e1e1; border-color: #bcbcbc; }"
            "QToolBar QToolButton#vertexNoteQtFamilyActionButton:checked { background: #dce8ff; border-color: #8db0ff; }"
            "QToolBar QToolButton#vertexNoteQtFamilyMenuButton {"
            " background: transparent; padding: 0px; min-width: 18px; max-width: 18px; min-height: 30px; max-height: 30px;"
            " border: 1px solid transparent; border-left: 0px; border-top-left-radius: 0px; border-bottom-left-radius: 0px;"
            " border-top-right-radius: 2px; border-bottom-right-radius: 2px; }"
            "QToolBar QToolButton#vertexNoteQtFamilyMenuButton:hover { background: #ececec; border-color: #c8c8c8; }"
            "QToolBar QToolButton#vertexNoteQtFamilyMenuButton:pressed,"
            " QToolBar QToolButton#vertexNoteQtFamilyMenuButton:open { background: #e1e1e1; border-color: #bcbcbc; }"
            "QToolBar QToolButton#vertexNoteQtToolbarColorButton {"
            " margin: 0px 4px; padding: 0px; min-width: 20px; max-width: 20px; min-height: 20px; max-height: 20px;"
            " border-radius: 10px; }"
            "QToolBar QToolButton#vertexNoteQtToolbarColorSelectButton {"
            " margin: 0px 5px; padding: 0px; min-width: 26px; max-width: 26px; min-height: 26px; max-height: 26px;"
            " border-radius: 13px; }"
            "#vertexNoteQtFontFamilyCombo, #vertexNoteQtFontSizeSpinner, #vertexNoteQtFillOpacitySpinner,"
            " #vertexNoteQtFooterPageSpin, #vertexNoteQtFooterLayerCombo {"
            " min-height: 28px; margin: 0 1px; padding: 0px 4px; border: 1px solid #c9c9c9; background: #ffffff; }"
            "#vertexNoteQtFooterZoomSlider { margin: 0 3px; }"
            "#vertexNoteQtFooterZoomSlider::groove:horizontal { height: 4px; background: #c8c8c8; border-radius: 2px; }"
            "#vertexNoteQtFooterZoomSlider::sub-page:horizontal { background: #4f8fff; border-radius: 2px; }"
            "#vertexNoteQtFooterZoomSlider::handle:horizontal { width: 10px; margin: -4px 0; border-radius: 5px;"
            " background: #ffffff; border: 1px solid #9a9a9a; }"
            "QDockWidget { titlebar-close-icon: none; titlebar-normal-icon: none; }"
            "QDockWidget::title { background: #f5f5f5; border-bottom: 1px solid #d8d8d8; padding: 3px 6px; text-align: left; }"
            "QMainWindow::separator { width: 1px; height: 1px; background: #d8d8d8; }"
            "QTabBar::tab { background: #f0efed; border: 1px solid #d7d3cb; padding: 3px 8px; margin-right: 1px; }"
            "QTabBar::tab:selected { background: #f7f7f7; }"
            "#vertexNoteQtPageSidebar QListWidget, #vertexNoteQtLayerPanel QListWidget {"
            " background: #fbfbfb; border: none; outline: none; }"
            "#vertexNoteQtPageSidebar QListWidget::item { padding: 2px 0px 4px 0px; }"
            "#vertexNoteQtLayerPanel QListWidget::item { padding: 2px 4px; }"
            "#vertexNoteQtLayerPanel QListWidget::item:selected {"
            " background: #e5efff; border: 1px solid #96b6ff; }"
            "#vertexNoteQtPageSidebar QListWidget::item:selected { background: #fff4f4; border: 1px solid #d85c5c; }"
            "#vertexNoteQtLayerPanel QToolButton {"
            " margin: 0px; padding: 2px; min-width: 26px; min-height: 26px;"
            " border: 1px solid transparent; border-radius: 2px; }"
            "#vertexNoteQtLayerPanel QToolButton:hover { background: #ececec; border-color: #c8c8c8; }"
            "#vertexNoteQtLayerPanel QToolButton:pressed { background: #e1e1e1; border-color: #bcbcbc; }"
            "QStatusBar { background: #f7f7f7; border-top: 1px solid #d8d8d8; }")
                          .arg(menuCheckIconPath));

    this->documentToolBar = addToolBar(QStringLiteral("Document"));
    this->documentToolBar->setObjectName(QStringLiteral("vertexNoteQtDocumentToolBar"));
    this->documentToolBar->setMovable(false);
    this->documentToolBar->setToolButtonStyle(Qt::ToolButtonIconOnly);
    this->documentToolBar->setIconSize(QSize(22, 22));

    addToolBarBreak();
    this->toolsToolBarWidget = addToolBar(QStringLiteral("Tools"));
    this->toolsToolBarWidget->setObjectName(QStringLiteral("vertexNoteQtToolsToolBar"));
    this->toolsToolBarWidget->setMovable(false);
    this->toolsToolBarWidget->setToolButtonStyle(Qt::ToolButtonIconOnly);
    this->toolsToolBarWidget->setIconSize(QSize(22, 22));

    this->footerToolBarWidget = new QToolBar(QStringLiteral("Footer"), this);
    this->footerToolBarWidget->setObjectName(QStringLiteral("vertexNoteQtFooterToolBar"));
    this->footerToolBarWidget->setMovable(false);
    this->footerToolBarWidget->setToolButtonStyle(Qt::ToolButtonIconOnly);
    this->footerToolBarWidget->setIconSize(QSize(22, 22));
    addToolBar(Qt::BottomToolBarArea, this->footerToolBarWidget);

    this->leftPrimaryToolBarWidget = new QToolBar(QStringLiteral("Left Primary"), this);
    this->leftPrimaryToolBarWidget->setObjectName(QStringLiteral("vertexNoteQtLeftPrimaryToolBar"));
    this->leftPrimaryToolBarWidget->setMovable(false);
    this->leftPrimaryToolBarWidget->setFloatable(false);
    this->leftPrimaryToolBarWidget->setOrientation(Qt::Vertical);
    this->leftPrimaryToolBarWidget->setToolButtonStyle(Qt::ToolButtonIconOnly);
    this->leftPrimaryToolBarWidget->setIconSize(QSize(22, 22));
    addToolBar(Qt::LeftToolBarArea, this->leftPrimaryToolBarWidget);

    this->leftSecondaryToolBarWidget = new QToolBar(QStringLiteral("Left Secondary"), this);
    this->leftSecondaryToolBarWidget->setObjectName(QStringLiteral("vertexNoteQtLeftSecondaryToolBar"));
    this->leftSecondaryToolBarWidget->setMovable(false);
    this->leftSecondaryToolBarWidget->setFloatable(false);
    this->leftSecondaryToolBarWidget->setOrientation(Qt::Vertical);
    this->leftSecondaryToolBarWidget->setToolButtonStyle(Qt::ToolButtonIconOnly);
    this->leftSecondaryToolBarWidget->setIconSize(QSize(22, 22));
    addToolBar(Qt::LeftToolBarArea, this->leftSecondaryToolBarWidget);

    this->rightPrimaryToolBarWidget = new QToolBar(QStringLiteral("Right Primary"), this);
    this->rightPrimaryToolBarWidget->setObjectName(QStringLiteral("vertexNoteQtRightPrimaryToolBar"));
    this->rightPrimaryToolBarWidget->setMovable(false);
    this->rightPrimaryToolBarWidget->setFloatable(false);
    this->rightPrimaryToolBarWidget->setOrientation(Qt::Vertical);
    this->rightPrimaryToolBarWidget->setToolButtonStyle(Qt::ToolButtonIconOnly);
    this->rightPrimaryToolBarWidget->setIconSize(QSize(22, 22));
    addToolBar(Qt::RightToolBarArea, this->rightPrimaryToolBarWidget);

    for (int index = 0; index < 4; ++index) {
        auto* floatingToolBar = new QToolBar(QStringLiteral("Floating Toolbar %1").arg(index + 1), this);
        floatingToolBar->setObjectName(QStringLiteral("vertexNoteQtFloatingToolBar%1").arg(index + 1));
        floatingToolBar->setProperty("vertexFloatToolbar", true);
        floatingToolBar->setProperty("vertexHasSavedGeometry", false);
        floatingToolBar->setProperty("vertexUserHidden", false);
        floatingToolBar->setProperty("vertexProgrammaticVisibilityChange", false);
        floatingToolBar->setMovable(true);
        floatingToolBar->setFloatable(true);
        floatingToolBar->setToolButtonStyle(Qt::ToolButtonIconOnly);
        floatingToolBar->setIconSize(QSize(22, 22));
        floatingToolBar->setParent(this, Qt::Tool | Qt::WindowTitleHint | Qt::WindowCloseButtonHint |
                                           Qt::CustomizeWindowHint);
        floatingToolBar->setOrientation(Qt::Horizontal);
        floatingToolBar->hide();
        this->floatingToolBarWidgets.push_back(floatingToolBar);
    }

    this->canvasWidget = new QtCanvas(this);
    setCentralWidget(this->canvasWidget);

    this->pageSidebarWidget = new QtPageSidebar(this);
    addDockWidget(Qt::LeftDockWidgetArea, this->pageSidebarWidget);

    this->layerPanelWidget = new QtLayerPanel(this);
    addDockWidget(Qt::LeftDockWidgetArea, this->layerPanelWidget);
    tabifyDockWidget(this->pageSidebarWidget, this->layerPanelWidget);
    this->pageSidebarWidget->raise();
    resizeDocks({this->pageSidebarWidget}, {90}, Qt::Horizontal);

    this->toolPaletteWidget = new QtToolPalette(this);
    this->toolPaletteWidget->setVisible(false);  // Hidden until a drawing tool is selected

    this->footerPageSpinWidget = new QSpinBox(this);
    this->footerPageSpinWidget->setObjectName(QStringLiteral("vertexNoteQtFooterPageSpin"));
    this->footerPageSpinWidget->setMinimum(1);
    this->footerPageSpinWidget->setMaximum(1);
    this->footerPageSpinWidget->setFixedWidth(64);

    this->footerLayerComboWidget = new QComboBox(this);
    this->footerLayerComboWidget->setObjectName(QStringLiteral("vertexNoteQtFooterLayerCombo"));
    this->footerLayerComboWidget->setMinimumContentsLength(8);
    this->footerLayerComboWidget->setSizeAdjustPolicy(QComboBox::AdjustToContents);

    this->footerZoomSliderWidget = new QSlider(Qt::Horizontal, this);
    this->footerZoomSliderWidget->setObjectName(QStringLiteral("vertexNoteQtFooterZoomSlider"));
    this->footerZoomSliderWidget->setRange(10, 800);
    this->footerZoomSliderWidget->setFixedWidth(136);

    // Persistent status bar widgets
    this->pageLabel = new QLabel(QStringLiteral("Page 1 of 1"), this);
    this->layerLabel = new QLabel(QStringLiteral("Layer: Layer 1"), this);
    this->zoomLabel = new QLabel(QStringLiteral("100%"), this);
    this->pageLabel->hide();
    this->layerLabel->hide();
    this->zoomLabel->hide();
    statusBar()->addPermanentWidget(this->pageLabel);
    statusBar()->addPermanentWidget(this->layerLabel);
    statusBar()->addPermanentWidget(this->zoomLabel);

    statusBar()->showMessage(QStringLiteral("Ready"));
}

auto QtMainWindow::canvas() -> QtCanvas* { return this->canvasWidget; }

auto QtMainWindow::canvas() const -> const QtCanvas* { return this->canvasWidget; }

auto QtMainWindow::commandHost() -> QtCommandHost* { return &this->commandRegistry; }

auto QtMainWindow::commandHost() const -> const QtCommandHost* { return &this->commandRegistry; }

auto QtMainWindow::mainToolBar() -> QToolBar* { return this->documentToolBar; }

auto QtMainWindow::toolsToolBar() -> QToolBar* { return this->toolsToolBarWidget; }

auto QtMainWindow::footerToolBar() -> QToolBar* { return this->footerToolBarWidget; }

auto QtMainWindow::leftPrimaryToolBar() -> QToolBar* { return this->leftPrimaryToolBarWidget; }

auto QtMainWindow::leftSecondaryToolBar() -> QToolBar* { return this->leftSecondaryToolBarWidget; }

auto QtMainWindow::rightPrimaryToolBar() -> QToolBar* { return this->rightPrimaryToolBarWidget; }

auto QtMainWindow::floatingToolBars() const -> const std::vector<QToolBar*>& { return this->floatingToolBarWidgets; }

auto QtMainWindow::layerPanel() -> QtLayerPanel* { return this->layerPanelWidget; }

auto QtMainWindow::pageSidebar() -> QtPageSidebar* { return this->pageSidebarWidget; }

auto QtMainWindow::toolPalette() -> QtToolPalette* { return this->toolPaletteWidget; }

auto QtMainWindow::footerPageSpin() -> QSpinBox* { return this->footerPageSpinWidget; }

auto QtMainWindow::footerLayerCombo() -> QComboBox* { return this->footerLayerComboWidget; }

auto QtMainWindow::footerZoomSlider() -> QSlider* { return this->footerZoomSliderWidget; }

auto QtMainWindow::pageStatusLabel() -> QLabel* { return this->pageLabel; }

auto QtMainWindow::layerStatusLabel() -> QLabel* { return this->layerLabel; }

auto QtMainWindow::zoomStatusLabel() -> QLabel* { return this->zoomLabel; }

auto QtMainWindow::preferredSidebarArea() const -> Qt::DockWidgetArea {
    return this->sidebarRightSide ? Qt::RightDockWidgetArea : Qt::LeftDockWidgetArea;
}

void QtMainWindow::setSidebarPreferences(int width, bool onRight, int numberingStyle, int scrollbarHideType,
                                         bool scrollbarOnLeft, bool disableScrollbarFadeout) {
    this->sidebarPreferredWidth = std::clamp(width, 76, 600);
    this->sidebarRightSide = onRight;
    this->sidebarNumberingStyle = numberingStyle;
    this->sidebarScrollbarHideType = scrollbarHideType;
    this->sidebarScrollbarOnLeft = scrollbarOnLeft;
    this->sidebarDisableScrollbarFadeout = disableScrollbarFadeout;
    this->pageSidebarWidget->setPreferredSidebarWidth(this->sidebarPreferredWidth);
    this->pageSidebarWidget->setDisplayOptions(this->sidebarNumberingStyle, this->sidebarScrollbarHideType,
                                               this->sidebarScrollbarOnLeft, this->sidebarDisableScrollbarFadeout);
    resizeDocks({this->pageSidebarWidget}, {this->sidebarPreferredWidth}, Qt::Horizontal);
}

void QtMainWindow::setGtkParitySidebarMode(bool enabled) {
    const auto sidebarArea = preferredSidebarArea();
    if (enabled) {
        removeDockWidget(this->layerPanelWidget);
        if (dockWidgetArea(this->pageSidebarWidget) == Qt::NoDockWidgetArea) {
            addDockWidget(sidebarArea, this->pageSidebarWidget);
        } else if (dockWidgetArea(this->pageSidebarWidget) != sidebarArea) {
            addDockWidget(sidebarArea, this->pageSidebarWidget);
        }
        this->pageSidebarWidget->raise();
        resizeDocks({this->pageSidebarWidget}, {this->sidebarPreferredWidth}, Qt::Horizontal);
        return;
    }

    if (dockWidgetArea(this->pageSidebarWidget) == Qt::NoDockWidgetArea) {
        addDockWidget(sidebarArea, this->pageSidebarWidget);
    } else if (dockWidgetArea(this->pageSidebarWidget) != sidebarArea) {
        addDockWidget(sidebarArea, this->pageSidebarWidget);
    }
    if (dockWidgetArea(this->layerPanelWidget) == Qt::NoDockWidgetArea) {
        addDockWidget(sidebarArea, this->layerPanelWidget);
    } else if (dockWidgetArea(this->layerPanelWidget) != sidebarArea) {
        addDockWidget(sidebarArea, this->layerPanelWidget);
    }
    tabifyDockWidget(this->pageSidebarWidget, this->layerPanelWidget);
    this->pageSidebarWidget->raise();
    resizeDocks({this->pageSidebarWidget}, {this->sidebarPreferredWidth}, Qt::Horizontal);
}

void QtMainWindow::cascadeFloatingToolBars() {
    const QPoint base = this->frameGeometry().topLeft() + QPoint(36, 92);
    int visibleIndex = 0;
    for (auto* floatingToolBar: this->floatingToolBarWidgets) {
        if (!floatingToolBar || floatingToolBar->actions().isEmpty()) {
            continue;
        }
        if (floatingToolBar->property("vertexHasSavedGeometry").toBool()) {
            continue;
        }

        floatingToolBar->adjustSize();
        const QPoint offset(visibleIndex * 28, visibleIndex * 34);
        floatingToolBar->move(base + offset);
        ++visibleIndex;
    }
}
