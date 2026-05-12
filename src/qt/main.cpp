/*
 * VertexNote
 *
 * Qt shell bootstrap entry point.
 */

#include <QApplication>

#include "QtAppShell.h"
#include "control/CrashHandler.h"

auto main(int argc, char* argv[]) -> int {
    installCrashHandlers();

    QApplication app(argc, argv);
    app.setApplicationName("VertexNote");
    app.setOrganizationName("VertexNote");
    app.setApplicationDisplayName("VertexNote");

    QtAppShell shell;
    shell.showMainWindow();

    return QApplication::exec();
}
