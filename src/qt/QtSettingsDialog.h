/*
 * VertexNote
 *
 * Qt settings/preferences dialog.
 */

#pragma once

#include <string>
#include <vector>

#include <QColor>
#include <QDialog>

#include "QtToolState.h"

class QDoubleSpinBox;
class QFontComboBox;
class QSpinBox;
class QCheckBox;
class QComboBox;
class QLineEdit;
class QListWidget;
class QTableWidget;
class QPushButton;

struct QtToolbarProfileOption {
    std::string id;
    std::string displayName;
};

struct QtAudioDeviceOption {
    int index = -1;
    std::string displayName;
    bool selected = false;
};

struct QtSettings {
    double defaultPenWidth = 1.41;
    double defaultHighlighterWidth = 8.50;
    double defaultEraserWidth = 8.50;
    std::string defaultFontName = "Sans";
    double defaultFontSize = 12.0;
    bool defaultPressureSensitive = true;
    QtEraserMode defaultEraserMode = QtEraserMode::Standard;
    double defaultPageWidth = 595.0;
    double defaultPageHeight = 842.0;
    int undoHistoryLimit = 50;
    bool autosaveEnabled = true;
    int autosaveTimeoutMinutes = 3;
    bool autoloadMostRecent = false;
    std::string preferredLocale;
    bool automaticUpdateCheckEnabled = false;
    bool presentationModeDefault = false;
    int displayDpi = -1;
    bool addHorizontalSpace = false;
    int addHorizontalSpaceAmountRight = 150;
    int addHorizontalSpaceAmountLeft = 150;
    bool addVerticalSpace = false;
    int addVerticalSpaceAmountAbove = 150;
    int addVerticalSpaceAmountBelow = 150;
    bool geometrySnapDefault = true;
    bool gridSnapDefault = false;
    bool rotationSnapDefault = false;
    double rotationSnapTolerance = 0.30;
    bool drawDirModsEnabled = false;
    int drawDirModsRadius = 50;
    double zoomStepPercent = 10.0;
    double zoomStepScrollPercent = 2.0;
    bool unlimitedScrolling = false;
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
    bool snapRecognizedShapesEnabled = false;
    int laserPointerFadeOutMs = 1500;
    bool useSpacesForTab = false;
    int numberOfSpacesForTab = 4;
    double edgePanSpeed = 20.0;
    double edgePanMaxMult = 5.0;
    bool strokeFilterEnabled = false;
    int strokeFilterIgnoreTime = 150;
    double strokeFilterIgnoreLength = 1.0;
    int strokeFilterSuccessiveTime = 500;
    bool eraserCursorHidden = true;
    QtPointerButtonMatrix buttonMatrix{};
    std::vector<QtInputDeviceButtonProfile> inputDeviceButtonProfiles;
    bool showFilePathInTitlebar = false;
    bool showPageNumberInTitlebar = false;
    bool showPageShadow = true;
    std::string themeVariant = "system";
    std::string iconTheme = "color";
    Color selectionColor{0, 120, 255, 255};
    Color backgroundColor = Colors::xopp_gainsboro02;
    bool recolorMainView = false;
    bool recolorSidebarMiniatures = false;
    Color recolorLight{198, 208, 245, 255};
    Color recolorDark{48, 52, 70, 255};
    std::string colorPalettePath;
    bool autoloadPdfXoj = true;
    std::string defaultPdfExportName = "%{name}_annotated";
    int pdfPageCacheSize = 10;
    int pdfPreloadPagesBefore = 1;
    int pdfPreloadPagesAfter = 1;
    bool pdfEagerPageCleanup = false;
    double pdfPageRerenderThreshold = 5.0;
    std::string emptyLastPageAppend = "disabled";
    std::string sizeUnit = "cm";
    std::string latexTemplatePath;
    bool latexAutoCheckDependencies = true;
    std::string latexDefaultText = "x^2";
    std::string latexGenCmd = "pdflatex -halt-on-error -interaction=nonstopmode '{}'";
    std::string latexSourceViewThemeId;
    bool latexSourceViewAutoIndent = true;
    bool latexSourceViewSyntaxHighlight = true;
    bool latexSourceViewShowLineNumbers = false;
    std::string latexEditorFont = "Monospace 12";
    bool latexUseCustomEditorFont = false;
    bool latexEditorWordWrap = true;
    bool latexUseExternalEditor = false;
    bool latexExternalEditorAutoConfirm = false;
    std::string latexExternalEditorCmd;
    std::string latexTemporaryFileExt = "tex";
    std::string audioFolder;
    std::string lastOpenPath;
    std::string lastSavePath;
    std::string lastImagePath;
    std::string lastPdfPath;
    std::string lastExportPath;
    bool disableAudio = false;
    int audioInputDevice = -1;
    int audioOutputDevice = -1;
    double audioSampleRate = 44100.0;
    double audioGain = 1.0;
    int defaultSeekTimeSeconds = 5;
    std::string toolbarProfileId = "Portrait";
};

