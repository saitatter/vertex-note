/*
 * VertexNote
 *
 * Qt shell bootstrap entry point.
 */

#include <array>
#include <memory>
#include <string_view>

#include <QApplication>
#include <QCoreApplication>
#include <QIcon>
#include <QSettings>
#include <QString>
#include <QTemporaryDir>
#include <QTimer>

#include "QtAppShell.h"
#include "config-paths.h"
#include "control/CrashHandler.h"

namespace {

[[nodiscard]] auto hasArgument(int argc, char* argv[], std::string_view wanted) -> bool {
    for (int index = 1; index < argc; ++index) {
        if (std::string_view(argv[index]) == wanted) {
            return true;
        }
    }
    return false;
}

[[nodiscard]] auto triggerSmokeCommand(vn::ui::common::ICommandHost* commandHost, std::string_view commandId) -> bool {
    if (!commandHost || !commandHost->hasCommand(commandId)) {
        return false;
    }
    commandHost->triggerCommand(commandId);
    return true;
}

[[nodiscard]] auto run3DWorkspaceSmoke(QtAppShell& shell) -> bool {
    auto* commandHost = shell.commandHost();
    for (const auto commandId: std::array{std::string_view("view.workspace-3d"),
                                          std::string_view("geometry.create-3d-vertex"),
                                          std::string_view("geometry.create-3d-edge"),
                                          std::string_view("geometry.create-3d-box"),
                                          std::string_view("geometry.project-3d-isometric"),
                                          std::string_view("geometry.project-3d-front"),
                                          std::string_view("geometry.project-3d-top"),
                                          std::string_view("geometry.nudge-z-up"),
                                          std::string_view("geometry.nudge-z-down")}) {
        if (!triggerSmokeCommand(commandHost, commandId)) {
            return false;
        }
    }
    return true;
}

}  // namespace

auto main(int argc, char* argv[]) -> int {
    installCrashHandlers();

    const bool smokeTest = hasArgument(argc, argv, "--smoke-test");
    if (smokeTest && qEnvironmentVariableIsEmpty("QT_QPA_PLATFORM")) {
        qputenv("QT_QPA_PLATFORM", QByteArrayLiteral("offscreen"));
    }

    QApplication app(argc, argv);
    app.setApplicationName("VertexNote");
    app.setOrganizationName("VertexNote");
    app.setApplicationDisplayName("VertexNote");
    app.setWindowIcon(QIcon(QString::fromUtf8(PROJECT_SOURCE_DIR) +
                            QStringLiteral("/ui/pixmaps/app.vertexnote.VertexNote.svg")));

    std::unique_ptr<QTemporaryDir> smokeSettingsDir;
    if (smokeTest) {
        smokeSettingsDir = std::make_unique<QTemporaryDir>();
        if (!smokeSettingsDir->isValid()) {
            return 2;
        }
        QSettings::setDefaultFormat(QSettings::IniFormat);
        QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, smokeSettingsDir->path());
    }

    QtAppShell shell;
    if (smokeTest) {
        shell.setPersistentUiStateSavingEnabled(false);
    }
    shell.showMainWindow();

    if (smokeTest) {
        QTimer::singleShot(0, &app, [&shell]() {
            const bool ok = run3DWorkspaceSmoke(shell);
            QTimer::singleShot(250, QCoreApplication::instance(), [ok]() {
                QCoreApplication::exit(ok ? 0 : 3);
            });
        });
    }

    return QApplication::exec();
}
