/*
 * VertexNote
 *
 * Qt geometry / 3D command panel implementation.
 */

#include "QtGeometryPanel.h"

#include <algorithm>

#include <QAction>
#include <QDoubleSpinBox>
#include <QFrame>
#include <QGridLayout>
#include <QLabel>
#include <QScrollArea>
#include <QSignalBlocker>
#include <QSize>
#include <QSizePolicy>
#include <QToolButton>
#include <QVBoxLayout>
#include <QWidget>

#include "QtCommandHost.h"

namespace {

auto makePanelButton(QWidget* parent, QAction* action, std::string_view label) -> QToolButton* {
    auto* button = new QToolButton(parent);
    button->setObjectName(QStringLiteral("vertexNoteQtGeometryPanelButton"));
    button->setDefaultAction(action);
    button->setText(QString::fromUtf8(label.data(), static_cast<qsizetype>(label.size())));
    button->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    button->setIconSize(QSize(18, 18));
    button->setAutoRaise(false);
    button->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    QObject::connect(action, &QAction::changed, button, [button, label]() {
        button->setText(QString::fromUtf8(label.data(), static_cast<qsizetype>(label.size())));
    });
    return button;
}

auto makeStatusChip(QWidget* parent, const QString& text) -> QLabel* {
    auto* label = new QLabel(text, parent);
    label->setObjectName(QStringLiteral("vertexNoteQtGeometryPanelStatusChip"));
    label->setMinimumHeight(24);
    label->setAlignment(Qt::AlignVCenter | Qt::AlignLeft);
    label->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    return label;
}

}  // namespace

QtGeometryPanel::QtGeometryPanel(QWidget* parent): QDockWidget(QStringLiteral("Write Workspace"), parent) {
    setObjectName(QStringLiteral("vertexNoteQtGeometryPanel"));
    setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);
    setFeatures(QDockWidget::DockWidgetClosable | QDockWidget::DockWidgetMovable);
    setMinimumWidth(190);
    setMaximumWidth(280);

    auto* scrollArea = new QScrollArea(this);
    scrollArea->setObjectName(QStringLiteral("vertexNoteQtGeometryPanelScrollArea"));
    scrollArea->setWidgetResizable(true);
    scrollArea->setFrameShape(QFrame::NoFrame);
    scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

    this->contentWidget = new QWidget(scrollArea);
    this->contentWidget->setObjectName(QStringLiteral("vertexNoteQtGeometryPanelContent"));
    this->contentLayout = new QVBoxLayout(this->contentWidget);
    this->contentLayout->setContentsMargins(8, 8, 8, 8);
    this->contentLayout->setSpacing(8);

    auto* summaryFrame = new QFrame(this->contentWidget);
    summaryFrame->setObjectName(QStringLiteral("vertexNoteQtGeometryPanelSummary"));
    auto* summaryLayout = new QGridLayout(summaryFrame);
    summaryLayout->setContentsMargins(7, 7, 7, 7);
    summaryLayout->setHorizontalSpacing(5);
    summaryLayout->setVerticalSpacing(5);
    this->modeStatusLabel = makeStatusChip(summaryFrame, QStringLiteral("Mode -"));
    this->snapStatusLabel = makeStatusChip(summaryFrame, QStringLiteral("Snap -"));
    this->selectionStatusLabel = makeStatusChip(summaryFrame, QStringLiteral("Sel -"));
    this->topologyStatusLabel = makeStatusChip(summaryFrame, QStringLiteral("Topology -"));
    this->viewStatusLabel = makeStatusChip(summaryFrame, QStringLiteral("View -"));
    this->projectionStatusLabel = makeStatusChip(summaryFrame, QStringLiteral("3D view Iso"));
    this->depthStatusLabel = makeStatusChip(summaryFrame, QStringLiteral("Depth -"));
    this->depthSpinBox = new QDoubleSpinBox(summaryFrame);
    this->depthSpinBox->setObjectName(QStringLiteral("vertexNoteQtGeometryPanelDepthSpin"));
    this->depthSpinBox->setRange(-10000.0, 10000.0);
    this->depthSpinBox->setDecimals(1);
    this->depthSpinBox->setSingleStep(1.0);
    this->depthSpinBox->setPrefix(QStringLiteral("Z "));
    this->depthSpinBox->setEnabled(false);
    this->depthSpinBox->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    summaryLayout->addWidget(this->modeStatusLabel, 0, 0);
    summaryLayout->addWidget(this->selectionStatusLabel, 1, 0);
    summaryLayout->addWidget(this->snapStatusLabel, 2, 0);
    summaryLayout->addWidget(this->topologyStatusLabel, 3, 0);
    summaryLayout->addWidget(this->viewStatusLabel, 4, 0);
    summaryLayout->addWidget(this->projectionStatusLabel, 5, 0);
    summaryLayout->addWidget(this->depthStatusLabel, 6, 0);
    summaryLayout->addWidget(this->depthSpinBox, 7, 0);
    QObject::connect(this->depthSpinBox, &QDoubleSpinBox::valueChanged, this, [this](double value) {
        if (!this->loadingDepthEditor) {
            Q_EMIT depthEdited(value);
        }
    });
    this->contentLayout->addWidget(summaryFrame);

    auto* sectionsWidget = new QWidget(this->contentWidget);
    sectionsWidget->setObjectName(QStringLiteral("vertexNoteQtGeometryPanelSections"));
    this->sectionsLayout = new QVBoxLayout(sectionsWidget);
    this->sectionsLayout->setContentsMargins(0, 0, 0, 0);
    this->sectionsLayout->setSpacing(8);
    this->sectionsLayout->addStretch();
    this->contentLayout->addWidget(sectionsWidget);

    scrollArea->setWidget(this->contentWidget);
    setWidget(scrollArea);
    updateSummaryVisibility();
}

