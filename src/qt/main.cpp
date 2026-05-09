/*
 * VertexNote
 *
 * Experimental Qt shell bootstrap entry point.
 */

#include <QApplication>

#include "QtExperimentalAppShell.h"

auto main(int argc, char* argv[]) -> int {
    QApplication app(argc, argv);
    app.setApplicationName("VertexNote");
    app.setOrganizationName("VertexNote");
    app.setApplicationDisplayName("VertexNote");

    QtExperimentalAppShell shell;
    shell.showMainWindow();

    return QApplication::exec();
}
