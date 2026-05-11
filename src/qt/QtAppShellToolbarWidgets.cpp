/*
 * VertexNote
 *
 * Qt app shell toolbar widget factories.
 */

#include "QtAppShell.h"

#include <cstddef>
#include <cstdint>
#include <string>

#include <QAction>
#include <QColor>
#include <QColorDialog>
#include <QDoubleSpinBox>
#include <QFont>
#include <QFontComboBox>
#include <QMenu>
#include <QObject>
#include <QSize>
#include <QSizePolicy>
#include <QSpinBox>
#include <QString>
#include <QToolBar>
#include <QToolButton>
#include <QWidget>

#include "QtColorPalette.h"
#include "QtIconResources.h"
#include "QtToolFamilies.h"
#include "QtToolbarLayoutEngine.h"
#include "QtToolbarProfileStore.h"

namespace {

auto qColorFromColor(Color color) -> QColor { return QColor(color.red, color.green, color.blue, color.alpha); }

}  // namespace

void QtAppShell::resetToolbarWidgetState() {
    this->selectionToolButton = nullptr;
    this->strokeDrawingToolButtons.clear();
    this->vertexDrawingToolButtons.clear();
    this->laserToolButton = nullptr;
    this->pdfToolButton = nullptr;
    this->fontFamilyCombo = nullptr;
    this->fontSizeSpinner = nullptr;
    this->toolbarFillAction = nullptr;
    this->toolbarColorSelectButton = nullptr;
    this->toolbarColorButtons.clear();
}

void QtAppShell::loadActiveToolbarProfile() {
    const auto wantedToolbarProfile =
            this->currentSettings.toolbarProfileId.empty() ? std::string(QT_GTK_PARITY_PROFILE_ID)
                                                           : this->currentSettings.toolbarProfileId;
    this->activeToolbarProfile = wantedToolbarProfile == QT_CUSTOM_PROFILE_ID
                                         ? customToolbarProfileFromSettings()
                                         : QtToolbarLayoutEngine::loadProfile(toolbarProfilePath(), wantedToolbarProfile);
    if (!this->activeToolbarProfile) {
        this->activeToolbarProfile = QtToolbarLayoutEngine::loadProfile(toolbarProfilePath(), QT_GTK_PARITY_PROFILE_ID);
    }
}

void QtAppShell::configureToolbarChrome() {
    auto* documentToolBar = this->window.mainToolBar();
    auto* toolsToolBar = this->window.toolsToolBar();
    auto* footerToolBar = this->window.footerToolBar();
    auto* leftPrimaryToolBar = this->window.leftPrimaryToolBar();
    auto* leftSecondaryToolBar = this->window.leftSecondaryToolBar();
    auto* rightPrimaryToolBar = this->window.rightPrimaryToolBar();

    documentToolBar->setToolButtonStyle(Qt::ToolButtonIconOnly);
    toolsToolBar->setToolButtonStyle(Qt::ToolButtonIconOnly);
    footerToolBar->setToolButtonStyle(Qt::ToolButtonIconOnly);
    leftPrimaryToolBar->setToolButtonStyle(Qt::ToolButtonIconOnly);
    leftSecondaryToolBar->setToolButtonStyle(Qt::ToolButtonIconOnly);
    footerToolBar->setIconSize(QSize(20, 20));
    leftPrimaryToolBar->setIconSize(QSize(22, 22));
    leftSecondaryToolBar->setIconSize(QSize(22, 22));
    rightPrimaryToolBar->setToolButtonStyle(Qt::ToolButtonIconOnly);
    rightPrimaryToolBar->setIconSize(QSize(22, 22));
    for (auto* floatingToolBar: this->window.floatingToolBars()) {
        floatingToolBar->setToolButtonStyle(Qt::ToolButtonIconOnly);
        floatingToolBar->setIconSize(QSize(22, 22));
        floatingToolBar->hide();
    }

    applyQtCommandIcons(this->window);
}

