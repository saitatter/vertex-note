/*
 * VertexNote
 *
 * Qt settings/preferences dialog.
 */

#pragma once

#include <QDialog>

#include "QtToolState.h"

class QDoubleSpinBox;
class QSpinBox;
class QCheckBox;
class QComboBox;

struct QtSettings {
    double defaultPenWidth = 1.41;
    double defaultHighlighterWidth = 8.50;
    double defaultEraserWidth = 8.50;
    bool defaultPressureSensitive = true;
    QtEraserMode defaultEraserMode = QtEraserMode::Standard;
    double defaultPageWidth = 595.0;
    double defaultPageHeight = 842.0;
    int undoHistoryLimit = 50;
    bool geometrySnapDefault = true;
    bool gridSnapDefault = false;
};

class QtSettingsDialog: public QDialog {
    Q_OBJECT

public:
    explicit QtSettingsDialog(const QtSettings& current, QWidget* parent = nullptr);

    [[nodiscard]] auto settings() const -> QtSettings;

private:
    QDoubleSpinBox* penWidthSpin = nullptr;
    QDoubleSpinBox* highlighterWidthSpin = nullptr;
    QDoubleSpinBox* eraserWidthSpin = nullptr;
    QCheckBox* pressureCheck = nullptr;
    QComboBox* eraserModeCombo = nullptr;
    QDoubleSpinBox* pageWidthSpin = nullptr;
    QDoubleSpinBox* pageHeightSpin = nullptr;
    QSpinBox* undoLimitSpin = nullptr;
    QCheckBox* geoSnapCheck = nullptr;
    QCheckBox* gridSnapCheck = nullptr;
};
