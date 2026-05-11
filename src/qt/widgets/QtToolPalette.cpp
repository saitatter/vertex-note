/*
 * VertexNote
 *
 * Qt tool palette implementation — colour, width, pressure controls.
 */

#include "QtToolPalette.h"

#include "QtColorPalette.h"

#include <algorithm>
#include <utility>

#include <QCheckBox>
#include <QColorDialog>
#include <QDoubleSpinBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QSignalBlocker>
#include <QToolButton>

namespace {

auto colorToHex(const Color& color) -> QString {
    return QColor(color.red, color.green, color.blue, color.alpha).name(QColor::HexArgb);
}

}  // namespace

QtToolPalette::QtToolPalette(QWidget* parent): QWidget(parent) {
    setObjectName(QStringLiteral("vertexNoteQtToolPalette"));
    this->quickColors = qtPaletteColorsOnly(qtDefaultColorPalette());
    auto* layout = new QHBoxLayout(this);
    layout->setContentsMargins(2, 0, 2, 0);
    layout->setSpacing(2);

    // Colour swatch button
    this->colorSwatch = new QToolButton(this);
    this->colorSwatch->setObjectName(QStringLiteral("vertexNoteQtPrimaryColorSwatch"));
    this->colorSwatch->setFixedSize(24, 24);
    this->colorSwatch->setToolTip(QStringLiteral("Stroke colour"));
    updateSwatchColor();
    connect(this->colorSwatch, &QToolButton::clicked, this, &QtToolPalette::pickColor);
    layout->addWidget(this->colorSwatch);

    for (std::size_t colorIndex = 0; colorIndex < this->quickColors.size(); ++colorIndex) {
        auto* button = new QToolButton(this);
        button->setObjectName(QStringLiteral("vertexNoteQtQuickColor"));
        button->setFixedSize(17, 17);
        button->setAutoRaise(true);
        button->setToolTip(QStringLiteral("Quick colour"));
        button->setProperty("quickColorIndex", static_cast<int>(colorIndex));
        connect(button, &QToolButton::clicked, this, [this, button]() {
            const auto index = static_cast<std::size_t>(button->property("quickColorIndex").toInt());
            if (index < this->quickColors.size()) {
                selectPresetColor(this->quickColors[index]);
            }
        });
        this->presetButtons.push_back(button);
        layout->addWidget(button);
    }

    // Width spinner
    this->widthLabel = new QLabel(QStringLiteral("W:"), this);
    this->widthLabel->setObjectName(QStringLiteral("vertexNoteQtToolMetricLabel"));
    layout->addWidget(this->widthLabel);
    this->widthSpinner = new QDoubleSpinBox(this);
    this->widthSpinner->setObjectName(QStringLiteral("vertexNoteQtWidthSpinner"));
    this->widthSpinner->setRange(0.1, 50.0);
    this->widthSpinner->setSingleStep(0.5);
    this->widthSpinner->setDecimals(2);
    this->widthSpinner->setValue(1.41);
    this->widthSpinner->setToolTip(QStringLiteral("Stroke width"));
    this->widthSpinner->setFixedWidth(64);
    connect(this->widthSpinner, &QDoubleSpinBox::valueChanged, this, &QtToolPalette::widthChanged);
    layout->addWidget(this->widthSpinner);

    // Pressure toggle
    this->pressureCheck = new QCheckBox(QStringLiteral("Pressure"), this);
    this->pressureCheck->setObjectName(QStringLiteral("vertexNoteQtToolToggle"));
    this->pressureCheck->setChecked(true);
    this->pressureCheck->setToolTip(QStringLiteral("Enable pressure sensitivity"));
    connect(this->pressureCheck, &QCheckBox::toggled, this, &QtToolPalette::pressureToggled);
    layout->addWidget(this->pressureCheck);

    // Segment eraser toggle (visible only when eraser tool is active)
    this->segmentCheck = new QCheckBox(QStringLiteral("Segment"), this);
    this->segmentCheck->setObjectName(QStringLiteral("vertexNoteQtToolToggle"));
    this->segmentCheck->setChecked(false);
    this->segmentCheck->setToolTip(QStringLiteral("Segment eraser — split strokes instead of deleting whole strokes"));
    this->segmentCheck->setVisible(false);
    connect(this->segmentCheck, &QCheckBox::toggled, this, [this](bool checked) {
        Q_EMIT eraserModeChanged(checked ? QtEraserMode::Segment : QtEraserMode::Standard);
    });
    layout->addWidget(this->segmentCheck);

    setStyleSheet(QStringLiteral(
            "#vertexNoteQtPrimaryColorSwatch { border: 1px solid #8d8d8d; border-radius: 3px; }"
            "#vertexNoteQtQuickColor { border-radius: 7px; border: 1px solid #a0a0a0; padding: 0px; }"
            "#vertexNoteQtToolMetricLabel { color: #4c4c4c; }"
            "#vertexNoteQtWidthSpinner { min-height: 24px; }"
            "#vertexNoteQtToolToggle { spacing: 4px; }"));

    setLayout(layout);
    updatePresetButtons();
}