void QtAppShell::addToolbarCommand(QToolBar* toolbar, std::string_view commandId) {
    if (auto* action = this->window.commandHost()->actionForCommand(commandId)) {
        toolbar->addAction(action);
    }
}

void QtAppShell::addGenericSizeToolbarAction(QToolBar* toolbar, const char* text, const char* iconFile, int sizeIndex) {
    auto* action = new QAction(QString::fromUtf8(text), toolbar);
    action->setToolTip(QString::fromUtf8(text));
    action->setIcon(bundledQtIcon(iconFile));
    QObject::connect(action, &QAction::triggered, toolbar, [this, sizeIndex]() {
        switch (this->window.canvas()->activeTool()) {
            case QtToolType::Eraser:
                setEraserSize(sizeIndex);
                break;
            case QtToolType::Highlighter:
                setHighlighterSize(sizeIndex);
                break;
            default:
                setPenSize(sizeIndex);
                break;
        }
    });
    toolbar->addAction(action);
}

void QtAppShell::addFillToolbarAction(QToolBar* toolbar) {
    auto* action = new QAction(QStringLiteral("Fill"), toolbar);
    action->setToolTip(QStringLiteral("Toggle fill"));
    action->setCheckable(true);
    action->setIcon(bundledQtIcon("xopp-fill.svg"));
    QObject::connect(action, &QAction::triggered, toolbar, [this, action]() {
        auto& ts = this->window.canvas()->toolState();
        if (this->window.canvas()->activeTool() == QtToolType::Highlighter) {
            ts.highlighterFillEnabled = !ts.highlighterFillEnabled;
            action->setChecked(ts.highlighterFillEnabled);
        } else {
            ts.fillEnabled = !ts.fillEnabled;
            action->setChecked(ts.fillEnabled);
        }
    });
    action->setChecked(this->window.canvas()->toolState().fillEnabled);
    toolbar->addAction(action);
    this->toolbarFillAction = action;
}

void QtAppShell::addStretchToolbarSpacer(QToolBar* toolbar) {
    auto* spacer = new QWidget(toolbar);
    spacer->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    toolbar->addWidget(spacer);
}

auto QtAppShell::ensureSelectionToolButton() -> QToolButton* {
    if (!this->selectionToolButton) {
        this->selectionToolButton = new QToolButton(&this->window);
        this->selectionToolButton->setObjectName(QStringLiteral("vertexNoteQtFamilyToolButton"));
        this->selectionToolButton->setPopupMode(QToolButton::MenuButtonPopup);
        this->selectionToolButton->setIcon(bundledQtIcon("xopp-combo-selection.svg"));
        auto* selectionMenu = new QMenu(this->selectionToolButton);
        for (const auto& spec: selectionToolSpecs()) {
            if (auto* action = this->window.commandHost()->actionForCommand(spec.commandId)) {
                selectionMenu->addAction(action);
            }
        }
        this->selectionToolButton->setMenu(selectionMenu);
    }
    return this->selectionToolButton;
}

auto QtAppShell::createStrokeDrawingToolButton() -> QToolButton* {
    auto* button = new QToolButton(&this->window);
    button->setObjectName(QStringLiteral("vertexNoteQtFamilyToolButton"));
    button->setPopupMode(QToolButton::MenuButtonPopup);
    button->setIcon(bundledQtIcon("xopp-combo-drawing-type.svg"));
    button->setToolTip(QStringLiteral("Stroke drawing tools"));
    auto* drawingMenu = new QMenu(button);
    for (const auto& spec: strokeDrawingToolSpecs()) {
        if (auto* action = this->window.commandHost()->actionForCommand(spec.commandId)) {
            drawingMenu->addAction(action);
        }
    }
    if (auto* action = this->window.commandHost()->actionForCommand("tool.draw-line")) {
        button->setDefaultAction(action);
    }
    button->setMenu(drawingMenu);
    button->setToolTip(QStringLiteral("Stroke drawing tools"));
    this->strokeDrawingToolButtons.push_back(button);
    return button;
}

