/*
 * VertexNote
 *
 * Qt geometry / 3D command panel.
 */

#pragma once

#include <initializer_list>
#include <string_view>

#include <QDockWidget>
#include <QString>

class QtCommandHost;
class QDoubleSpinBox;
class QLabel;
class QVBoxLayout;
class QWidget;

enum class QtWorkspacePanelMode {
    Notes,
    Geometry,
    ThreeD,
};

class QtGeometryPanel: public QDockWidget {
    Q_OBJECT

public:
    explicit QtGeometryPanel(QWidget* parent = nullptr);

    void bindCommandHost(QtCommandHost* commandHost);
    void setWorkspaceMode(QtWorkspacePanelMode mode);
    [[nodiscard]] auto workspaceMode() const -> QtWorkspacePanelMode;
    void setStatusSummary(const QString& mode, const QString& snap, const QString& selection, const QString& view,
                          const QString& projection, const QString& depth);
    void setTopologySummary(const QString& topology);
    void setModelInspector(double x, double y, double z, bool enabled);

Q_SIGNALS:
    void modelPositionEdited(double x, double y, double z);

private:
    struct PanelCommand {
        std::string_view id;
        std::string_view label;
    };

    void clearContent();
    void rebuildContent();
    void updateSummaryVisibility();
    void emitModelPositionEdited();
    void addSection(const QString& title, std::initializer_list<PanelCommand> commands, int columns = 2,
                    bool expanded = true);

private:
    QtCommandHost* commandHost = nullptr;
    QtWorkspacePanelMode panelMode = QtWorkspacePanelMode::Notes;
    QWidget* contentWidget = nullptr;
    QVBoxLayout* contentLayout = nullptr;
    QVBoxLayout* sectionsLayout = nullptr;
    QLabel* modeStatusLabel = nullptr;
    QLabel* snapStatusLabel = nullptr;
    QLabel* selectionStatusLabel = nullptr;
    QLabel* topologyStatusLabel = nullptr;
    QLabel* viewStatusLabel = nullptr;
    QLabel* projectionStatusLabel = nullptr;
    QLabel* depthStatusLabel = nullptr;
    QDoubleSpinBox* modelXSpinBox = nullptr;
    QDoubleSpinBox* modelYSpinBox = nullptr;
    QDoubleSpinBox* modelZSpinBox = nullptr;
    bool loadingModelInspector = false;
};