class QtSettingsDialog: public QDialog {
    Q_OBJECT

public:
    explicit QtSettingsDialog(const QtSettings& current, const std::vector<QtToolbarProfileOption>& toolbarProfiles,
                              const std::vector<QtAudioDeviceOption>& audioInputDevices = {},
                              const std::vector<QtAudioDeviceOption>& audioOutputDevices = {},
                              QWidget* parent = nullptr);

    [[nodiscard]] auto settings() const -> QtSettings;

private:
    QDoubleSpinBox* penWidthSpin = nullptr;
    QDoubleSpinBox* highlighterWidthSpin = nullptr;
    QDoubleSpinBox* eraserWidthSpin = nullptr;
    QFontComboBox* defaultFontCombo = nullptr;
    QDoubleSpinBox* defaultFontSizeSpin = nullptr;
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
    QComboBox* sizeUnitCombo = nullptr;
    QCheckBox* addHorizontalSpaceCheck = nullptr;
    QSpinBox* addHorizontalSpaceLeftSpin = nullptr;
    QSpinBox* addHorizontalSpaceRightSpin = nullptr;
    QCheckBox* addVerticalSpaceCheck = nullptr;
    QSpinBox* addVerticalSpaceAboveSpin = nullptr;
    QSpinBox* addVerticalSpaceBelowSpin = nullptr;
    QSpinBox* undoLimitSpin = nullptr;
    QCheckBox* autosaveEnabledCheck = nullptr;
    QSpinBox* autosaveTimeoutSpin = nullptr;
    QCheckBox* autoloadMostRecentCheck = nullptr;
    QComboBox* preferredLocaleCombo = nullptr;
    QCheckBox* automaticUpdateCheckEnabledCheck = nullptr;
    QCheckBox* presentationModeDefaultCheck = nullptr;
    QSpinBox* displayDpiSpin = nullptr;
    QCheckBox* geoSnapCheck = nullptr;
    QCheckBox* gridSnapCheck = nullptr;
    QCheckBox* rotationSnapCheck = nullptr;
    QDoubleSpinBox* rotationSnapToleranceSpin = nullptr;
    QCheckBox* drawDirModsEnabledCheck = nullptr;
    QSpinBox* drawDirModsRadiusSpin = nullptr;
    QDoubleSpinBox* zoomStepSpin = nullptr;
    QDoubleSpinBox* zoomStepScrollSpin = nullptr;
    QCheckBox* unlimitedScrollingCheck = nullptr;
    QCheckBox* touchDrawingCheck = nullptr;
    QDoubleSpinBox* snapGridToleranceSpin = nullptr;
    QDoubleSpinBox* snapGridSizeSpin = nullptr;
    QDoubleSpinBox* strokeRecognizerMinSizeSpin = nullptr;
    QCheckBox* snapRecognizedShapesCheck = nullptr;
    QSpinBox* laserPointerFadeOutSpin = nullptr;
    QCheckBox* useSpacesForTabCheck = nullptr;
    QSpinBox* numberOfSpacesForTabSpin = nullptr;
    QDoubleSpinBox* edgePanSpeedSpin = nullptr;
    QDoubleSpinBox* edgePanMaxMultSpin = nullptr;
    QCheckBox* strokeFilterEnabledCheck = nullptr;
    QSpinBox* strokeFilterIgnoreTimeSpin = nullptr;
    QDoubleSpinBox* strokeFilterIgnoreLengthSpin = nullptr;
    QSpinBox* strokeFilterSuccessiveTimeSpin = nullptr;
    QCheckBox* eraserCursorHiddenCheck = nullptr;
    QComboBox* eraserTipActionCombo = nullptr;
    QComboBox* stylusButton1ActionCombo = nullptr;
    QComboBox* stylusButton2ActionCombo = nullptr;
    QComboBox* mouseLeftActionCombo = nullptr;
    QComboBox* mouseMiddleActionCombo = nullptr;
    QComboBox* mouseRightActionCombo = nullptr;
    QComboBox* mouseBackActionCombo = nullptr;
    QComboBox* mouseForwardActionCombo = nullptr;
    QComboBox* touchActionCombo = nullptr;
    QListWidget* inputDeviceList = nullptr;
    QTableWidget* inputDeviceMatrixTable = nullptr;
    QCheckBox* showFilePathInTitlebarCheck = nullptr;
    QCheckBox* showPageNumberInTitlebarCheck = nullptr;
    QCheckBox* showPageShadowCheck = nullptr;
    QComboBox* themeVariantCombo = nullptr;
    QComboBox* iconThemeCombo = nullptr;
    QPushButton* selectionColorButton = nullptr;
    QPushButton* backgroundColorButton = nullptr;
    QCheckBox* recolorMainViewCheck = nullptr;
    QCheckBox* recolorSidebarCheck = nullptr;
    QPushButton* recolorLightButton = nullptr;
    QPushButton* recolorDarkButton = nullptr;
    QLineEdit* colorPalettePathEdit = nullptr;
    QCheckBox* autoloadPdfXojCheck = nullptr;
    QLineEdit* defaultPdfExportNameEdit = nullptr;
    QSpinBox* pdfPageCacheSizeSpin = nullptr;
    QSpinBox* pdfPreloadBeforeSpin = nullptr;
    QSpinBox* pdfPreloadAfterSpin = nullptr;
    QCheckBox* pdfEagerCleanupCheck = nullptr;
    QDoubleSpinBox* pdfPageRerenderThresholdSpin = nullptr;
    QComboBox* emptyLastPageAppendCombo = nullptr;
    QLineEdit* latexTemplatePathEdit = nullptr;
    QCheckBox* latexAutoCheckDependenciesCheck = nullptr;
    QLineEdit* latexDefaultTextEdit = nullptr;
    QLineEdit* latexGenCmdEdit = nullptr;
    QLineEdit* latexSourceViewThemeIdEdit = nullptr;
    QCheckBox* latexSourceViewAutoIndentCheck = nullptr;
    QCheckBox* latexSourceViewSyntaxHighlightCheck = nullptr;
    QCheckBox* latexSourceViewShowLineNumbersCheck = nullptr;
    QLineEdit* latexEditorFontEdit = nullptr;
    QCheckBox* latexUseCustomEditorFontCheck = nullptr;
    QCheckBox* latexEditorWordWrapCheck = nullptr;
    QCheckBox* latexUseExternalEditorCheck = nullptr;
    QCheckBox* latexExternalEditorAutoConfirmCheck = nullptr;
    QLineEdit* latexExternalEditorCmdEdit = nullptr;
    QLineEdit* latexTemporaryFileExtEdit = nullptr;
    QCheckBox* disableAudioCheck = nullptr;
    QComboBox* audioInputDeviceCombo = nullptr;
    QComboBox* audioOutputDeviceCombo = nullptr;
    QLineEdit* audioFolderEdit = nullptr;
    QDoubleSpinBox* audioSampleRateSpin = nullptr;
    QDoubleSpinBox* audioGainSpin = nullptr;
    QSpinBox* defaultSeekTimeSpin = nullptr;
    QComboBox* toolbarProfileCombo = nullptr;
    QColor selectionColor;
    QColor backgroundColor;
    QColor recolorLightColor;
    QColor recolorDarkColor;
    std::string lastOpenPath;
    std::string lastSavePath;
    std::string lastImagePath;
    std::string lastPdfPath;
    std::string lastExportPath;
};
