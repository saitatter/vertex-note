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
class QListWidget;

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
    bool autosaveEnabled = true;
    int autosaveTimeoutMinutes = 3;
    bool geometrySnapDefault = true;
    bool gridSnapDefault = false;
    bool rotationSnapDefault = false;
    bool touchDrawingDefault = false;
    double minimumPressure = 0.05;
    double pressureMultiplier = 1.0;
    bool pressureGuessing = false;
    bool strokeStabilizerEnabled = false;
    int strokeStabilizerSamples = 6;
    double strokeStabilizerStrength = 0.65;
    bool strokeStabilizerFinalizeStroke = true;
    double snapGridTolerance = 0.50;
    double snapGridSize = 14.17;
    double strokeRecognizerMinSize = 40.0;
    int laserPointerFadeOutMs = 1500;
    bool eraserCursorHidden = true;
    QtPointerButtonAction rightButtonAction = QtPointerButtonAction::Eraser;
    QtPointerButtonAction middleButtonAction = QtPointerButtonAction::Pan;
    bool showFilePathInTitlebar = false;
    bool showPageNumberInTitlebar = false;
    bool showPageShadow = true;
    std::string themeVariant = "system";
    std::string colorPalettePath;
    bool autoloadPdfXoj = true;
    std::string defaultPdfExportName = "%{name}_annotated";
    int pdfPageCacheSize = 10;
    int pdfPreloadPagesBefore = 1;
    int pdfPreloadPagesAfter = 1;
    bool pdfEagerPageCleanup = false;
    std::string latexTemplatePath;
    std::string audioFolder;
    double audioSampleRate = 44100.0;
    double audioGain = 1.0;
    int defaultSeekTimeSeconds = 5;
    std::string toolbarProfileId = "Portrait";
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
    QDoubleSpinBox* minimumPressureSpin = nullptr;
    QDoubleSpinBox* pressureMultiplierSpin = nullptr;
    QCheckBox* pressureGuessingCheck = nullptr;
    QCheckBox* strokeStabilizerEnabledCheck = nullptr;
    QSpinBox* strokeStabilizerSamplesSpin = nullptr;
    QDoubleSpinBox* strokeStabilizerStrengthSpin = nullptr;
    QCheckBox* strokeStabilizerFinalizeCheck = nullptr;
    QComboBox* eraserModeCombo = nullptr;
    QDoubleSpinBox* pageWidthSpin = nullptr;
    QDoubleSpinBox* pageHeightSpin = nullptr;
    QSpinBox* undoLimitSpin = nullptr;
    QCheckBox* autosaveEnabledCheck = nullptr;
    QSpinBox* autosaveTimeoutSpin = nullptr;
    QCheckBox* geoSnapCheck = nullptr;
    QCheckBox* gridSnapCheck = nullptr;
    QCheckBox* rotationSnapCheck = nullptr;
    QCheckBox* touchDrawingCheck = nullptr;
    QDoubleSpinBox* snapGridToleranceSpin = nullptr;
    QDoubleSpinBox* snapGridSizeSpin = nullptr;
    QDoubleSpinBox* strokeRecognizerMinSizeSpin = nullptr;
    QSpinBox* laserPointerFadeOutSpin = nullptr;
    QCheckBox* eraserCursorHiddenCheck = nullptr;
    QComboBox* rightButtonActionCombo = nullptr;
    QComboBox* middleButtonActionCombo = nullptr;
    QListWidget* inputDeviceList = nullptr;
    QCheckBox* showFilePathInTitlebarCheck = nullptr;
    QCheckBox* showPageNumberInTitlebarCheck = nullptr;
    QCheckBox* showPageShadowCheck = nullptr;
    QComboBox* themeVariantCombo = nullptr;
    QLineEdit* colorPalettePathEdit = nullptr;
    QCheckBox* autoloadPdfXojCheck = nullptr;
    QLineEdit* defaultPdfExportNameEdit = nullptr;
    QSpinBox* pdfPageCacheSizeSpin = nullptr;
    QSpinBox* pdfPreloadBeforeSpin = nullptr;
    QSpinBox* pdfPreloadAfterSpin = nullptr;
    QCheckBox* pdfEagerCleanupCheck = nullptr;
    QLineEdit* latexTemplatePathEdit = nullptr;
    QLineEdit* audioFolderEdit = nullptr;
    QDoubleSpinBox* audioSampleRateSpin = nullptr;
    QDoubleSpinBox* audioGainSpin = nullptr;
    QSpinBox* defaultSeekTimeSpin = nullptr;
    QComboBox* toolbarProfileCombo = nullptr;
};