void QtToolPalette::setCompactToolbarMode(bool compact) {
    this->compactToolbarMode = compact;
    syncFromToolState(QtToolState{
            .activeTool = this->currentTool,
            .penColor = this->currentColor,
            .highlighterColor = this->currentColor,
    });
}

void QtToolPalette::setQuickColors(std::vector<Color> colors) {
    if (colors.empty()) {
        colors = qtPaletteColorsOnly(qtDefaultColorPalette());
    }
    const std::size_t visibleCount = std::min(this->presetButtons.size(), colors.size());
    this->quickColors = std::move(colors);
    for (std::size_t index = 0; index < this->presetButtons.size(); ++index) {
        this->presetButtons[index]->setVisible(index < visibleCount);
    }
    updatePresetButtons();
}

void QtToolPalette::syncFromToolState(const QtToolState& state) {
    this->currentTool = state.activeTool;

    const bool visible = !this->compactToolbarMode &&
                         (state.activeTool == QtToolType::Pen || state.activeTool == QtToolType::Highlighter ||
                          state.activeTool == QtToolType::Eraser);
    setVisible(visible);

    if (!visible) {
        return;
    }

    // Block signals to avoid feedback loops
    const QSignalBlocker widthBlock(this->widthSpinner);
    const QSignalBlocker pressureBlock(this->pressureCheck);
    const QSignalBlocker segmentBlock(this->segmentCheck);

    switch (state.activeTool) {
        case QtToolType::Pen:
            this->currentColor = state.penColor;
            this->widthSpinner->setValue(state.penWidth);
            this->colorSwatch->setVisible(true);
            this->pressureCheck->setVisible(!this->compactToolbarMode);
            this->pressureCheck->setChecked(state.pressureSensitive);
            this->segmentCheck->setVisible(false);
            break;
        case QtToolType::Highlighter:
            this->currentColor = state.highlighterColor;
            this->widthSpinner->setValue(state.highlighterWidth);
            this->colorSwatch->setVisible(true);
            this->pressureCheck->setVisible(false);
            this->segmentCheck->setVisible(false);
            break;
        case QtToolType::Eraser:
            this->widthSpinner->setValue(state.eraserWidth);
            this->colorSwatch->setVisible(this->compactToolbarMode);
            this->pressureCheck->setVisible(false);
            this->segmentCheck->setVisible(!this->compactToolbarMode);
            this->segmentCheck->setChecked(state.eraserMode == QtEraserMode::Segment);
            break;
        default:
            this->currentColor = state.penColor;
            this->colorSwatch->setVisible(true);
            this->pressureCheck->setVisible(false);
            this->segmentCheck->setVisible(false);
            break;
    }

    this->widthLabel->setVisible(!this->compactToolbarMode);
    this->widthSpinner->setVisible(!this->compactToolbarMode);

    updateSwatchColor();
    updatePresetButtons();
}

void QtToolPalette::pickColor() {
    const QColor initial(this->currentColor.red, this->currentColor.green, this->currentColor.blue,
                         this->currentColor.alpha);
    const QColor chosen =
            QColorDialog::getColor(initial, this, QStringLiteral("Stroke Colour"), QColorDialog::ShowAlphaChannel);
    if (!chosen.isValid()) {
        return;
    }

    this->currentColor = Color{static_cast<uint8_t>(chosen.red()), static_cast<uint8_t>(chosen.green()),
                               static_cast<uint8_t>(chosen.blue()), static_cast<uint8_t>(chosen.alpha())};
    updateSwatchColor();
    updatePresetButtons();
    Q_EMIT colorChanged(this->currentColor);
}

void QtToolPalette::updateSwatchColor() {
    this->colorSwatch->setStyleSheet(QStringLiteral("#vertexNoteQtPrimaryColorSwatch { background-color: %1; }")
                                             .arg(colorToHex(this->currentColor)));
}

void QtToolPalette::selectPresetColor(Color color) {
    this->currentColor = color;
    updateSwatchColor();
    updatePresetButtons();
    Q_EMIT colorChanged(this->currentColor);
}

void QtToolPalette::updatePresetButtons() {
    for (std::size_t index = 0; index < this->presetButtons.size() && index < this->quickColors.size(); ++index) {
        const bool selected = this->quickColors[index] == this->currentColor;
        this->presetButtons[index]->setStyleSheet(
                QStringLiteral("#vertexNoteQtQuickColor { background-color: %1; border: %2; }")
                        .arg(colorToHex(this->quickColors[index]))
                        .arg(selected ? QStringLiteral("2px solid #2f66ff") : QStringLiteral("1px solid #a0a0a0")));
    }
}
