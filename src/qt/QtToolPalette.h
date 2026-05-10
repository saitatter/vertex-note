/*
 * VertexNote
 *
 * Qt tool palette widget for the toolbar — colour swatch, pen width, pressure toggle.
 */

#pragma once

#include <QWidget>

#include <vector>

#include "QtToolState.h"

class QDoubleSpinBox;
class QToolButton;
class QCheckBox;
class QLabel;

class QtToolPalette: public QWidget {
    Q_OBJECT

public:
    explicit QtToolPalette(QWidget* parent = nullptr);

    void setCompactToolbarMode(bool compact);
    void syncFromToolState(const QtToolState& state);

Q_SIGNALS:
    void colorChanged(Color newColor);
    void widthChanged(double newWidth);
    void pressureToggled(bool enabled);
    void eraserModeChanged(QtEraserMode mode);

private:
    void pickColor();
    void updateSwatchColor();
    void selectPresetColor(Color color);
    void updatePresetButtons();

private:
    QToolButton* colorSwatch = nullptr;
    QLabel* widthLabel = nullptr;
    QDoubleSpinBox* widthSpinner = nullptr;
    QCheckBox* pressureCheck = nullptr;
    QCheckBox* segmentCheck = nullptr;
    std::vector<QToolButton*> presetButtons;
    Color currentColor{0x33, 0x33, 0xcc, 0xff};
    QtToolType currentTool = QtToolType::Hand;
    bool compactToolbarMode = false;
};
