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
#include <QIcon>
#include <QPainter>
#include <QPen>
#include <QPixmap>
#include <QRectF>
#include <QSize>
#include <QSignalBlocker>
#include <QSpinBox>
#include <QString>
#include <QToolButton>

namespace {

auto toolbarSyncQColorFromColor(Color color) -> QColor {
    return QColor(color.red, color.green, color.blue, color.alpha);
}

void refreshFamilyToolButtonChrome(QToolButton* button) {
    if (!button) {
        return;
    }
    button->setText(QString());
    button->setArrowType(Qt::NoArrow);
    button->setPopupMode(QToolButton::MenuButtonPopup);
    button->setToolButtonStyle(Qt::ToolButtonIconOnly);
}

void refreshFamilyActionButtonChrome(QToolButton* button) {
    if (!button) {
        return;
    }
    button->setText(QString());
    button->setMenu(nullptr);
    button->setArrowType(Qt::NoArrow);
    button->setPopupMode(QToolButton::DelayedPopup);
    button->setToolButtonStyle(Qt::ToolButtonIconOnly);
}

auto makeSwatchIcon(Color color, int diameter) -> QIcon {
    const int pixmapSize = diameter + 4;
    QPixmap pixmap(pixmapSize, pixmapSize);
    pixmap.fill(Qt::transparent);

    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing, true);
    const QColor qcolor = toolbarSyncQColorFromColor(color);
    const QColor outline = qcolor.lightness() > 220 ? QColor(110, 110, 110) : QColor(35, 35, 35, 180);
    painter.setPen(QPen(outline, 1.0));
    painter.setBrush(qcolor);
    painter.drawEllipse(QRectF(2.0, 2.0, static_cast<double>(diameter), static_cast<double>(diameter)));
    return QIcon(pixmap);
}

}  // namespace

void QtAppShell::syncToolbarWidgets() {
    const auto& toolState = this->window.canvas()->toolState();

    if (this->selectionToolButton) {
        if (auto* action = findActionForTool(this->window.commandHost(), selectionToolSpecs(), toolState.activeTool)) {
            this->selectionToolButton->setDefaultAction(action);
            this->selectionToolButton->setMenu(this->selectionToolButton->menu());
            refreshFamilyToolButtonChrome(this->selectionToolButton);
        }
    }

    if (auto* action = findActionForTool(this->window.commandHost(), strokeDrawingToolSpecs(), toolState.activeTool)) {
        for (auto* button: this->strokeDrawingToolButtons) {
            button->setDefaultAction(action);
            refreshFamilyActionButtonChrome(button);
            button->setToolTip(action->toolTip());
        }
    }

    if (auto* action = findActionForTool(this->window.commandHost(), vertexDrawingToolSpecs(), toolState.activeTool)) {
        for (auto* button: this->vertexDrawingToolButtons) {
            button->setDefaultAction(action);
            refreshFamilyActionButtonChrome(button);
            button->setToolTip(action->toolTip());
        }
    }

    if (this->laserToolButton) {
        if (auto* action = findActionForTool(this->window.commandHost(), laserToolSpecs(), toolState.activeTool)) {
            this->laserToolButton->setDefaultAction(action);
            this->laserToolButton->setMenu(this->laserToolButton->menu());
            refreshFamilyToolButtonChrome(this->laserToolButton);
        }
    }

    if (this->pdfToolButton) {
        if (auto* action = findActionForTool(this->window.commandHost(), pdfToolSpecs(), toolState.activeTool)) {
            this->pdfToolButton->setDefaultAction(action);
            this->pdfToolButton->setMenu(this->pdfToolButton->menu());
            refreshFamilyToolButtonChrome(this->pdfToolButton);
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
        button->setFixedSize(selected ? QSize(24, 24) : QSize(20, 20));
        button->setIcon(makeSwatchIcon(color, selected ? 18 : 14));
        button->setIconSize(QSize(selected ? 22 : 18, selected ? 22 : 18));
        button->setStyleSheet(QStringLiteral(
                                      "QToolButton#vertexNoteQtToolbarColorButton {"
                                      " background-color: %1; border-radius: %2px; border: %3; padding: 0px;"
                                      " margin: 0px 4px;"
                                      " min-width: %4px; max-width: %4px; min-height: %4px; max-height: %4px; }")
                                      .arg(selected ? QStringLiteral("#dce8ff") : QStringLiteral("transparent"))
                                      .arg(selected ? 12 : 10)
                                      .arg(selected ? QStringLiteral("1px solid #8db0ff")
                                                    : QStringLiteral("1px solid transparent"))
                                      .arg(selected ? 24 : 20));
    }

    if (this->toolbarColorSelectButton) {
        this->toolbarColorSelectButton->setFixedSize(26, 26);
        this->toolbarColorSelectButton->setIcon(makeSwatchIcon(selectedColor, 18));
        this->toolbarColorSelectButton->setIconSize(QSize(22, 22));
        this->toolbarColorSelectButton->setStyleSheet(
                QStringLiteral("QToolButton#vertexNoteQtToolbarColorSelectButton {"
                               " background-color: #dce8ff; border: 1px solid #8db0ff; border-radius: 13px;"
                               " margin: 0px 5px;"
                               " min-width: 26px; max-width: 26px; min-height: 26px; max-height: 26px; padding: 0px; }"));
    }
}
