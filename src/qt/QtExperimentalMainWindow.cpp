/*
 * VertexNote
 *
 * Experimental Qt main window bootstrap.
 */

#include "QtExperimentalMainWindow.h"

#include <QStatusBar>
#include <QToolBar>

QtExperimentalMainWindow::QtExperimentalMainWindow(): commandRegistry(this) {
    setObjectName("vertexNoteQtExperimentalMainWindow");
    setWindowTitle("VertexNote");
    resize(1440, 900);

    this->toolBar = addToolBar(QStringLiteral("Main"));
    this->toolBar->setObjectName(QStringLiteral("vertexNoteQtExperimentalMainToolBar"));
    this->toolBar->setMovable(false);

    this->canvasWidget = new QtExperimentalCanvas(this);
    setCentralWidget(this->canvasWidget);
    statusBar()->showMessage(QStringLiteral("Qt experimental shell ready"));
}

auto QtExperimentalMainWindow::canvas() -> QtExperimentalCanvas* { return this->canvasWidget; }

auto QtExperimentalMainWindow::commandHost() -> QtExperimentalCommandHost* { return &this->commandRegistry; }

auto QtExperimentalMainWindow::mainToolBar() -> QToolBar* { return this->toolBar; }
