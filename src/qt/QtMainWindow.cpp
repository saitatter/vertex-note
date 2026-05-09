/*
 * VertexNote
 *
 * Qt main window bootstrap.
 */

#include "QtMainWindow.h"

#include <QStatusBar>
#include <QStyle>
#include <QToolBar>

QtMainWindow::QtMainWindow(): commandRegistry(this) {
    setObjectName("vertexNoteQtMainWindow");
    setWindowTitle("VertexNote");
    resize(1440, 900);

    this->toolBar = addToolBar(QStringLiteral("Main"));
    this->toolBar->setObjectName(QStringLiteral("vertexNoteQtMainToolBar"));
    this->toolBar->setMovable(false);
    this->toolBar->setToolButtonStyle(Qt::ToolButtonIconOnly);
    this->toolBar->setIconSize(QSize(22, 22));

    this->canvasWidget = new QtCanvas(this);
    setCentralWidget(this->canvasWidget);

    this->layerPanelWidget = new QtLayerPanel(this);
    addDockWidget(Qt::RightDockWidgetArea, this->layerPanelWidget);

    this->pageSidebarWidget = new QtPageSidebar(this);
    addDockWidget(Qt::LeftDockWidgetArea, this->pageSidebarWidget);

    this->toolPaletteWidget = new QtToolPalette(this);
    this->toolPaletteWidget->setVisible(false);  // Hidden until a drawing tool is selected

    // Persistent status bar widgets
    this->pageLabel = new QLabel(QStringLiteral("Page 1 of 1"), this);
    this->layerLabel = new QLabel(QStringLiteral("Layer: Layer 1"), this);
    this->zoomLabel = new QLabel(QStringLiteral("100%"), this);
    statusBar()->addPermanentWidget(this->pageLabel);
    statusBar()->addPermanentWidget(this->layerLabel);
    statusBar()->addPermanentWidget(this->zoomLabel);

    statusBar()->showMessage(QStringLiteral("Ready"));
}

auto QtMainWindow::canvas() -> QtCanvas* { return this->canvasWidget; }

auto QtMainWindow::commandHost() -> QtCommandHost* { return &this->commandRegistry; }

auto QtMainWindow::mainToolBar() -> QToolBar* { return this->toolBar; }

auto QtMainWindow::layerPanel() -> QtLayerPanel* { return this->layerPanelWidget; }

auto QtMainWindow::pageSidebar() -> QtPageSidebar* { return this->pageSidebarWidget; }

auto QtMainWindow::toolPalette() -> QtToolPalette* { return this->toolPaletteWidget; }

auto QtMainWindow::pageStatusLabel() -> QLabel* { return this->pageLabel; }

auto QtMainWindow::layerStatusLabel() -> QLabel* { return this->layerLabel; }

auto QtMainWindow::zoomStatusLabel() -> QLabel* { return this->zoomLabel; }
