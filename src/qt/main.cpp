/*
 * VertexNote
 *
 * Qt shell bootstrap entry point.
 */

#include <QApplication>
#include <QIcon>
#include <QString>

#include "QtAppShell.h"
#include "config-paths.h"
#include "control/CrashHandler.h"

auto main(int argc, char* argv[]) -> int {
    installCrashHandlers();

    QApplication app(argc, argv);
    app.setApplicationName("VertexNote");
    app.setOrganizationName("VertexNote");
    app.setApplicationDisplayName("VertexNote");
    app.setWindowIcon(QIcon(QString::fromUtf8(PROJECT_SOURCE_DIR) +
                            QStringLiteral("/ui/pixmaps/app.vertexnote.VertexNote.svg")));

    QtAppShell shell;
    shell.showMainWindow();

    return QApplication::exec();
}
