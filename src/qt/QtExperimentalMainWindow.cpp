/*
 * VertexNote
 *
 * Experimental Qt main window bootstrap.
 */

#include "QtExperimentalMainWindow.h"

#include <QIcon>

QtExperimentalMainWindow::QtExperimentalMainWindow(): commandRegistry(this) {
    setObjectName("vertexNoteQtExperimentalMainWindow");
    setWindowTitle("VertexNote");
    resize(1440, 900);

    this->canvasWidget = new QtExperimentalCanvas(this);
    setCentralWidget(this->canvasWidget);
}

auto QtExperimentalMainWindow::canvas() -> QtExperimentalCanvas* { return this->canvasWidget; }

auto QtExperimentalMainWindow::commandHost() -> QtExperimentalCommandHost* { return &this->commandRegistry; }