void QtGeometryPanel::bindCommandHost(QtCommandHost* host) {
    this->commandHost = host;
    rebuildContent();
}

void QtGeometryPanel::setWorkspaceMode(QtWorkspacePanelMode mode) {
    if (this->panelMode == mode) {
        updateSummaryVisibility();
        return;
    }

    this->panelMode = mode;
    rebuildContent();
    updateSummaryVisibility();
}

auto QtGeometryPanel::workspaceMode() const -> QtWorkspacePanelMode { return this->panelMode; }

void QtGeometryPanel::rebuildContent() {
    clearContent();
    if (!this->commandHost || !this->sectionsLayout) {
        return;
    }

    switch (this->panelMode) {
        case QtWorkspacePanelMode::Notes:
            setWindowTitle(QStringLiteral("Write Workspace"));
            addSection(QStringLiteral("Write"),
                       {{"tool.pen", "Pen"},
                        {"tool.highlighter", "Highlight"},
                        {"tool.eraser", "Eraser"},
                        {"tool.text", "Text"}},
                       2);
            addSection(QStringLiteral("Insert"),
                       {{"edit.insert-image", "Image"},
                        {"tool.math-tex", "Math"},
                        {"tool.draw-polyline", "Polyline"},
                        {"tool.draw-shape-recognizer", "Recognize"}},
                       2);
            addSection(QStringLiteral("Select / Move"),
                       {{"tool.select", "Rectangle"},
                        {"tool.select-region", "Region"},
                        {"tool.hand", "Hand"},
                        {"tool.vertical-space", "Space"}},
                       2);
            addSection(QStringLiteral("Page"),
                       {{"page.add", "New page"},
                        {"page.duplicate", "Duplicate"},
                        {"page.delete", "Delete"},
                        {"layer.add-above", "Layer +"},
                        {"layer.rename", "Rename"},
                        {"page.background", "Paper"}},
                       2, false);
            addSection(QStringLiteral("PDF Review"),
                       {{"tool.select-pdf-text-linear", "Text line"},
                        {"tool.select-pdf-text-rect", "Text area"},
                        {"tool.pdf-text-highlight", "Highlight"},
                        {"tool.select-pdf-text-marker-opacity", "Opacity"}},
                       2, false);
            break;
        case QtWorkspacePanelMode::Geometry:
            setWindowTitle(QStringLiteral("Geometry Workspace"));
            addSection(QStringLiteral("Draw"),
                       {{"tool.draw-edge", "Edge"},
                        {"tool.draw-polyline", "Polyline"},
                        {"tool.draw-circle", "Circle"},
                        {"tool.draw-arc", "Arc"}},
                       2);
            addSection(QStringLiteral("Selection"),
                       {{"geometry.selection-mode-vertex", "Vertex"},
                        {"geometry.selection-mode-edge", "Edge"},
                        {"geometry.selection-mode-face", "Face"},
                        {"geometry.selection-mode-object", "Object"}},
                       2);
            addSection(QStringLiteral("View"),
                       {{"view.geometry-wireframe", "Wireframe"},
                        {"view.geometry-highlight-vertices", "Vertices"},
                        {"view.geometry-linked-markers", "Linked"},
                        {"view.geometry-face-fills", "Faces"}},
                       2);
            addSection(QStringLiteral("Transform"),
                       {{"geometry.translate-vertices", "Move"},
                        {"geometry.rotate-selection", "Rotate"},
                        {"geometry.scale-selection", "Scale"}},
                       2);
            addSection(QStringLiteral("Topology"),
                       {{"geometry.weld-selection", "Weld"},
                        {"geometry.detach-selection", "Detach"},
                        {"edit.insert-vertex", "Insert"},
                        {"edit.delete-geometry", "Delete"}},
                       2);
            addSection(QStringLiteral("Faces"),
                       {{"geometry.fill-face", "Fill loop"},
                        {"geometry.delete-face", "Delete face"},
                        {"geometry.split-face", "Split face"},
                        {"geometry.triangulate-face", "Triangles"}},
                       2);
            addSection(QStringLiteral("Constraints"),
                       {{"constraint.coincident", "Coincident"},
                        {"constraint.horizontal", "Horizontal"},
                        {"constraint.vertical", "Vertical"},
                        {"constraint.fixed-length", "Length"},
                        {"constraint.equal-length", "Equal"},
                        {"constraint.fixed-angle", "Angle"},
                        {"constraint.on-edge", "On Edge"},
                        {"constraint.parallel", "Parallel"},
                        {"constraint.perpendicular", "Perp"},
                        {"constraint.delete", "Clear"}},
                       2, false);
            break;
        case QtWorkspacePanelMode::ThreeD:
            setWindowTitle(QStringLiteral("3D Workspace"));
            addSection(QStringLiteral("3D Projection"),
                       {{"geometry.create-3d-box", "Box"},
                        {"geometry.project-3d-isometric", "Iso"},
                        {"geometry.project-3d-front", "Front"},
                        {"geometry.project-3d-top", "Top"},
                        {"geometry.nudge-z-up", "Z +"},
                        {"geometry.nudge-z-down", "Z -"}},
                       2);
            addSection(QStringLiteral("Selection"),
                       {{"geometry.selection-mode-object", "Object"},
                        {"geometry.selection-mode-vertex", "Vertex"},
                        {"geometry.selection-mode-edge", "Edge"},
                        {"geometry.selection-mode-face", "Face"}},
                       2);
            addSection(QStringLiteral("View"),
                       {{"view.geometry-wireframe", "Wireframe"},
                        {"view.geometry-highlight-vertices", "Vertices"},
                        {"view.geometry-linked-markers", "Linked"},
                        {"view.geometry-face-fills", "Faces"}},
                       2);
            addSection(QStringLiteral("Transform"),
                       {{"geometry.translate-vertices", "Move"},
                        {"geometry.rotate-selection", "Rotate"},
                        {"geometry.scale-selection", "Scale"}},
                       2);
            addSection(QStringLiteral("Faces"),
                       {{"geometry.fill-face", "Fill loop"},
                        {"geometry.delete-face", "Delete face"},
                        {"geometry.split-face", "Split face"},
                        {"geometry.triangulate-face", "Triangles"}},
                       2);
            addSection(QStringLiteral("Topology"),
                       {{"geometry.weld-selection", "Weld"},
                        {"geometry.detach-selection", "Detach"},
                        {"edit.insert-vertex", "Insert"},
                        {"edit.delete-geometry", "Delete"}},
                       2, false);
            break;
    }
    this->sectionsLayout->addStretch();
}

