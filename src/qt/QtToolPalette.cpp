/*
 * VertexNote
 *
 * Qt tool palette implementation — colour, width, pressure controls.
 */

#include "QtToolPalette.h"

#include <QCheckBox>
#include <QColorDialog>
#include <QDoubleSpinBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QToolButton>

QtToolPalette::QtToolPalette(QWidget* parent): QWidget(parent) {
    auto* layout = new QHBoxLayout(this);
    layout->setContentsMargins(4, 0, 4, 0);
    layout->setSpacing(6);

    // Colour swatch button
    this->colorSwatch = new QToolButton(this);
    this->colorSwatch->setFixedSize(24, 24);
    this->colorSwatch->setToolTip(QStringLiteral("Stroke colour"));
    updateSwatchColor();
    connect(this->colorSwatch, &QToolButton::clicked, this, &QtToolPalette::pickColor);
    layout->addWidget(this->colorSwatch);

    // Width spinner
    layout->addWidget(new QLabel(QStringLiteral("W:"), this));
    this->widthSpinner = new QDoubleSpinBox(this);
    this->widthSpinner->setRange(0.1, 50.0);
    this->widthSpinner->setSingleStep(0.5);
    this->widthSpinner->setDecimals(2);
    this->widthSpinner->setValue(1.41);
    this->widthSpinner->setToolTip(QStringLiteral("Stroke width"));
    this->widthSpinner->setFixedWidth(70);
    connect(this->widthSpinner, &QDoubleSpinBox::valueChanged, this, &QtToolPalette::widthChanged);
    layout->addWidget(this->widthSpinner);

    // Pressure toggle
    this->pressureCheck = new QCheckBox(QStringLiteral("Pressure"), this);
    this->pressureCheck->setChecked(true);
    this->pressureCheck->setToolTip(QStringLiteral("Enable pressure sensitivity"));
    connect(this->pressureCheck, &QCheckBox::toggled, this, &QtToolPalette::pressureToggled);
    layout->addWidget(this->pressureCheck);

    setLayout(layout);
}

void QtToolPalette::syncFromToolState(const QtToolState& state) {
    this->currentTool = state.activeTool;

    // Show palette only for drawing tools
    const bool visible = state.activeTool == QtToolType::Pen || state.activeTool == QtToolType::Highlighter ||
                         state.activeTool == QtToolType::Eraser;
    setVisible(visible);

    if (!visible) {
        return;
    }

    // Block signals to avoid feedback loops
    const QSignalBlocker widthBlock(this->widthSpinner);
    const QSignalBlocker pressureBlock(this->pressureCheck);

    switch (state.activeTool) {
        case QtToolType::Pen:
            this->currentColor = state.penColor;
            this->widthSpinner->setValue(state.penWidth);
            this->colorSwatch->setVisible(true);
            this->pressureCheck->setVisible(true);
            this->pressureCheck->setChecked(state.pressureSensitive);
            break;
        case QtToolType::Highlighter:
            this->currentColor = state.highlighterColor;
            this->widthSpinner->setValue(state.highlighterWidth);
            this->colorSwatch->setVisible(true);
            this->pressureCheck->setVisible(false);
            break;
        case QtToolType::Eraser:
            this->widthSpinner->setValue(state.eraserWidth);
            this->colorSwatch->setVisible(false);
            this->pressureCheck->setVisible(false);
            break;
        default:
            break;
    }

    updateSwatchColor();
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
    Q_EMIT colorChanged(this->currentColor);
}

void QtToolPalette::updateSwatchColor() {
    const QColor qc(this->currentColor.red, this->currentColor.green, this->currentColor.blue,
                    this->currentColor.alpha);
    this->colorSwatch->setStyleSheet(
            QStringLiteral("QToolButton { background-color: %1; border: 1px solid #888; border-radius: 3px; }")
                    .arg(qc.name(QColor::HexArgb)));
}
