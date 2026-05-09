/*
 * VertexNote
 *
 * Qt shell bootstrap entry point.
 */

#include <QApplication>

#include "QtAppShell.h"

auto main(int argc, char* argv[]) -> int {
    QApplication app(argc, argv);
    app.setApplicationName("VertexNote");
    app.setOrganizationName("VertexNote");
    app.setApplicationDisplayName("VertexNote");

    QtAppShell shell;
    shell.showMainWindow();

    return QApplication::exec();
}
