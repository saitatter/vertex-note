/*
 * VertexNote
 *
 * Qt settings/preferences dialog.
 */

#pragma once

#include <string>
#include <vector>

#include <QDialog>

#include "QtToolState.h"

class QDoubleSpinBox;
class QSpinBox;
class QCheckBox;
class QComboBox;
class QLineEdit;

struct QtToolbarProfileOption {
    std::string id;
    std::string displayName;
};

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
    bool rotationSnapDefault = false;
    bool touchDrawingDefault = false;
    double strokeRecognizerMinSize = 40.0;
    int laserPointerFadeOutMs = 1500;
    std::string audioFolder;
    double audioSampleRate = 44100.0;
    double audioGain = 1.0;
    int defaultSeekTimeSeconds = 5;
    std::string toolbarProfileId = "All in";
};

class QtSettingsDialog: public QDialog {
    Q_OBJECT

public:
    explicit QtSettingsDialog(const QtSettings& current, const std::vector<QtToolbarProfileOption>& toolbarProfiles,
                              QWidget* parent = nullptr);

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
    QCheckBox* rotationSnapCheck = nullptr;
    QCheckBox* touchDrawingCheck = nullptr;
    QDoubleSpinBox* strokeRecognizerMinSizeSpin = nullptr;
    QSpinBox* laserPointerFadeOutSpin = nullptr;
    QLineEdit* audioFolderEdit = nullptr;
    QDoubleSpinBox* audioSampleRateSpin = nullptr;
    QDoubleSpinBox* audioGainSpin = nullptr;
    QSpinBox* defaultSeekTimeSpin = nullptr;
    QComboBox* toolbarProfileCombo = nullptr;
};
