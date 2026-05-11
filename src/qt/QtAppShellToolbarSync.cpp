/*
 * VertexNote
 *
 * Qt app shell toolbar widget synchronization.
 */

#include "QtAppShell.h"

#include "QtColorPalette.h"
#include "QtToolFamilies.h"

#include <cstddef>

#include <QAction>
#include <QColor>
#include <QDoubleSpinBox>
#include <QFont>
#include <QFontComboBox>
#include <QSignalBlocker>
#include <QSpinBox>
#include <QString>
#include <QToolButton>

namespace {

auto toolbarSyncQColorFromColor(Color color) -> QColor {
    return QColor(color.red, color.green, color.blue, color.alpha);
}

}  // namespace

void QtAppShell::syncToolbarWidgets() {
    const auto& toolState = this->window.canvas()->toolState();

    if (this->selectionToolButton) {
        if (auto* action = findActionForTool(this->window.commandHost(), selectionToolSpecs(), toolState.activeTool)) {
            this->selectionToolButton->setDefaultAction(action);
            this->selectionToolButton->setMenu(this->selectionToolButton->menu());
            this->selectionToolButton->setPopupMode(QToolButton::MenuButtonPopup);
        }
    }

    if (auto* action = findActionForTool(this->window.commandHost(), strokeDrawingToolSpecs(), toolState.activeTool)) {
        for (auto* button: this->strokeDrawingToolButtons) {
            button->setDefaultAction(action);
            button->setMenu(button->menu());
            button->setPopupMode(QToolButton::MenuButtonPopup);
            button->setToolTip(QStringLiteral("Stroke drawing tools"));
        }
    }

    if (auto* action = findActionForTool(this->window.commandHost(), vertexDrawingToolSpecs(), toolState.activeTool)) {
        for (auto* button: this->vertexDrawingToolButtons) {
            button->setDefaultAction(action);
            button->setMenu(button->menu());
            button->setPopupMode(QToolButton::MenuButtonPopup);
            button->setToolTip(QStringLiteral("Vertex drawing tools"));
        }
    }

    if (this->laserToolButton) {
        if (auto* action = findActionForTool(this->window.commandHost(), laserToolSpecs(), toolState.activeTool)) {
            this->laserToolButton->setDefaultAction(action);
            this->laserToolButton->setMenu(this->laserToolButton->menu());
            this->laserToolButton->setPopupMode(QToolButton::MenuButtonPopup);
        }
    }

    if (this->pdfToolButton) {
        if (auto* action = findActionForTool(this->window.commandHost(), pdfToolSpecs(), toolState.activeTool)) {
            this->pdfToolButton->setDefaultAction(action);
            this->pdfToolButton->setMenu(this->pdfToolButton->menu());
            this->pdfToolButton->setPopupMode(QToolButton::MenuButtonPopup);
        }
    }

    if (this->fontFamilyCombo) {
        const QSignalBlocker blocker(this->fontFamilyCombo);
        this->fontFamilyCombo->setCurrentFont(QFont(QString::fromStdString(toolState.fontName)));
    }

    if (this->fontSizeSpinner) {
        const QSignalBlocker blocker(this->fontSizeSpinner);
        this->fontSizeSpinner->setValue(toolState.fontSize);
    }

    if (this->toolbarFillAction) {
        const QSignalBlocker blocker(this->toolbarFillAction);
        const bool checked = toolState.activeTool == QtToolType::Highlighter ||
                                             toolState.activeTool == QtToolType::LaserPointerHighlighter
                                     ? toolState.highlighterFillEnabled
                                     : toolState.fillEnabled;
        this->toolbarFillAction->setChecked(checked);
    }

    if (this->fillOpacitySpinner) {
        const QSignalBlocker blocker(this->fillOpacitySpinner);
        this->fillOpacitySpinner->setValue(toolState.fillOpacity);
    }

    const Color selectedColor = toolState.activeTool == QtToolType::Highlighter ||
                                                toolState.activeTool == QtToolType::LaserPointerHighlighter
                                        ? toolState.highlighterColor
                                        : toolState.penColor;
    for (auto* button: this->toolbarColorButtons) {
        if (!button) {
            continue;
        }

        const auto colorIndex = button->property("toolbarColorIndex").toInt();
        if (colorIndex < 0) {
            continue;
        }

        Color color;
        if (this->activeColorPalette.empty()) {
            const auto palette = qtDefaultColorPalette();
            color = palette[static_cast<std::size_t>(colorIndex) % palette.size()].color;
        } else {
            color = this->activeColorPalette[static_cast<std::size_t>(colorIndex) % this->activeColorPalette.size()].color;
        }
        const bool selected = color == selectedColor;
        button->setStyleSheet(QStringLiteral(
                                      "QToolButton { background-color: %1; border-radius: 7px; border: %2; padding: 0px; }")
                                      .arg(toolbarSyncQColorFromColor(color).name(QColor::HexArgb))
                                      .arg(selected ? QStringLiteral("2px solid #2f66ff")
                                                    : QStringLiteral("1px solid #a0a0a0")));
    }

    if (this->toolbarColorSelectButton) {
        this->toolbarColorSelectButton->setStyleSheet(
                QStringLiteral("QToolButton { background-color: %1; border: 1px solid #8d8d8d; border-radius: 3px; }")
                        .arg(toolbarSyncQColorFromColor(selectedColor).name(QColor::HexArgb)));
    }
}