auto QtAppShell::createVertexDrawingToolButton() -> QToolButton* {
    auto* button = new QToolButton(&this->window);
    button->setObjectName(QStringLiteral("vertexNoteQtFamilyToolButton"));
    button->setPopupMode(QToolButton::MenuButtonPopup);
    button->setIcon(bundledQtIcon("xopp-draw-coordinate-system.svg"));
    button->setToolTip(QStringLiteral("Vertex drawing tools"));
    auto* drawingMenu = new QMenu(button);
    for (const auto& spec: vertexDrawingToolSpecs()) {
        if (auto* action = this->window.commandHost()->actionForCommand(spec.commandId)) {
            drawingMenu->addAction(action);
        }
    }
    if (auto* action = this->window.commandHost()->actionForCommand("tool.draw-circle")) {
        button->setDefaultAction(action);
    }
    button->setMenu(drawingMenu);
    button->setToolTip(QStringLiteral("Vertex drawing tools"));
    this->vertexDrawingToolButtons.push_back(button);
    return button;
}

auto QtAppShell::ensureLaserToolButton() -> QToolButton* {
    if (!this->laserToolButton) {
        this->laserToolButton = new QToolButton(&this->window);
        this->laserToolButton->setObjectName(QStringLiteral("vertexNoteQtFamilyToolButton"));
        this->laserToolButton->setPopupMode(QToolButton::MenuButtonPopup);
        this->laserToolButton->setIcon(bundledQtIcon("xopp-laser-pointer.svg"));
        auto* laserMenu = new QMenu(this->laserToolButton);
        for (const auto& spec: laserToolSpecs()) {
            if (auto* action = this->window.commandHost()->actionForCommand(spec.commandId)) {
                laserMenu->addAction(action);
            }
        }
        this->laserToolButton->setMenu(laserMenu);
    }
    return this->laserToolButton;
}

auto QtAppShell::ensurePdfToolButton() -> QToolButton* {
    if (!this->pdfToolButton) {
        this->pdfToolButton = new QToolButton(&this->window);
        this->pdfToolButton->setObjectName(QStringLiteral("vertexNoteQtFamilyToolButton"));
        this->pdfToolButton->setPopupMode(QToolButton::MenuButtonPopup);
        this->pdfToolButton->setIcon(bundledQtIcon("xopp-select-pdf-text-ht.svg"));
        auto* pdfMenu = new QMenu(this->pdfToolButton);
        for (const auto& spec: pdfToolSpecs()) {
            if (auto* action = this->window.commandHost()->actionForCommand(spec.commandId)) {
                pdfMenu->addAction(action);
            }
        }
        this->pdfToolButton->setMenu(pdfMenu);
    }
    return this->pdfToolButton;
}

void QtAppShell::ensureFontToolbarWidgets() {
    if (!this->fontFamilyCombo) {
        this->fontFamilyCombo = new QFontComboBox(&this->window);
        this->fontFamilyCombo->setObjectName(QStringLiteral("vertexNoteQtFontFamilyCombo"));
        this->fontFamilyCombo->setMaximumWidth(140);
        QObject::connect(this->fontFamilyCombo, &QFontComboBox::currentFontChanged, &this->window,
                         [this](const QFont& font) {
                             this->window.canvas()->toolState().fontName = font.family().toStdString();
                         });
    }
    if (!this->fontSizeSpinner) {
        this->fontSizeSpinner = new QDoubleSpinBox(&this->window);
        this->fontSizeSpinner->setObjectName(QStringLiteral("vertexNoteQtFontSizeSpinner"));
        this->fontSizeSpinner->setRange(6.0, 96.0);
        this->fontSizeSpinner->setDecimals(0);
        this->fontSizeSpinner->setSingleStep(1.0);
        this->fontSizeSpinner->setFixedWidth(56);
        QObject::connect(this->fontSizeSpinner, &QDoubleSpinBox::valueChanged, &this->window,
                         [this](double size) { this->window.canvas()->toolState().fontSize = size; });
    }
}

