#include <string>
#include <utility>

#include <gtest/gtest.h>

#include <QDoubleSpinBox>
#include <QMainWindow>
#include <QString>
#include <QToolButton>

#include "QtCommandHost.h"
#include "QtGeometryPanel.h"

namespace {

void registerPanelCommand(QtCommandHost& commandHost, std::string id, std::string text) {
    vn::ui::common::CommandDescriptor descriptor;
    descriptor.id = std::move(id);
    descriptor.text = std::move(text);
    descriptor.tooltip = descriptor.text;
    descriptor.menu = "Smoke";
    commandHost.registerCommand(std::move(descriptor), []() {});
}

[[nodiscard]] auto panelButton(QtGeometryPanel& panel, const QString& text) -> QToolButton* {
    for (auto* button: panel.findChildren<QToolButton*>(QStringLiteral("vertexNoteQtGeometryPanelButton"))) {
        if (button->text() == text) {
            return button;
        }
    }
    return nullptr;
}

}  // namespace

TEST(VertexNoteQtGeometryPanelSmoke, threeDWorkspaceExposesCreateProjectionAndInspectorControls) {
    QMainWindow window;
    QtCommandHost commandHost(&window);
    for (const auto& command: {std::pair{"geometry.create-3d-vertex", "3D Vertex"},
                               std::pair{"geometry.create-3d-edge", "3D Edge"},
                               std::pair{"geometry.create-3d-box", "3D Box"},
                               std::pair{"geometry.project-3d-isometric", "3D Isometric"},
                               std::pair{"geometry.project-3d-front", "3D Front"},
                               std::pair{"geometry.project-3d-top", "3D Top"},
                               std::pair{"geometry.nudge-z-up", "Push Z"},
                               std::pair{"geometry.nudge-z-down", "Pull Z"}}) {
        registerPanelCommand(commandHost, command.first, command.second);
    }

    QtGeometryPanel panel;
    panel.bindCommandHost(&commandHost);
    panel.setWorkspaceMode(QtWorkspacePanelMode::ThreeD);

    EXPECT_EQ(panel.workspaceMode(), QtWorkspacePanelMode::ThreeD);
    EXPECT_NE(panelButton(panel, QStringLiteral("Vertex")), nullptr);
    EXPECT_NE(panelButton(panel, QStringLiteral("Edge")), nullptr);
    EXPECT_NE(panelButton(panel, QStringLiteral("Box")), nullptr);
    EXPECT_NE(panelButton(panel, QStringLiteral("Iso")), nullptr);
    EXPECT_NE(panelButton(panel, QStringLiteral("Front")), nullptr);
    EXPECT_NE(panelButton(panel, QStringLiteral("Top")), nullptr);
    EXPECT_NE(panelButton(panel, QStringLiteral("Z +")), nullptr);
    EXPECT_NE(panelButton(panel, QStringLiteral("Z -")), nullptr);

    auto* xSpin = panel.findChild<QDoubleSpinBox*>(QStringLiteral("vertexNoteQtGeometryPanelModelXSpin"));
    auto* ySpin = panel.findChild<QDoubleSpinBox*>(QStringLiteral("vertexNoteQtGeometryPanelModelYSpin"));
    auto* zSpin = panel.findChild<QDoubleSpinBox*>(QStringLiteral("vertexNoteQtGeometryPanelModelZSpin"));
    ASSERT_NE(xSpin, nullptr);
    ASSERT_NE(ySpin, nullptr);
    ASSERT_NE(zSpin, nullptr);
    EXPECT_FALSE(xSpin->isHidden());
    EXPECT_FALSE(ySpin->isHidden());
    EXPECT_FALSE(zSpin->isHidden());
    EXPECT_FALSE(xSpin->isEnabled());

    panel.setModelInspector(10.0, 20.5, -3.0, true);
    EXPECT_TRUE(xSpin->isEnabled());
    EXPECT_TRUE(ySpin->isEnabled());
    EXPECT_TRUE(zSpin->isEnabled());
    EXPECT_DOUBLE_EQ(xSpin->value(), 10.0);
    EXPECT_DOUBLE_EQ(ySpin->value(), 20.5);
    EXPECT_DOUBLE_EQ(zSpin->value(), -3.0);

    int editedSignals = 0;
    QObject::connect(&panel, &QtGeometryPanel::modelPositionEdited, &panel,
                     [&editedSignals](double x, double y, double z) {
                         ++editedSignals;
                         EXPECT_DOUBLE_EQ(x, 11.0);
                         EXPECT_DOUBLE_EQ(y, 20.5);
                         EXPECT_DOUBLE_EQ(z, -3.0);
                     });
    xSpin->setValue(11.0);
    EXPECT_EQ(editedSignals, 1);
}
