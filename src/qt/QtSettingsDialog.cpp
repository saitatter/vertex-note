/*
 * VertexNote
 *
 * Qt settings/preferences dialog implementation.
 */

#include "QtSettingsDialog.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPushButton>
#include <QFileDialog>
#include <QInputDevice>
#include <QPointingDevice>
#include <QSpinBox>
#include <QTabWidget>
#include <QVBoxLayout>

QtSettingsDialog::QtSettingsDialog(const QtSettings& current, const std::vector<QtToolbarProfileOption>& toolbarProfiles,
                                   QWidget* parent):
        QDialog(parent) {
    setWindowTitle(QStringLiteral("Preferences"));
    setMinimumWidth(380);

    auto* mainLayout = new QVBoxLayout(this);
    auto* tabs = new QTabWidget(this);

    // --- Tools tab ---
    auto* toolsPage = new QWidget(this);
    auto* toolsLayout = new QFormLayout(toolsPage);

    this->penWidthSpin = new QDoubleSpinBox(toolsPage);
    this->penWidthSpin->setRange(0.1, 50.0);
    this->penWidthSpin->setSingleStep(0.5);
    this->penWidthSpin->setDecimals(2);
    this->penWidthSpin->setValue(current.defaultPenWidth);
    toolsLayout->addRow(QStringLiteral("Default pen width:"), this->penWidthSpin);

    this->highlighterWidthSpin = new QDoubleSpinBox(toolsPage);
    this->highlighterWidthSpin->setRange(0.1, 50.0);
    this->highlighterWidthSpin->setSingleStep(0.5);
    this->highlighterWidthSpin->setDecimals(2);
    this->highlighterWidthSpin->setValue(current.defaultHighlighterWidth);
    toolsLayout->addRow(QStringLiteral("Default highlighter width:"), this->highlighterWidthSpin);

    this->eraserWidthSpin = new QDoubleSpinBox(toolsPage);
    this->eraserWidthSpin->setRange(0.1, 50.0);
    this->eraserWidthSpin->setSingleStep(0.5);
    this->eraserWidthSpin->setDecimals(2);
    this->eraserWidthSpin->setValue(current.defaultEraserWidth);
    toolsLayout->addRow(QStringLiteral("Default eraser width:"), this->eraserWidthSpin);

    this->pressureCheck = new QCheckBox(toolsPage);
    this->pressureCheck->setChecked(current.defaultPressureSensitive);
    toolsLayout->addRow(QStringLiteral("Pressure sensitive by default:"), this->pressureCheck);

    this->minimumPressureSpin = new QDoubleSpinBox(toolsPage);
    this->minimumPressureSpin->setRange(0.0, 0.95);
    this->minimumPressureSpin->setSingleStep(0.01);
    this->minimumPressureSpin->setDecimals(2);
    this->minimumPressureSpin->setValue(current.minimumPressure);
    toolsLayout->addRow(QStringLiteral("Minimum pressure:"), this->minimumPressureSpin);

    this->pressureMultiplierSpin = new QDoubleSpinBox(toolsPage);
    this->pressureMultiplierSpin->setRange(0.1, 4.0);
    this->pressureMultiplierSpin->setSingleStep(0.1);
    this->pressureMultiplierSpin->setDecimals(2);
    this->pressureMultiplierSpin->setValue(current.pressureMultiplier);
    toolsLayout->addRow(QStringLiteral("Pressure multiplier:"), this->pressureMultiplierSpin);

    this->pressureGuessingCheck = new QCheckBox(toolsPage);
    this->pressureGuessingCheck->setChecked(current.pressureGuessing);
    toolsLayout->addRow(QStringLiteral("Guess pressure if missing:"), this->pressureGuessingCheck);

    this->strokeStabilizerEnabledCheck = new QCheckBox(toolsPage);
    this->strokeStabilizerEnabledCheck->setChecked(current.strokeStabilizerEnabled);
    toolsLayout->addRow(QStringLiteral("Stroke stabilizer:"), this->strokeStabilizerEnabledCheck);

    this->strokeStabilizerSamplesSpin = new QSpinBox(toolsPage);
    this->strokeStabilizerSamplesSpin->setRange(2, 64);
    this->strokeStabilizerSamplesSpin->setValue(current.strokeStabilizerSamples);
    toolsLayout->addRow(QStringLiteral("Stabilizer samples:"), this->strokeStabilizerSamplesSpin);

    this->strokeStabilizerStrengthSpin = new QDoubleSpinBox(toolsPage);
    this->strokeStabilizerStrengthSpin->setRange(0.0, 1.0);
    this->strokeStabilizerStrengthSpin->setSingleStep(0.05);
    this->strokeStabilizerStrengthSpin->setDecimals(2);
    this->strokeStabilizerStrengthSpin->setValue(current.strokeStabilizerStrength);
    toolsLayout->addRow(QStringLiteral("Stabilizer strength:"), this->strokeStabilizerStrengthSpin);

    this->strokeStabilizerFinalizeCheck = new QCheckBox(toolsPage);
    this->strokeStabilizerFinalizeCheck->setChecked(current.strokeStabilizerFinalizeStroke);
    toolsLayout->addRow(QStringLiteral("Stabilizer catches final point:"), this->strokeStabilizerFinalizeCheck);

    this->eraserModeCombo = new QComboBox(toolsPage);
    this->eraserModeCombo->addItem(QStringLiteral("Whole Stroke"), static_cast<int>(QtEraserMode::Standard));
    this->eraserModeCombo->addItem(QStringLiteral("Segment"), static_cast<int>(QtEraserMode::Segment));
    this->eraserModeCombo->setCurrentIndex(current.defaultEraserMode == QtEraserMode::Segment ? 1 : 0);
    toolsLayout->addRow(QStringLiteral("Default eraser mode:"), this->eraserModeCombo);

    toolsPage->setLayout(toolsLayout);
    tabs->addTab(toolsPage, QStringLiteral("Tools"));

    // --- Page tab ---
    auto* pagePage = new QWidget(this);
    auto* pageLayout = new QFormLayout(pagePage);

    this->pageWidthSpin = new QDoubleSpinBox(pagePage);
    this->pageWidthSpin->setRange(50.0, 5000.0);
    this->pageWidthSpin->setSingleStep(10.0);
    this->pageWidthSpin->setDecimals(1);
    this->pageWidthSpin->setValue(current.defaultPageWidth);
    this->pageWidthSpin->setSuffix(QStringLiteral(" pt"));
    pageLayout->addRow(QStringLiteral("Default page width:"), this->pageWidthSpin);

    this->pageHeightSpin = new QDoubleSpinBox(pagePage);
    this->pageHeightSpin->setRange(50.0, 5000.0);
    this->pageHeightSpin->setSingleStep(10.0);
    this->pageHeightSpin->setDecimals(1);
    this->pageHeightSpin->setValue(current.defaultPageHeight);
    this->pageHeightSpin->setSuffix(QStringLiteral(" pt"));
    pageLayout->addRow(QStringLiteral("Default page height:"), this->pageHeightSpin);

    pagePage->setLayout(pageLayout);
    tabs->addTab(pagePage, QStringLiteral("Page"));

    // --- General tab ---
    auto* generalPage = new QWidget(this);
    auto* generalLayout = new QFormLayout(generalPage);

    this->undoLimitSpin = new QSpinBox(generalPage);
    this->undoLimitSpin->setRange(10, 500);
    this->undoLimitSpin->setValue(current.undoHistoryLimit);
    generalLayout->addRow(QStringLiteral("Undo history limit:"), this->undoLimitSpin);

    this->autosaveEnabledCheck = new QCheckBox(generalPage);
    this->autosaveEnabledCheck->setChecked(current.autosaveEnabled);
    generalLayout->addRow(QStringLiteral("Autosave existing documents:"), this->autosaveEnabledCheck);

    this->autosaveTimeoutSpin = new QSpinBox(generalPage);
    this->autosaveTimeoutSpin->setRange(1, 120);
    this->autosaveTimeoutSpin->setValue(current.autosaveTimeoutMinutes);
    this->autosaveTimeoutSpin->setSuffix(QStringLiteral(" min"));
    generalLayout->addRow(QStringLiteral("Autosave interval:"), this->autosaveTimeoutSpin);

    this->geoSnapCheck = new QCheckBox(generalPage);
    this->geoSnapCheck->setChecked(current.geometrySnapDefault);
    generalLayout->addRow(QStringLiteral("Geometry snap enabled:"), this->geoSnapCheck);

    this->gridSnapCheck = new QCheckBox(generalPage);
    this->gridSnapCheck->setChecked(current.gridSnapDefault);
    generalLayout->addRow(QStringLiteral("Grid snap enabled:"), this->gridSnapCheck);

    this->rotationSnapCheck = new QCheckBox(generalPage);
    this->rotationSnapCheck->setChecked(current.rotationSnapDefault);
    generalLayout->addRow(QStringLiteral("Rotation snap enabled:"), this->rotationSnapCheck);

    this->touchDrawingCheck = new QCheckBox(generalPage);
    this->touchDrawingCheck->setChecked(current.touchDrawingDefault);
    generalLayout->addRow(QStringLiteral("Touch drawing enabled:"), this->touchDrawingCheck);

    this->snapGridSizeSpin = new QDoubleSpinBox(generalPage);
    this->snapGridSizeSpin->setRange(1.0, 500.0);
    this->snapGridSizeSpin->setSingleStep(1.0);
    this->snapGridSizeSpin->setDecimals(2);
    this->snapGridSizeSpin->setValue(current.snapGridSize);
    this->snapGridSizeSpin->setSuffix(QStringLiteral(" pt"));
    generalLayout->addRow(QStringLiteral("Grid snap size:"), this->snapGridSizeSpin);

    this->snapGridToleranceSpin = new QDoubleSpinBox(generalPage);
    this->snapGridToleranceSpin->setRange(0.01, 10.0);
    this->snapGridToleranceSpin->setSingleStep(0.05);
    this->snapGridToleranceSpin->setDecimals(2);
    this->snapGridToleranceSpin->setValue(current.snapGridTolerance);
    generalLayout->addRow(QStringLiteral("Grid snap tolerance:"), this->snapGridToleranceSpin);

    this->strokeRecognizerMinSizeSpin = new QDoubleSpinBox(generalPage);
    this->strokeRecognizerMinSizeSpin->setRange(5.0, 500.0);
    this->strokeRecognizerMinSizeSpin->setSingleStep(5.0);
    this->strokeRecognizerMinSizeSpin->setDecimals(1);
    this->strokeRecognizerMinSizeSpin->setValue(current.strokeRecognizerMinSize);
    this->strokeRecognizerMinSizeSpin->setSuffix(QStringLiteral(" pt"));
    generalLayout->addRow(QStringLiteral("Shape recognizer min size:"), this->strokeRecognizerMinSizeSpin);

    this->laserPointerFadeOutSpin = new QSpinBox(generalPage);
    this->laserPointerFadeOutSpin->setRange(100, 10000);
    this->laserPointerFadeOutSpin->setSingleStep(100);
    this->laserPointerFadeOutSpin->setValue(current.laserPointerFadeOutMs);
    this->laserPointerFadeOutSpin->setSuffix(QStringLiteral(" ms"));
    generalLayout->addRow(QStringLiteral("Laser pointer fade-out:"), this->laserPointerFadeOutSpin);

    generalPage->setLayout(generalLayout);
    tabs->addTab(generalPage, QStringLiteral("General"));

    // --- Appearance tab ---
    auto* appearancePage = new QWidget(this);
    auto* appearanceLayout = new QFormLayout(appearancePage);
    this->showFilePathInTitlebarCheck = new QCheckBox(appearancePage);
    this->showFilePathInTitlebarCheck->setChecked(current.showFilePathInTitlebar);
    appearanceLayout->addRow(QStringLiteral("Show file path in titlebar:"), this->showFilePathInTitlebarCheck);
    this->showPageNumberInTitlebarCheck = new QCheckBox(appearancePage);
    this->showPageNumberInTitlebarCheck->setChecked(current.showPageNumberInTitlebar);
    appearanceLayout->addRow(QStringLiteral("Show page number in titlebar:"), this->showPageNumberInTitlebarCheck);
    this->showPageShadowCheck = new QCheckBox(appearancePage);
    this->showPageShadowCheck->setChecked(current.showPageShadow);
    appearanceLayout->addRow(QStringLiteral("Show page shadow:"), this->showPageShadowCheck);

    this->themeVariantCombo = new QComboBox(appearancePage);
    this->themeVariantCombo->addItem(QStringLiteral("System"), QStringLiteral("system"));
    this->themeVariantCombo->addItem(QStringLiteral("Light"), QStringLiteral("light"));
    this->themeVariantCombo->addItem(QStringLiteral("Dark"), QStringLiteral("dark"));
    const QString currentTheme = QString::fromStdString(current.themeVariant);
    const int themeIndex = this->themeVariantCombo->findData(currentTheme);
    this->themeVariantCombo->setCurrentIndex(themeIndex >= 0 ? themeIndex : 0);
    appearanceLayout->addRow(QStringLiteral("Theme:"), this->themeVariantCombo);

    auto* paletteRow = new QWidget(appearancePage);
    auto* paletteRowLayout = new QHBoxLayout(paletteRow);
    paletteRowLayout->setContentsMargins(0, 0, 0, 0);
    this->colorPalettePathEdit = new QLineEdit(QString::fromStdString(current.colorPalettePath), paletteRow);
    auto* browsePaletteButton = new QPushButton(QStringLiteral("Browse..."), paletteRow);
    paletteRowLayout->addWidget(this->colorPalettePathEdit, 1);
    paletteRowLayout->addWidget(browsePaletteButton, 0);
    QObject::connect(browsePaletteButton, &QPushButton::clicked, this, [this]() {
        const QString selectedPath =
                QFileDialog::getOpenFileName(this, QStringLiteral("Select Color Palette"),
                                             this->colorPalettePathEdit->text().trimmed(),
                                             QStringLiteral("GIMP Palette (*.gpl);;All Files (*)"));
        if (!selectedPath.isEmpty()) {
            this->colorPalettePathEdit->setText(selectedPath);
        }
    });
    appearanceLayout->addRow(QStringLiteral("Color palette:"), paletteRow);
    appearancePage->setLayout(appearanceLayout);
    tabs->addTab(appearancePage, QStringLiteral("Appearance"));

    // --- Toolbar tab ---
    auto* toolbarPage = new QWidget(this);
    auto* toolbarLayout = new QFormLayout(toolbarPage);

    this->toolbarProfileCombo = new QComboBox(toolbarPage);
    int currentProfileIndex = -1;
    for (int profileIndex = 0; profileIndex < static_cast<int>(toolbarProfiles.size()); ++profileIndex) {
        const auto& profile = toolbarProfiles[static_cast<std::size_t>(profileIndex)];
        const auto label = profile.displayName.empty() ? profile.id : profile.displayName;
        this->toolbarProfileCombo->addItem(QString::fromStdString(label), QString::fromStdString(profile.id));
        if (profile.id == current.toolbarProfileId) {
            currentProfileIndex = profileIndex;
        }
    }
    if (currentProfileIndex >= 0) {
        this->toolbarProfileCombo->setCurrentIndex(currentProfileIndex);
    }
    toolbarLayout->addRow(QStringLiteral("Toolbar profile:"), this->toolbarProfileCombo);

    toolbarPage->setLayout(toolbarLayout);
    tabs->addTab(toolbarPage, QStringLiteral("Toolbar"));

    // --- PDF tab ---
    auto* pdfPage = new QWidget(this);
    auto* pdfLayout = new QFormLayout(pdfPage);
    this->autoloadPdfXojCheck = new QCheckBox(pdfPage);
    this->autoloadPdfXojCheck->setChecked(current.autoloadPdfXoj);
    pdfLayout->addRow(QStringLiteral("Autoload PDF .xoj:"), this->autoloadPdfXojCheck);
    this->defaultPdfExportNameEdit = new QLineEdit(QString::fromStdString(current.defaultPdfExportName), pdfPage);
    pdfLayout->addRow(QStringLiteral("Default PDF export name:"), this->defaultPdfExportNameEdit);
    this->pdfPageCacheSizeSpin = new QSpinBox(pdfPage);
    this->pdfPageCacheSizeSpin->setRange(1, 500);
    this->pdfPageCacheSizeSpin->setValue(current.pdfPageCacheSize);
    pdfLayout->addRow(QStringLiteral("PDF page cache size:"), this->pdfPageCacheSizeSpin);
    this->pdfPreloadBeforeSpin = new QSpinBox(pdfPage);
    this->pdfPreloadBeforeSpin->setRange(0, 50);
    this->pdfPreloadBeforeSpin->setValue(current.pdfPreloadPagesBefore);
    pdfLayout->addRow(QStringLiteral("Preload pages before:"), this->pdfPreloadBeforeSpin);
    this->pdfPreloadAfterSpin = new QSpinBox(pdfPage);
    this->pdfPreloadAfterSpin->setRange(0, 50);
    this->pdfPreloadAfterSpin->setValue(current.pdfPreloadPagesAfter);
    pdfLayout->addRow(QStringLiteral("Preload pages after:"), this->pdfPreloadAfterSpin);
    this->pdfEagerCleanupCheck = new QCheckBox(pdfPage);
    this->pdfEagerCleanupCheck->setChecked(current.pdfEagerPageCleanup);
    pdfLayout->addRow(QStringLiteral("Eager PDF cleanup:"), this->pdfEagerCleanupCheck);
    pdfPage->setLayout(pdfLayout);
    tabs->addTab(pdfPage, QStringLiteral("PDF"));

    // --- LaTeX tab ---
    auto* latexPage = new QWidget(this);
    auto* latexLayout = new QFormLayout(latexPage);
    auto* latexTemplateRow = new QWidget(latexPage);
    auto* latexTemplateRowLayout = new QHBoxLayout(latexTemplateRow);
    latexTemplateRowLayout->setContentsMargins(0, 0, 0, 0);
    this->latexTemplatePathEdit = new QLineEdit(QString::fromStdString(current.latexTemplatePath), latexTemplateRow);
    auto* browseLatexTemplateButton = new QPushButton(QStringLiteral("Browse..."), latexTemplateRow);
    latexTemplateRowLayout->addWidget(this->latexTemplatePathEdit, 1);
    latexTemplateRowLayout->addWidget(browseLatexTemplateButton, 0);
    QObject::connect(browseLatexTemplateButton, &QPushButton::clicked, this, [this]() {
        const QString selectedPath =
                QFileDialog::getOpenFileName(this, QStringLiteral("Select LaTeX Template"),
                                             this->latexTemplatePathEdit->text().trimmed(),
                                             QStringLiteral("TeX Files (*.tex);;All Files (*)"));
        if (!selectedPath.isEmpty()) {
            this->latexTemplatePathEdit->setText(selectedPath);
        }
    });
    latexLayout->addRow(QStringLiteral("Template path:"), latexTemplateRow);
    latexPage->setLayout(latexLayout);
    tabs->addTab(latexPage, QStringLiteral("LaTeX"));

    // --- Audio tab ---
    auto* audioPage = new QWidget(this);
    auto* audioLayout = new QFormLayout(audioPage);

    auto* audioFolderRow = new QWidget(audioPage);
    auto* audioFolderRowLayout = new QHBoxLayout(audioFolderRow);
    audioFolderRowLayout->setContentsMargins(0, 0, 0, 0);
    this->audioFolderEdit = new QLineEdit(QString::fromStdString(current.audioFolder), audioFolderRow);
    auto* browseAudioFolderButton = new QPushButton(QStringLiteral("Browse..."), audioFolderRow);
    audioFolderRowLayout->addWidget(this->audioFolderEdit, 1);
    audioFolderRowLayout->addWidget(browseAudioFolderButton, 0);
    QObject::connect(browseAudioFolderButton, &QPushButton::clicked, this, [this]() {
        const QString currentPath = this->audioFolderEdit->text().trimmed();
        const QString selectedPath =
                QFileDialog::getExistingDirectory(this, QStringLiteral("Select Audio Folder"), currentPath);
        if (!selectedPath.isEmpty()) {
            this->audioFolderEdit->setText(selectedPath);
        }
    });
    audioLayout->addRow(QStringLiteral("Audio folder:"), audioFolderRow);

    this->audioSampleRateSpin = new QDoubleSpinBox(audioPage);
    this->audioSampleRateSpin->setRange(8000.0, 192000.0);
    this->audioSampleRateSpin->setDecimals(0);
    this->audioSampleRateSpin->setSingleStep(1000.0);
    this->audioSampleRateSpin->setValue(current.audioSampleRate);
    this->audioSampleRateSpin->setSuffix(QStringLiteral(" Hz"));
    audioLayout->addRow(QStringLiteral("Sample rate:"), this->audioSampleRateSpin);

    this->audioGainSpin = new QDoubleSpinBox(audioPage);
    this->audioGainSpin->setRange(0.1, 8.0);
    this->audioGainSpin->setDecimals(2);
    this->audioGainSpin->setSingleStep(0.1);
    this->audioGainSpin->setValue(current.audioGain);
    audioLayout->addRow(QStringLiteral("Playback gain:"), this->audioGainSpin);

    this->defaultSeekTimeSpin = new QSpinBox(audioPage);
    this->defaultSeekTimeSpin->setRange(1, 120);
    this->defaultSeekTimeSpin->setSingleStep(1);
    this->defaultSeekTimeSpin->setValue(current.defaultSeekTimeSeconds);
    this->defaultSeekTimeSpin->setSuffix(QStringLiteral(" s"));
    audioLayout->addRow(QStringLiteral("Seek step:"), this->defaultSeekTimeSpin);

    audioPage->setLayout(audioLayout);
    tabs->addTab(audioPage, QStringLiteral("Audio"));

    // --- Devices tab ---
    auto* devicesPage = new QWidget(this);
    auto* devicesLayout = new QVBoxLayout(devicesPage);
    auto* devicesForm = new QFormLayout();
    this->eraserCursorHiddenCheck = new QCheckBox(devicesPage);
    this->eraserCursorHiddenCheck->setChecked(current.eraserCursorHidden);
    devicesForm->addRow(QStringLiteral("Hide eraser cursor:"), this->eraserCursorHiddenCheck);
    this->rightButtonActionCombo = new QComboBox(devicesPage);
    this->rightButtonActionCombo->addItem(QStringLiteral("None"), static_cast<int>(QtPointerButtonAction::None));
    this->rightButtonActionCombo->addItem(QStringLiteral("Eraser"), static_cast<int>(QtPointerButtonAction::Eraser));
    this->rightButtonActionCombo->setCurrentIndex(current.rightButtonAction == QtPointerButtonAction::None ? 0 : 1);
    devicesForm->addRow(QStringLiteral("Right button:"), this->rightButtonActionCombo);
    this->middleButtonActionCombo = new QComboBox(devicesPage);
    this->middleButtonActionCombo->addItem(QStringLiteral("None"), static_cast<int>(QtPointerButtonAction::None));
    this->middleButtonActionCombo->addItem(QStringLiteral("Pan"), static_cast<int>(QtPointerButtonAction::Pan));
    this->middleButtonActionCombo->setCurrentIndex(current.middleButtonAction == QtPointerButtonAction::None ? 0 : 1);
    devicesForm->addRow(QStringLiteral("Middle button:"), this->middleButtonActionCombo);
    devicesLayout->addLayout(devicesForm);

    this->inputDeviceList = new QListWidget(devicesPage);
    const auto devices = QInputDevice::devices();
    for (const auto* device: devices) {
        if (!device) {
            continue;
        }
        QString type = QStringLiteral("device");
        if (auto* pointing = dynamic_cast<const QPointingDevice*>(device)) {
            switch (pointing->pointerType()) {
                case QPointingDevice::PointerType::Generic: type = QStringLiteral("pointer"); break;
                case QPointingDevice::PointerType::Finger: type = QStringLiteral("touch"); break;
                case QPointingDevice::PointerType::Pen: type = QStringLiteral("pen"); break;
                case QPointingDevice::PointerType::Eraser: type = QStringLiteral("eraser"); break;
                case QPointingDevice::PointerType::Cursor: type = QStringLiteral("cursor"); break;
                default: break;
            }
        }
        this->inputDeviceList->addItem(QStringLiteral("%1 (%2)").arg(device->name(), type));
    }
    if (this->inputDeviceList->count() == 0) {
        this->inputDeviceList->addItem(QStringLiteral("No Qt input devices reported yet"));
    }
    devicesLayout->addWidget(this->inputDeviceList);
    devicesLayout->addStretch(1);
    devicesPage->setLayout(devicesLayout);
    tabs->addTab(devicesPage, QStringLiteral("Devices"));

    mainLayout->addWidget(tabs);

    // Buttons
    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    mainLayout->addWidget(buttons);

    setLayout(mainLayout);
}