auto QtAppShell::ensureFillOpacityWidget() -> QSpinBox* {
    if (!this->fillOpacitySpinner) {
        this->fillOpacitySpinner = new QSpinBox(&this->window);
        this->fillOpacitySpinner->setObjectName(QStringLiteral("vertexNoteQtFillOpacitySpinner"));
        this->fillOpacitySpinner->setRange(0, 255);
        this->fillOpacitySpinner->setSingleStep(8);
        this->fillOpacitySpinner->setPrefix(QStringLiteral("A "));
        this->fillOpacitySpinner->setToolTip(QStringLiteral("Fill opacity"));
        this->fillOpacitySpinner->setFixedWidth(78);
        QObject::connect(this->fillOpacitySpinner, &QSpinBox::valueChanged, &this->window,
                         [this](int value) { setStrokeFill(value); });
    }
    return this->fillOpacitySpinner;
}

auto QtAppShell::makeToolbarColorButton(int colorIndex) -> QToolButton* {
    auto* button = new QToolButton(&this->window);
    button->setAutoRaise(true);
    button->setFixedSize(20, 20);
    button->setProperty("toolbarColorIndex", colorIndex);
    button->setToolTip(QStringLiteral("Quick colour"));
    QObject::connect(button, &QToolButton::clicked, &this->window,
                     [this, colorIndex]() { applyToolbarColor(toolbarColorAt(colorIndex)); });
    this->toolbarColorButtons.push_back(button);
    return button;
}

auto QtAppShell::ensureToolbarColorSelectButton() -> QToolButton* {
    if (!this->toolbarColorSelectButton) {
        this->toolbarColorSelectButton = new QToolButton(&this->window);
        this->toolbarColorSelectButton->setAutoRaise(true);
        this->toolbarColorSelectButton->setFixedSize(21, 21);
        this->toolbarColorSelectButton->setToolTip(QStringLiteral("Choose colour"));
        QObject::connect(this->toolbarColorSelectButton, &QToolButton::clicked, &this->window, [this]() {
            const QColor initial = qColorFromColor(currentToolbarStrokeColor());
            const QColor chosen = QColorDialog::getColor(initial, &this->window, QStringLiteral("Stroke Colour"),
                                                         QColorDialog::ShowAlphaChannel);
            if (chosen.isValid()) {
                applyToolbarColor(Color{static_cast<uint8_t>(chosen.red()), static_cast<uint8_t>(chosen.green()),
                                        static_cast<uint8_t>(chosen.blue()), static_cast<uint8_t>(chosen.alpha())});
            }
        });
    }
    return this->toolbarColorSelectButton;
}

auto QtAppShell::currentToolbarStrokeColor() const -> Color {
    const auto& toolState = this->window.canvas()->toolState();
    return toolState.activeTool == QtToolType::Highlighter ||
                   toolState.activeTool == QtToolType::LaserPointerHighlighter
           ? toolState.highlighterColor
           : toolState.penColor;
}

auto QtAppShell::toolbarColorAt(int colorIndex) const -> Color {
    if (this->activeColorPalette.empty()) {
        const auto palette = qtDefaultColorPalette();
        return palette[static_cast<std::size_t>(colorIndex) % palette.size()].color;
    }
    return this->activeColorPalette[static_cast<std::size_t>(colorIndex) % this->activeColorPalette.size()].color;
}

void QtAppShell::applyToolbarColor(Color color) {
    auto& toolState = this->window.canvas()->toolState();
    if (toolState.activeTool == QtToolType::Highlighter ||
        toolState.activeTool == QtToolType::LaserPointerHighlighter) {
        toolState.highlighterColor = color;
    } else {
        toolState.penColor = color;
    }
    this->window.toolPalette()->syncFromToolState(toolState);
    syncToolbarWidgets();
}