void QtGeometryPanel::clearContent() {
    if (!this->sectionsLayout) {
        return;
    }
    while (auto* item = this->sectionsLayout->takeAt(0)) {
        if (auto* widget = item->widget()) {
            widget->deleteLater();
        }
        delete item;
    }
}

void QtGeometryPanel::setStatusSummary(const QString& mode, const QString& snap, const QString& selection,
                                       const QString& view, const QString& projection, const QString& depth) {
    if (this->modeStatusLabel) {
        this->modeStatusLabel->setText(QStringLiteral("Mode %1").arg(mode));
    }
    if (this->snapStatusLabel) {
        this->snapStatusLabel->setText(QStringLiteral("Snap %1").arg(snap));
    }
    if (this->selectionStatusLabel) {
        this->selectionStatusLabel->setText(QStringLiteral("Sel %1").arg(selection));
    }
    if (this->viewStatusLabel) {
        this->viewStatusLabel->setText(QStringLiteral("View %1").arg(view));
    }
    if (this->projectionStatusLabel) {
        this->projectionStatusLabel->setText(QStringLiteral("3D view %1").arg(projection));
    }
    if (this->depthStatusLabel) {
        this->depthStatusLabel->setText(QStringLiteral("Depth %1").arg(depth));
    }
    updateSummaryVisibility();
}

