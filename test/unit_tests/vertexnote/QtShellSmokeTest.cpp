#include <gtest/gtest.h>

#include <QByteArray>
#include <QFileInfo>
#include <QProcess>
#include <QProcessEnvironment>
#include <QString>
#include <QStringList>

TEST(VertexNoteQtShellSmoke, launchesOffscreenAndRuns3DWorkspaceCommands) {
#if !defined(VERTEXNOTE_QT_SHELL_EXE)
    GTEST_SKIP() << "Qt shell executable path is not configured";
#else
    const QString executable = QString::fromUtf8(VERTEXNOTE_QT_SHELL_EXE);
    ASSERT_TRUE(QFileInfo::exists(executable)) << executable.toStdString();

    QProcess process;
    auto environment = QProcessEnvironment::systemEnvironment();
    environment.insert(QStringLiteral("QT_QPA_PLATFORM"), QStringLiteral("offscreen"));
    process.setProcessEnvironment(environment);
    process.setProgram(executable);
    process.setArguments({QStringLiteral("--smoke-test")});

    process.start();
    ASSERT_TRUE(process.waitForStarted(10000)) << process.errorString().toStdString();
    ASSERT_TRUE(process.waitForFinished(20000)) << process.errorString().toStdString();

    const QByteArray standardOutput = process.readAllStandardOutput();
    const QByteArray standardError = process.readAllStandardError();
    EXPECT_EQ(process.exitStatus(), QProcess::NormalExit)
            << standardOutput.toStdString() << standardError.toStdString();
    EXPECT_EQ(process.exitCode(), 0) << standardOutput.toStdString() << standardError.toStdString();
#endif
}
