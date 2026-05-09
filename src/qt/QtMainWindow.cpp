/*
 * VertexNote
 *
 * Qt main window bootstrap.
 */

#include "QtMainWindow.h"

#include <QStatusBar>
#include <QToolBar>

QtMainWindow::QtMainWindow(): commandRegistry(this) {
    setObjectName("vertexNoteQtMainWindow");
    setWindowTitle("VertexNote");
    resize(1440, 900);

    this->toolBar = addToolBar(QStringLiteral("Main"));
    this->toolBar->setObjectName(QStringLiteral("vertexNoteQtMainToolBar"));
    this->toolBar->setMovable(false);

    this->canvasWidget = new QtCanvas(this);
    setCentralWidget(this->canvasWidget);

    this->layerPanelWidget = new QtLayerPanel(this);
    addDockWidget(Qt::RightDockWidgetArea, this->layerPanelWidget);

    this->pageSidebarWidget = new QtPageSidebar(this);
    addDockWidget(Qt::LeftDockWidgetArea, this->pageSidebarWidget);

    statusBar()->showMessage(QStringLiteral("Qt shell ready"));
}

auto QtMainWindow::canvas() -> QtCanvas* { return this->canvasWidget; }

auto QtMainWindow::commandHost() -> QtCommandHost* { return &this->commandRegistry; }

auto QtMainWindow::mainToolBar() -> QToolBar* { return this->toolBar; }

auto QtMainWindow::layerPanel() -> QtLayerPanel* { return this->layerPanelWidget; }

auto QtMainWindow::pageSidebar() -> QtPageSidebar* { return this->pageSidebarWidget; }