auto QtSettingsDialog::settings() const -> QtSettings {
    return {
            .defaultPenWidth = this->penWidthSpin->value(),
            .defaultHighlighterWidth = this->highlighterWidthSpin->value(),
            .defaultEraserWidth = this->eraserWidthSpin->value(),
            .defaultPressureSensitive = this->pressureCheck->isChecked(),
            .defaultEraserMode = this->eraserModeCombo->currentIndex() == 1 ? QtEraserMode::Segment
                                                                            : QtEraserMode::Standard,
            .defaultPageWidth = this->pageWidthSpin->value(),
            .defaultPageHeight = this->pageHeightSpin->value(),
            .undoHistoryLimit = this->undoLimitSpin->value(),
            .autosaveEnabled = this->autosaveEnabledCheck->isChecked(),
            .autosaveTimeoutMinutes = this->autosaveTimeoutSpin->value(),
            .geometrySnapDefault = this->geoSnapCheck->isChecked(),
            .gridSnapDefault = this->gridSnapCheck->isChecked(),
            .rotationSnapDefault = this->rotationSnapCheck->isChecked(),
            .touchDrawingDefault = this->touchDrawingCheck->isChecked(),
            .minimumPressure = this->minimumPressureSpin->value(),
            .pressureMultiplier = this->pressureMultiplierSpin->value(),
            .pressureGuessing = this->pressureGuessingCheck->isChecked(),
            .strokeStabilizerEnabled = this->strokeStabilizerEnabledCheck->isChecked(),
            .strokeStabilizerSamples = this->strokeStabilizerSamplesSpin->value(),
            .strokeStabilizerStrength = this->strokeStabilizerStrengthSpin->value(),
            .strokeStabilizerFinalizeStroke = this->strokeStabilizerFinalizeCheck->isChecked(),
            .snapGridTolerance = this->snapGridToleranceSpin->value(),
            .snapGridSize = this->snapGridSizeSpin->value(),
            .strokeRecognizerMinSize = this->strokeRecognizerMinSizeSpin->value(),
            .laserPointerFadeOutMs = this->laserPointerFadeOutSpin->value(),
            .eraserCursorHidden = this->eraserCursorHiddenCheck->isChecked(),
            .rightButtonAction = static_cast<QtPointerButtonAction>(this->rightButtonActionCombo->currentData().toInt()),
            .middleButtonAction = static_cast<QtPointerButtonAction>(this->middleButtonActionCombo->currentData().toInt()),
            .showFilePathInTitlebar = this->showFilePathInTitlebarCheck->isChecked(),
            .showPageNumberInTitlebar = this->showPageNumberInTitlebarCheck->isChecked(),
            .showPageShadow = this->showPageShadowCheck->isChecked(),
            .themeVariant = this->themeVariantCombo->currentData().toString().toStdString(),
            .colorPalettePath = this->colorPalettePathEdit->text().trimmed().toStdString(),
            .autoloadPdfXoj = this->autoloadPdfXojCheck->isChecked(),
            .defaultPdfExportName = this->defaultPdfExportNameEdit->text().trimmed().toStdString(),
            .pdfPageCacheSize = this->pdfPageCacheSizeSpin->value(),
            .pdfPreloadPagesBefore = this->pdfPreloadBeforeSpin->value(),
            .pdfPreloadPagesAfter = this->pdfPreloadAfterSpin->value(),
            .pdfEagerPageCleanup = this->pdfEagerCleanupCheck->isChecked(),
            .latexTemplatePath = this->latexTemplatePathEdit->text().trimmed().toStdString(),
            .audioFolder = this->audioFolderEdit->text().trimmed().toStdString(),
            .audioSampleRate = this->audioSampleRateSpin->value(),
            .audioGain = this->audioGainSpin->value(),
            .defaultSeekTimeSeconds = this->defaultSeekTimeSpin->value(),
            .toolbarProfileId = this->toolbarProfileCombo->currentData().toString().toStdString(),
    };
}
