/*
 * VertexNote
 *
 * Qt main window bootstrap.
 */

#pragma once

#include <QMainWindow>

#include "QtCanvas.h"
#include "QtCommandHost.h"

class QToolBar;

class QtMainWindow: public QMainWindow {
public:
    QtMainWindow();

    [[nodiscard]] auto canvas() -> QtCanvas*;
    [[nodiscard]] auto commandHost() -> QtCommandHost*;
    [[nodiscard]] auto mainToolBar() -> QToolBar*;

private:
    QtCanvas* canvasWidget = nullptr;
    QToolBar* toolBar = nullptr;
    QtCommandHost commandRegistry;
};
