/*
 * VertexNote
 *
 * Experimental Qt main window bootstrap.
 */

#pragma once

#include <QMainWindow>

#include "QtExperimentalCanvas.h"
#include "QtExperimentalCommandHost.h"

class QToolBar;

class QtExperimentalMainWindow: public QMainWindow {
public:
    QtExperimentalMainWindow();

    [[nodiscard]] auto canvas() -> QtExperimentalCanvas*;
    [[nodiscard]] auto commandHost() -> QtExperimentalCommandHost*;
    [[nodiscard]] auto mainToolBar() -> QToolBar*;

private:
    QtExperimentalCanvas* canvasWidget = nullptr;
    QToolBar* toolBar = nullptr;
    QtExperimentalCommandHost commandRegistry;
};