void QtGeometryPanel::setTopologySummary(const QString& topology) {
    if (this->topologyStatusLabel) {
        this->topologyStatusLabel->setText(QStringLiteral("Topology %1").arg(topology));
    }
    updateSummaryVisibility();
}

void QtGeometryPanel::setDepthEditor(double z, bool enabled) {
    if (!this->depthSpinBox) {
        return;
    }

    this->loadingDepthEditor = true;
    const QSignalBlocker blocker(this->depthSpinBox);
    this->depthSpinBox->setEnabled(enabled);
    this->depthSpinBox->setValue(z);
    this->loadingDepthEditor = false;
}

void QtGeometryPanel::updateSummaryVisibility() {
    const bool notes = this->panelMode == QtWorkspacePanelMode::Notes;
    const bool threeD = this->panelMode == QtWorkspacePanelMode::ThreeD;

    if (this->selectionStatusLabel) {
        this->selectionStatusLabel->setVisible(!notes);
    }
    if (this->viewStatusLabel) {
        this->viewStatusLabel->setVisible(!notes);
    }
    if (this->topologyStatusLabel) {
        this->topologyStatusLabel->setVisible(!notes);
    }
    if (this->projectionStatusLabel) {
        this->projectionStatusLabel->setVisible(threeD);
    }
    if (this->depthStatusLabel) {
        this->depthStatusLabel->setVisible(threeD);
    }
    if (this->depthSpinBox) {
        this->depthSpinBox->setVisible(threeD);
    }
}

void QtGeometryPanel::addSection(const QString& title, std::initializer_list<PanelCommand> commands, int columns,
                                 bool expanded) {
    if (!this->commandHost || !this->sectionsLayout || columns <= 0) {
        return;
    }

    auto* section = new QFrame(this->contentWidget);
    section->setObjectName(QStringLiteral("vertexNoteQtGeometryPanelSection"));
    auto* sectionLayout = new QVBoxLayout(section);
    sectionLayout->setContentsMargins(0, 0, 0, 0);
    sectionLayout->setSpacing(0);

    auto* headerButton = new QToolButton(section);
    headerButton->setObjectName(QStringLiteral("vertexNoteQtGeometryPanelSectionHeader"));
    headerButton->setText(title);
    headerButton->setCheckable(true);
    headerButton->setChecked(expanded);
    headerButton->setArrowType(expanded ? Qt::DownArrow : Qt::RightArrow);
    headerButton->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    headerButton->setAutoRaise(false);
    headerButton->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    sectionLayout->addWidget(headerButton);

    auto* content = new QWidget(section);
    content->setObjectName(QStringLiteral("vertexNoteQtGeometryPanelSectionContent"));
    auto* grid = new QGridLayout(content);
    grid->setContentsMargins(7, 7, 7, 7);
    grid->setHorizontalSpacing(5);
    grid->setVerticalSpacing(5);

    int added = 0;
    for (const auto& command: commands) {
        auto* action = this->commandHost->actionForCommand(command.id);
        if (!action) {
            continue;
        }
        auto* button = makePanelButton(content, action, command.label);
        const int row = added / columns;
        const int column = added % columns;
        grid->addWidget(button, row, column);
        ++added;
    }

    if (added == 0) {
        section->deleteLater();
        return;
    }

    content->setVisible(expanded);
    QObject::connect(headerButton, &QToolButton::toggled, content, [headerButton, content](bool checked) {
        content->setVisible(checked);
        headerButton->setArrowType(checked ? Qt::DownArrow : Qt::RightArrow);
    });
    sectionLayout->addWidget(content);

    this->sectionsLayout->addWidget(section);
}
