/*
 * VertexNote
 *
 * Experimental Qt main window bootstrap.
 */

#pragma once

#include <QMainWindow>

#include "QtExperimentalCanvas.h"
#include "QtExperimentalCommandHost.h"

class QtExperimentalMainWindow: public QMainWindow {
public:
    QtExperimentalMainWindow();

    [[nodiscard]] auto canvas() -> QtExperimentalCanvas*;
    [[nodiscard]] auto commandHost() -> QtExperimentalCommandHost*;

private:
    QtExperimentalCanvas* canvasWidget = nullptr;
    QtExperimentalCommandHost commandRegistry;
};
