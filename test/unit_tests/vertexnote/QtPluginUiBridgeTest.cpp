#include <memory>

#include <gtest/gtest.h>

#include <QApplication>
#include <QLabel>
#include <QMainWindow>
#include <QToolBar>

#include "qt/QtCommandHost.h"
#include "qt/QtPluginUiBridge.h"

namespace {

auto ensureQApplication() -> QApplication* {
    if (auto* app = qobject_cast<QApplication*>(QApplication::instance())) {
        return app;
    }

    qputenv("QT_QPA_PLATFORM", "offscreen");
    static int argc = 1;
    static char appName[] = "vertexnote-test";
    static char* argv[] = {appName, nullptr};
    static auto app = std::make_unique<QApplication>(argc, argv);
    return app.get();
}

}  // namespace

TEST(VertexNoteQtPluginUiBridge, registersUpdatesAndRemovesToolbarPlaceholders) {
    ensureQApplication();

    QMainWindow window;
    QtCommandHost commandHost(&window);
    QtPluginUiBridge bridge(&commandHost, &window);

    bridge.registerPlaceholder("plugin.test.placeholder.mode", "mode", "Current mode");

    auto* toolbar = window.findChild<QToolBar*>(QStringLiteral("PluginsToolbar"));
    ASSERT_NE(toolbar, nullptr);
    ASSERT_EQ(toolbar->actions().size(), 1);

    auto* label = toolbar->findChild<QLabel*>(QStringLiteral("vertexNoteQtPluginPlaceholder"));
    ASSERT_NE(label, nullptr);
    EXPECT_EQ(label->text(), QStringLiteral("mode"));
    EXPECT_EQ(label->toolTip(), QStringLiteral("Current mode"));

    bridge.setPlaceholderValue("plugin.test.placeholder.mode", "INSERT");
    EXPECT_EQ(label->text(), QStringLiteral("INSERT"));

    bridge.setPlaceholderValue("plugin.test.placeholder.mode", "");
    EXPECT_EQ(label->text(), QStringLiteral("mode"));

    bridge.removePlaceholder("plugin.test.placeholder.mode");
    EXPECT_TRUE(toolbar->actions().empty());
}
