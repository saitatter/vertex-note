/*
 * VertexNote
 *
 * Qt settings/preferences dialog tab construction.
 */

#include "QtSettingsDialog.h"

#include <algorithm>
#include <utility>

#include <QAbstractItemView>
#include <QCheckBox>
#include <QColorDialog>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QFileDialog>
#include <QFontComboBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QInputDevice>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPointingDevice>
#include <QPushButton>
#include <QSignalBlocker>
#include <QSpinBox>
#include <QTabWidget>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QVBoxLayout>
#include <QWidget>

#include "QtInputDeviceKey.h"
#include "filesystem.h"
#include "model/FormatDefinitions.h"
#include "util/PathUtil.h"

namespace {

auto colorToQColor(Color color) -> QColor { return QColor(color.red, color.green, color.blue, color.alpha); }

void updateColorButton(QPushButton* button, const QColor& color) {
    button->setText(color.name(QColor::HexArgb).toUpper());
    button->setStyleSheet(QStringLiteral("QPushButton { background-color: %1; color: %2; }")
                                  .arg(color.name(QColor::HexArgb),
                                       color.lightness() < 128 ? QStringLiteral("#ffffff") : QStringLiteral("#202020")));
}

auto makeColorButton(QWidget* parent, const QColor& initialColor) -> QPushButton* {
    auto* button = new QPushButton(parent);
    button->setMinimumWidth(96);
    updateColorButton(button, initialColor);
    return button;
}

auto unitScale(std::string_view unitName) -> double {
    for (int index = 0; index < NOTE_UNIT_COUNT; ++index) {
        if (unitName == NOTE_UNITS[index].name) {
            return NOTE_UNITS[index].scale;
        }
    }
    return NOTE_UNITS[0].scale;
}

auto currentSizeUnitName(const QComboBox* combo) -> std::string {
    return combo ? combo->currentData().toString().toStdString() : std::string(NOTE_UNITS[0].name);
}

auto currentSizeUnitScale(const QComboBox* combo) -> double { return unitScale(currentSizeUnitName(combo)); }

void populatePointerActionCombo(QComboBox* combo, QtPointerButtonAction current) {
    combo->addItem(QStringLiteral("Normal"), static_cast<int>(QtPointerButtonAction::None));
    combo->addItem(QStringLiteral("Pan"), static_cast<int>(QtPointerButtonAction::Pan));
    combo->addItem(QStringLiteral("Eraser"), static_cast<int>(QtPointerButtonAction::Eraser));
    const int index = combo->findData(static_cast<int>(current));
    combo->setCurrentIndex(index >= 0 ? index : 0);
}

auto pointerActionCombo(QWidget* parent, QtPointerButtonAction current) -> QComboBox* {
    auto* combo = new QComboBox(parent);
    populatePointerActionCombo(combo, current);
    return combo;
}

void addDeviceMatrixRow(QTableWidget* table, const QtInputDeviceButtonProfile& profile,
                        const QtPointerButtonMatrix& fallbackMatrix) {
    const int row = table->rowCount();
    table->insertRow(row);

    auto* deviceItem = new QTableWidgetItem(QString::fromStdString(profile.displayName));
    deviceItem->setData(Qt::UserRole, QString::fromStdString(profile.key));
    deviceItem->setFlags(deviceItem->flags() & ~Qt::ItemIsEditable);
    table->setItem(row, 0, deviceItem);

    auto* typeItem = new QTableWidgetItem(QString::fromStdString(profile.deviceType));
    typeItem->setFlags(typeItem->flags() & ~Qt::ItemIsEditable);
    table->setItem(row, 1, typeItem);

    auto* customItem = new QTableWidgetItem();
    customItem->setFlags((customItem->flags() | Qt::ItemIsUserCheckable) & ~Qt::ItemIsEditable);
    customItem->setCheckState(profile.customButtonMatrix ? Qt::Checked : Qt::Unchecked);
    table->setItem(row, 2, customItem);

    const auto& matrix = profile.customButtonMatrix ? profile.buttonMatrix : fallbackMatrix;
    table->setCellWidget(row, 3, pointerActionCombo(table, matrix.eraserTipAction));
    table->setCellWidget(row, 4, pointerActionCombo(table, matrix.stylusButton1Action));
    table->setCellWidget(row, 5, pointerActionCombo(table, matrix.stylusButton2Action));
    table->setCellWidget(row, 6, pointerActionCombo(table, matrix.mouseLeftAction));
    table->setCellWidget(row, 7, pointerActionCombo(table, matrix.mouseMiddleAction));
    table->setCellWidget(row, 8, pointerActionCombo(table, matrix.mouseRightAction));
    table->setCellWidget(row, 9, pointerActionCombo(table, matrix.mouseBackAction));
    table->setCellWidget(row, 10, pointerActionCombo(table, matrix.mouseForwardAction));
    table->setCellWidget(row, 11, pointerActionCombo(table, matrix.touchAction));
}

void populateAudioDeviceCombo(QComboBox* combo, const std::vector<QtAudioDeviceOption>& devices, int selectedIndex) {
    combo->addItem(QStringLiteral("System default"), -1);
    int currentIndex = selectedIndex < 0 ? 0 : -1;
    for (const auto& device: devices) {
        combo->addItem(QString::fromStdString(device.displayName), device.index);
        if (device.index == selectedIndex || (selectedIndex < 0 && device.selected)) {
            currentIndex = combo->count() - 1;
        }
    }
    combo->setCurrentIndex(currentIndex >= 0 ? currentIndex : 0);
}

auto availableLocaleCodes() -> std::vector<std::string> {
    std::vector<std::string> locales;
    try {
        const auto baseLocaleDir = Util::getGettextFilepath(Util::getLocalePath());
        if (fs::exists(baseLocaleDir)) {
            for (const auto& entry: fs::directory_iterator(baseLocaleDir)) {
                if (entry.is_directory()) {
                    locales.push_back(entry.path().filename().string());
                }
            }
        }
    } catch (...) {
        locales.clear();
    }
    std::ranges::sort(locales);
    if (!std::ranges::binary_search(locales, std::string("en"))) {
        locales.push_back("en");
        std::ranges::sort(locales);
    }
    return locales;
}


}  // namespace

void QtSettingsDialog::addToolsTab(QTabWidget* tabs, const QtSettings& current) {
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

    this->defaultFontCombo = new QFontComboBox(toolsPage);
    this->defaultFontCombo->setCurrentFont(QFont(QString::fromStdString(current.defaultFontName)));
    toolsLayout->addRow(QStringLiteral("Default text font:"), this->defaultFontCombo);

    this->defaultFontSizeSpin = new QDoubleSpinBox(toolsPage);
    this->defaultFontSizeSpin->setRange(4.0, 200.0);
    this->defaultFontSizeSpin->setDecimals(1);
    this->defaultFontSizeSpin->setSingleStep(1.0);
    this->defaultFontSizeSpin->setValue(current.defaultFontSize);
    this->defaultFontSizeSpin->setSuffix(QStringLiteral(" pt"));
    toolsLayout->addRow(QStringLiteral("Default text size:"), this->defaultFontSizeSpin);

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

    this->strokeStabilizerAveragingCombo = new QComboBox(toolsPage);
    this->strokeStabilizerAveragingCombo->addItem(QStringLiteral("None"), 0);
    this->strokeStabilizerAveragingCombo->addItem(QStringLiteral("Arithmetic"), 1);
    this->strokeStabilizerAveragingCombo->addItem(QStringLiteral("Velocity Gaussian"), 2);
    const int averagingIndex = this->strokeStabilizerAveragingCombo->findData(current.strokeStabilizerAveragingMethod);
    this->strokeStabilizerAveragingCombo->setCurrentIndex(averagingIndex >= 0 ? averagingIndex : 1);
    toolsLayout->addRow(QStringLiteral("Stabilizer averaging:"), this->strokeStabilizerAveragingCombo);

    this->strokeStabilizerPreprocessorCombo = new QComboBox(toolsPage);
    this->strokeStabilizerPreprocessorCombo->addItem(QStringLiteral("None"), 0);
    this->strokeStabilizerPreprocessorCombo->addItem(QStringLiteral("Deadzone"), 1);
    this->strokeStabilizerPreprocessorCombo->addItem(QStringLiteral("Inertia"), 2);
    const int preprocessorIndex =
            this->strokeStabilizerPreprocessorCombo->findData(current.strokeStabilizerPreprocessor);
    this->strokeStabilizerPreprocessorCombo->setCurrentIndex(preprocessorIndex >= 0 ? preprocessorIndex : 0);
    toolsLayout->addRow(QStringLiteral("Stabilizer preprocessor:"), this->strokeStabilizerPreprocessorCombo);

    this->strokeStabilizerSigmaSpin = new QDoubleSpinBox(toolsPage);
    this->strokeStabilizerSigmaSpin->setRange(0.05, 20.0);
    this->strokeStabilizerSigmaSpin->setSingleStep(0.05);
    this->strokeStabilizerSigmaSpin->setDecimals(2);
    this->strokeStabilizerSigmaSpin->setValue(current.strokeStabilizerSigma);
    toolsLayout->addRow(QStringLiteral("Gaussian sigma:"), this->strokeStabilizerSigmaSpin);

    this->strokeStabilizerDeadzoneRadiusSpin = new QDoubleSpinBox(toolsPage);
    this->strokeStabilizerDeadzoneRadiusSpin->setRange(0.0, 100.0);
    this->strokeStabilizerDeadzoneRadiusSpin->setSingleStep(0.1);
    this->strokeStabilizerDeadzoneRadiusSpin->setDecimals(2);
    this->strokeStabilizerDeadzoneRadiusSpin->setValue(current.strokeStabilizerDeadzoneRadius);
    this->strokeStabilizerDeadzoneRadiusSpin->setSuffix(QStringLiteral(" pt"));
    toolsLayout->addRow(QStringLiteral("Deadzone radius:"), this->strokeStabilizerDeadzoneRadiusSpin);

    this->strokeStabilizerCuspDetectionCheck = new QCheckBox(toolsPage);
    this->strokeStabilizerCuspDetectionCheck->setChecked(current.strokeStabilizerCuspDetection);
    toolsLayout->addRow(QStringLiteral("Deadzone cusp detection:"), this->strokeStabilizerCuspDetectionCheck);

    this->strokeStabilizerDragSpin = new QDoubleSpinBox(toolsPage);
    this->strokeStabilizerDragSpin->setRange(0.0, 0.99);
    this->strokeStabilizerDragSpin->setSingleStep(0.05);
    this->strokeStabilizerDragSpin->setDecimals(2);
    this->strokeStabilizerDragSpin->setValue(current.strokeStabilizerDrag);
    toolsLayout->addRow(QStringLiteral("Inertia drag:"), this->strokeStabilizerDragSpin);

    this->strokeStabilizerMassSpin = new QDoubleSpinBox(toolsPage);
    this->strokeStabilizerMassSpin->setRange(0.1, 100.0);
    this->strokeStabilizerMassSpin->setSingleStep(0.5);
    this->strokeStabilizerMassSpin->setDecimals(2);
    this->strokeStabilizerMassSpin->setValue(current.strokeStabilizerMass);
    toolsLayout->addRow(QStringLiteral("Inertia mass:"), this->strokeStabilizerMassSpin);

    this->restoreLineWidthCheck = new QCheckBox(toolsPage);
    this->restoreLineWidthCheck->setChecked(current.restoreLineWidthEnabled);
    toolsLayout->addRow(QStringLiteral("Keep stroke width while scaling:"), this->restoreLineWidthCheck);

    auto* vertexToolsGroup = new QGroupBox(QStringLiteral("Vertex / Edge Tools"), toolsPage);
    auto* vertexToolsLayout = new QFormLayout(vertexToolsGroup);
    this->vertexSnapMarkerSizeSpin = new QSpinBox(vertexToolsGroup);
    this->vertexSnapMarkerSizeSpin->setRange(8, 48);
    this->vertexSnapMarkerSizeSpin->setSingleStep(1);
    this->vertexSnapMarkerSizeSpin->setValue(current.vertexSnapMarkerSize);
    this->vertexSnapMarkerSizeSpin->setSuffix(QStringLiteral(" px"));
    vertexToolsLayout->addRow(QStringLiteral("Vertex snap marker size:"), this->vertexSnapMarkerSizeSpin);
    vertexToolsGroup->setLayout(vertexToolsLayout);
    toolsLayout->addRow(vertexToolsGroup);

    this->eraserModeCombo = new QComboBox(toolsPage);
    this->eraserModeCombo->addItem(QStringLiteral("Whole Stroke"), static_cast<int>(QtEraserMode::Standard));
    this->eraserModeCombo->addItem(QStringLiteral("Segment"), static_cast<int>(QtEraserMode::Segment));
    this->eraserModeCombo->setCurrentIndex(current.defaultEraserMode == QtEraserMode::Segment ? 1 : 0);
    toolsLayout->addRow(QStringLiteral("Default eraser mode:"), this->eraserModeCombo);

    toolsPage->setLayout(toolsLayout);
    tabs->addTab(toolsPage, QStringLiteral("Tools"));
}

void QtSettingsDialog::addPageTab(QTabWidget* tabs, const QtSettings& current) {
    // --- Page tab ---
    auto* pagePage = new QWidget(this);
    auto* pageLayout = new QFormLayout(pagePage);

    this->sizeUnitCombo = new QComboBox(pagePage);
    for (int index = 0; index < NOTE_UNIT_COUNT; ++index) {
        this->sizeUnitCombo->addItem(QString::fromUtf8(NOTE_UNITS[index].name),
                                     QString::fromUtf8(NOTE_UNITS[index].name));
    }
    const int unitIndex = this->sizeUnitCombo->findData(QString::fromStdString(current.sizeUnit));
    this->sizeUnitCombo->setCurrentIndex(unitIndex >= 0 ? unitIndex : 0);
    pageLayout->addRow(QStringLiteral("Size unit:"), this->sizeUnitCombo);

    this->pageWidthSpin = new QDoubleSpinBox(pagePage);
    pageLayout->addRow(QStringLiteral("Default page width:"), this->pageWidthSpin);

    this->pageHeightSpin = new QDoubleSpinBox(pagePage);
    pageLayout->addRow(QStringLiteral("Default page height:"), this->pageHeightSpin);

    double pageSizeScale = currentSizeUnitScale(this->sizeUnitCombo);
    const auto configurePageSizeSpin = [](QDoubleSpinBox* spin, double scale, std::string_view unitName) {
        spin->setRange(50.0 / scale, 5000.0 / scale);
        spin->setSingleStep(unitName == "points" ? 10.0 : 0.5);
        spin->setDecimals(unitName == "points" ? 1 : 2);
        spin->setSuffix(QStringLiteral(" ") + QString::fromUtf8(unitName.data(), static_cast<int>(unitName.size())));
    };
    const auto configurePageSizeSpins = [this, configurePageSizeSpin](double scale) {
        const auto unitName = currentSizeUnitName(this->sizeUnitCombo);
        configurePageSizeSpin(this->pageWidthSpin, scale, unitName);
        configurePageSizeSpin(this->pageHeightSpin, scale, unitName);
    };
    configurePageSizeSpins(pageSizeScale);
    this->pageWidthSpin->setValue(current.defaultPageWidth / pageSizeScale);
    this->pageHeightSpin->setValue(current.defaultPageHeight / pageSizeScale);
    QObject::connect(this->sizeUnitCombo, static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged), this,
                     [this, configurePageSizeSpins, pageSizeScale]() mutable {
                         const double widthPoints = this->pageWidthSpin->value() * pageSizeScale;
                         const double heightPoints = this->pageHeightSpin->value() * pageSizeScale;
                         pageSizeScale = currentSizeUnitScale(this->sizeUnitCombo);
                         QSignalBlocker widthBlocker(this->pageWidthSpin);
                         QSignalBlocker heightBlocker(this->pageHeightSpin);
                         configurePageSizeSpins(pageSizeScale);
                         this->pageWidthSpin->setValue(widthPoints / pageSizeScale);
                         this->pageHeightSpin->setValue(heightPoints / pageSizeScale);
                     });

    this->addHorizontalSpaceCheck = new QCheckBox(pagePage);
    this->addHorizontalSpaceCheck->setChecked(current.addHorizontalSpace);
    pageLayout->addRow(QStringLiteral("Extra horizontal page space:"), this->addHorizontalSpaceCheck);

    this->addHorizontalSpaceLeftSpin = new QSpinBox(pagePage);
    this->addHorizontalSpaceLeftSpin->setRange(0, 5000);
    this->addHorizontalSpaceLeftSpin->setValue(current.addHorizontalSpaceAmountLeft);
    this->addHorizontalSpaceLeftSpin->setSuffix(QStringLiteral(" pt"));
    pageLayout->addRow(QStringLiteral("Horizontal space left:"), this->addHorizontalSpaceLeftSpin);

    this->addHorizontalSpaceRightSpin = new QSpinBox(pagePage);
    this->addHorizontalSpaceRightSpin->setRange(0, 5000);
    this->addHorizontalSpaceRightSpin->setValue(current.addHorizontalSpaceAmountRight);
    this->addHorizontalSpaceRightSpin->setSuffix(QStringLiteral(" pt"));
    pageLayout->addRow(QStringLiteral("Horizontal space right:"), this->addHorizontalSpaceRightSpin);

    this->addVerticalSpaceCheck = new QCheckBox(pagePage);
    this->addVerticalSpaceCheck->setChecked(current.addVerticalSpace);
    pageLayout->addRow(QStringLiteral("Extra vertical page space:"), this->addVerticalSpaceCheck);

    this->addVerticalSpaceAboveSpin = new QSpinBox(pagePage);
    this->addVerticalSpaceAboveSpin->setRange(0, 5000);
    this->addVerticalSpaceAboveSpin->setValue(current.addVerticalSpaceAmountAbove);
    this->addVerticalSpaceAboveSpin->setSuffix(QStringLiteral(" pt"));
    pageLayout->addRow(QStringLiteral("Vertical space above:"), this->addVerticalSpaceAboveSpin);

    this->addVerticalSpaceBelowSpin = new QSpinBox(pagePage);
    this->addVerticalSpaceBelowSpin->setRange(0, 5000);
    this->addVerticalSpaceBelowSpin->setValue(current.addVerticalSpaceAmountBelow);
    this->addVerticalSpaceBelowSpin->setSuffix(QStringLiteral(" pt"));
    pageLayout->addRow(QStringLiteral("Vertical space below:"), this->addVerticalSpaceBelowSpin);

    pagePage->setLayout(pageLayout);
    tabs->addTab(pagePage, QStringLiteral("Page"));
}

void QtSettingsDialog::addGeneralTab(QTabWidget* tabs, const QtSettings& current) {
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

    this->autoloadMostRecentCheck = new QCheckBox(generalPage);
    this->autoloadMostRecentCheck->setChecked(current.autoloadMostRecent);
    generalLayout->addRow(QStringLiteral("Autoload most recent document:"), this->autoloadMostRecentCheck);

    this->preferredLocaleCombo = new QComboBox(generalPage);
    this->preferredLocaleCombo->addItem(QStringLiteral("System default"), QString());
    for (const auto& locale: availableLocaleCodes()) {
        this->preferredLocaleCombo->addItem(QString::fromStdString(locale), QString::fromStdString(locale));
    }
    int localeIndex = this->preferredLocaleCombo->findData(QString::fromStdString(current.preferredLocale));
    if (localeIndex < 0 && !current.preferredLocale.empty()) {
        this->preferredLocaleCombo->addItem(QString::fromStdString(current.preferredLocale),
                                            QString::fromStdString(current.preferredLocale));
        localeIndex = this->preferredLocaleCombo->count() - 1;
    }
    this->preferredLocaleCombo->setCurrentIndex(localeIndex >= 0 ? localeIndex : 0);
    generalLayout->addRow(QStringLiteral("Preferred language:"), this->preferredLocaleCombo);

    this->automaticUpdateCheckEnabledCheck = new QCheckBox(generalPage);
    this->automaticUpdateCheckEnabledCheck->setChecked(current.automaticUpdateCheckEnabled);
    generalLayout->addRow(QStringLiteral("Check for updates on startup:"), this->automaticUpdateCheckEnabledCheck);

    this->presentationModeDefaultCheck = new QCheckBox(generalPage);
    this->presentationModeDefaultCheck->setChecked(current.presentationModeDefault);
    generalLayout->addRow(QStringLiteral("Start in presentation mode:"), this->presentationModeDefaultCheck);

    this->displayDpiSpin = new QSpinBox(generalPage);
    this->displayDpiSpin->setRange(-1, 1000);
    this->displayDpiSpin->setSpecialValueText(QStringLiteral("Automatic"));
    this->displayDpiSpin->setValue(current.displayDpi);
    generalLayout->addRow(QStringLiteral("Display DPI:"), this->displayDpiSpin);

    this->geoSnapCheck = new QCheckBox(generalPage);
    this->geoSnapCheck->setChecked(current.geometrySnapDefault);
    generalLayout->addRow(QStringLiteral("Geometry snap enabled:"), this->geoSnapCheck);

    this->gridSnapCheck = new QCheckBox(generalPage);
    this->gridSnapCheck->setChecked(current.gridSnapDefault);
    generalLayout->addRow(QStringLiteral("Grid snap enabled:"), this->gridSnapCheck);

    this->rotationSnapCheck = new QCheckBox(generalPage);
    this->rotationSnapCheck->setChecked(current.rotationSnapDefault);
    generalLayout->addRow(QStringLiteral("Rotation snap enabled:"), this->rotationSnapCheck);

    this->rotationSnapToleranceSpin = new QDoubleSpinBox(generalPage);
    this->rotationSnapToleranceSpin->setRange(0.01, 1.57);
    this->rotationSnapToleranceSpin->setSingleStep(0.05);
    this->rotationSnapToleranceSpin->setDecimals(2);
    this->rotationSnapToleranceSpin->setValue(current.rotationSnapTolerance);
    this->rotationSnapToleranceSpin->setSuffix(QStringLiteral(" rad"));
    generalLayout->addRow(QStringLiteral("Rotation snap tolerance:"), this->rotationSnapToleranceSpin);

    this->drawDirModsEnabledCheck = new QCheckBox(generalPage);
    this->drawDirModsEnabledCheck->setChecked(current.drawDirModsEnabled);
    generalLayout->addRow(QStringLiteral("Direction modifiers:"), this->drawDirModsEnabledCheck);

    this->drawDirModsRadiusSpin = new QSpinBox(generalPage);
    this->drawDirModsRadiusSpin->setRange(1, 500);
    this->drawDirModsRadiusSpin->setValue(current.drawDirModsRadius);
    this->drawDirModsRadiusSpin->setSuffix(QStringLiteral(" px"));
    generalLayout->addRow(QStringLiteral("Direction modifier radius:"), this->drawDirModsRadiusSpin);

    this->zoomStepSpin = new QDoubleSpinBox(generalPage);
    this->zoomStepSpin->setRange(1.0, 100.0);
    this->zoomStepSpin->setSingleStep(1.0);
    this->zoomStepSpin->setDecimals(1);
    this->zoomStepSpin->setValue(current.zoomStepPercent);
    this->zoomStepSpin->setSuffix(QStringLiteral("%"));
    generalLayout->addRow(QStringLiteral("Zoom step:"), this->zoomStepSpin);

    this->zoomStepScrollSpin = new QDoubleSpinBox(generalPage);
    this->zoomStepScrollSpin->setRange(1.0, 100.0);
    this->zoomStepScrollSpin->setSingleStep(1.0);
    this->zoomStepScrollSpin->setDecimals(1);
    this->zoomStepScrollSpin->setValue(current.zoomStepScrollPercent);
    this->zoomStepScrollSpin->setSuffix(QStringLiteral("%"));
    generalLayout->addRow(QStringLiteral("Wheel zoom step:"), this->zoomStepScrollSpin);

    this->zoomGesturesEnabledCheck = new QCheckBox(generalPage);
    this->zoomGesturesEnabledCheck->setChecked(current.zoomGesturesEnabled);
    generalLayout->addRow(QStringLiteral("Touch zoom gestures:"), this->zoomGesturesEnabledCheck);

    this->touchZoomStartThresholdSpin = new QDoubleSpinBox(generalPage);
    this->touchZoomStartThresholdSpin->setRange(0.0, 200.0);
    this->touchZoomStartThresholdSpin->setSingleStep(1.0);
    this->touchZoomStartThresholdSpin->setDecimals(1);
    this->touchZoomStartThresholdSpin->setValue(current.touchZoomStartThreshold);
    this->touchZoomStartThresholdSpin->setSuffix(QStringLiteral(" px"));
    generalLayout->addRow(QStringLiteral("Touch zoom start threshold:"), this->touchZoomStartThresholdSpin);

    this->touchInertialScrollingCheck = new QCheckBox(generalPage);
    this->touchInertialScrollingCheck->setChecked(current.touchInertialScrolling);
    this->touchInertialScrollingCheck->setToolTip(
            QStringLiteral("Stored for settings parity; Qt uses the platform touch scroll behavior."));
    generalLayout->addRow(QStringLiteral("Touch inertial scrolling:"), this->touchInertialScrollingCheck);

    this->unlimitedScrollingCheck = new QCheckBox(generalPage);
    this->unlimitedScrollingCheck->setChecked(current.unlimitedScrolling);
    generalLayout->addRow(QStringLiteral("Unlimited scrolling:"), this->unlimitedScrollingCheck);

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

    this->snapRecognizedShapesCheck = new QCheckBox(generalPage);
    this->snapRecognizedShapesCheck->setChecked(current.snapRecognizedShapesEnabled);
    generalLayout->addRow(QStringLiteral("Snap recognized shapes:"), this->snapRecognizedShapesCheck);

    this->useSpacesForTabCheck = new QCheckBox(generalPage);
    this->useSpacesForTabCheck->setChecked(current.useSpacesForTab);
    generalLayout->addRow(QStringLiteral("Use spaces for text tabs:"), this->useSpacesForTabCheck);

    this->numberOfSpacesForTabSpin = new QSpinBox(generalPage);
    this->numberOfSpacesForTabSpin->setRange(1, 32);
    this->numberOfSpacesForTabSpin->setValue(current.numberOfSpacesForTab);
    generalLayout->addRow(QStringLiteral("Spaces per tab:"), this->numberOfSpacesForTabSpin);

    this->edgePanSpeedSpin = new QDoubleSpinBox(generalPage);
    this->edgePanSpeedSpin->setRange(0.0, 200.0);
    this->edgePanSpeedSpin->setDecimals(1);
    this->edgePanSpeedSpin->setSingleStep(1.0);
    this->edgePanSpeedSpin->setValue(current.edgePanSpeed);
    generalLayout->addRow(QStringLiteral("Edge pan speed:"), this->edgePanSpeedSpin);

    this->edgePanMaxMultSpin = new QDoubleSpinBox(generalPage);
    this->edgePanMaxMultSpin->setRange(1.0, 20.0);
    this->edgePanMaxMultSpin->setDecimals(1);
    this->edgePanMaxMultSpin->setSingleStep(0.5);
    this->edgePanMaxMultSpin->setValue(current.edgePanMaxMult);
    generalLayout->addRow(QStringLiteral("Edge pan max multiplier:"), this->edgePanMaxMultSpin);

    this->strokeFilterEnabledCheck = new QCheckBox(generalPage);
    this->strokeFilterEnabledCheck->setChecked(current.strokeFilterEnabled);
    generalLayout->addRow(QStringLiteral("Stroke filter:"), this->strokeFilterEnabledCheck);

    this->strokeFilterIgnoreTimeSpin = new QSpinBox(generalPage);
    this->strokeFilterIgnoreTimeSpin->setRange(0, 5000);
    this->strokeFilterIgnoreTimeSpin->setValue(current.strokeFilterIgnoreTime);
    this->strokeFilterIgnoreTimeSpin->setSuffix(QStringLiteral(" ms"));
    generalLayout->addRow(QStringLiteral("Filter ignore time:"), this->strokeFilterIgnoreTimeSpin);

    this->strokeFilterIgnoreLengthSpin = new QDoubleSpinBox(generalPage);
    this->strokeFilterIgnoreLengthSpin->setRange(0.0, 100.0);
    this->strokeFilterIgnoreLengthSpin->setDecimals(2);
    this->strokeFilterIgnoreLengthSpin->setSingleStep(0.1);
    this->strokeFilterIgnoreLengthSpin->setValue(current.strokeFilterIgnoreLength);
    this->strokeFilterIgnoreLengthSpin->setSuffix(QStringLiteral(" mm"));
    generalLayout->addRow(QStringLiteral("Filter ignore length:"), this->strokeFilterIgnoreLengthSpin);

    this->strokeFilterSuccessiveTimeSpin = new QSpinBox(generalPage);
    this->strokeFilterSuccessiveTimeSpin->setRange(0, 5000);
    this->strokeFilterSuccessiveTimeSpin->setValue(current.strokeFilterSuccessiveTime);
    this->strokeFilterSuccessiveTimeSpin->setSuffix(QStringLiteral(" ms"));
    generalLayout->addRow(QStringLiteral("Filter successive time:"), this->strokeFilterSuccessiveTimeSpin);

    this->trySelectOnStrokeFilteredCheck = new QCheckBox(generalPage);
    this->trySelectOnStrokeFilteredCheck->setChecked(current.trySelectOnStrokeFiltered);
    generalLayout->addRow(QStringLiteral("Select on filtered stroke:"), this->trySelectOnStrokeFilteredCheck);

    this->doActionOnStrokeFilteredCheck = new QCheckBox(generalPage);
    this->doActionOnStrokeFilteredCheck->setChecked(current.doActionOnStrokeFiltered);
    this->doActionOnStrokeFilteredCheck->setToolTip(
            QStringLiteral("Stored for GTK parity; Qt does not show the legacy floating toolbox."));
    generalLayout->addRow(QStringLiteral("Action on filtered stroke:"), this->doActionOnStrokeFilteredCheck);

    this->laserPointerFadeOutSpin = new QSpinBox(generalPage);
    this->laserPointerFadeOutSpin->setRange(100, 10000);
    this->laserPointerFadeOutSpin->setSingleStep(100);
    this->laserPointerFadeOutSpin->setValue(current.laserPointerFadeOutMs);
    this->laserPointerFadeOutSpin->setSuffix(QStringLiteral(" ms"));
    generalLayout->addRow(QStringLiteral("Laser pointer fade-out:"), this->laserPointerFadeOutSpin);

    generalPage->setLayout(generalLayout);
    tabs->addTab(generalPage, QStringLiteral("General"));
}

void QtSettingsDialog::addAppearanceTab(QTabWidget* tabs, const QtSettings& current) {
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

    this->sidebarWidthSpin = new QSpinBox(appearancePage);
    this->sidebarWidthSpin->setRange(76, 600);
    this->sidebarWidthSpin->setValue(std::clamp(current.sidebarWidth, 76, 600));
    this->sidebarWidthSpin->setSuffix(QStringLiteral(" px"));
    appearanceLayout->addRow(QStringLiteral("Sidebar width:"), this->sidebarWidthSpin);

    this->sidebarOnRightCheck = new QCheckBox(appearancePage);
    this->sidebarOnRightCheck->setChecked(current.sidebarOnRight);
    appearanceLayout->addRow(QStringLiteral("Sidebar on right:"), this->sidebarOnRightCheck);

    this->scrollbarOnLeftCheck = new QCheckBox(appearancePage);
    this->scrollbarOnLeftCheck->setChecked(current.scrollbarOnLeft);
    appearanceLayout->addRow(QStringLiteral("Sidebar scrollbar on left:"), this->scrollbarOnLeftCheck);

    this->sidebarNumberingStyleCombo = new QComboBox(appearancePage);
    this->sidebarNumberingStyleCombo->addItem(QStringLiteral("None"), 0);
    this->sidebarNumberingStyleCombo->addItem(QStringLiteral("Below preview"), 1);
    this->sidebarNumberingStyleCombo->addItem(QStringLiteral("Circular badge"), 2);
    this->sidebarNumberingStyleCombo->addItem(QStringLiteral("Square badge"), 3);
    const int numberingIndex = this->sidebarNumberingStyleCombo->findData(current.sidebarNumberingStyle);
    this->sidebarNumberingStyleCombo->setCurrentIndex(numberingIndex >= 0 ? numberingIndex : 1);
    appearanceLayout->addRow(QStringLiteral("Sidebar numbering:"), this->sidebarNumberingStyleCombo);

    this->scrollbarHideTypeCombo = new QComboBox(appearancePage);
    this->scrollbarHideTypeCombo->addItem(QStringLiteral("None"), 0);
    this->scrollbarHideTypeCombo->addItem(QStringLiteral("Horizontal"), 2);
    this->scrollbarHideTypeCombo->addItem(QStringLiteral("Vertical"), 4);
    this->scrollbarHideTypeCombo->addItem(QStringLiteral("Both"), 6);
    const int scrollbarIndex = this->scrollbarHideTypeCombo->findData(current.scrollbarHideType);
    this->scrollbarHideTypeCombo->setCurrentIndex(scrollbarIndex >= 0 ? scrollbarIndex : 0);
    appearanceLayout->addRow(QStringLiteral("Hide sidebar scrollbars:"), this->scrollbarHideTypeCombo);

    this->disableScrollbarFadeoutCheck = new QCheckBox(appearancePage);
    this->disableScrollbarFadeoutCheck->setChecked(current.disableScrollbarFadeout);
    appearanceLayout->addRow(QStringLiteral("Disable scrollbar fadeout:"), this->disableScrollbarFadeoutCheck);

    this->themeVariantCombo = new QComboBox(appearancePage);
    this->themeVariantCombo->addItem(QStringLiteral("System"), QStringLiteral("system"));
    this->themeVariantCombo->addItem(QStringLiteral("Light"), QStringLiteral("light"));
    this->themeVariantCombo->addItem(QStringLiteral("Dark"), QStringLiteral("dark"));
    const QString currentTheme = QString::fromStdString(current.themeVariant);
    const int themeIndex = this->themeVariantCombo->findData(currentTheme);
    this->themeVariantCombo->setCurrentIndex(themeIndex >= 0 ? themeIndex : 0);
    appearanceLayout->addRow(QStringLiteral("Theme:"), this->themeVariantCombo);

    this->iconThemeCombo = new QComboBox(appearancePage);
    this->iconThemeCombo->addItem(QStringLiteral("Color"), QStringLiteral("color"));
    this->iconThemeCombo->addItem(QStringLiteral("Lucide"), QStringLiteral("lucide"));
    const QString currentIconTheme = QString::fromStdString(current.iconTheme);
    const int iconThemeIndex = this->iconThemeCombo->findData(currentIconTheme);
    this->iconThemeCombo->setCurrentIndex(iconThemeIndex >= 0 ? iconThemeIndex : 0);
    appearanceLayout->addRow(QStringLiteral("Icon theme:"), this->iconThemeCombo);

    this->selectionColor = colorToQColor(current.selectionColor);
    this->selectionColorButton = makeColorButton(appearancePage, this->selectionColor);
    QObject::connect(this->selectionColorButton, &QPushButton::clicked, this, [this]() {
        const QColor chosen = QColorDialog::getColor(this->selectionColor, this, QStringLiteral("Selection Colour"),
                                                     QColorDialog::ShowAlphaChannel);
        if (chosen.isValid()) {
            this->selectionColor = chosen;
            updateColorButton(this->selectionColorButton, this->selectionColor);
        }
    });
    appearanceLayout->addRow(QStringLiteral("Selection colour:"), this->selectionColorButton);

    this->backgroundColor = colorToQColor(current.backgroundColor);
    this->backgroundColorButton = makeColorButton(appearancePage, this->backgroundColor);
    QObject::connect(this->backgroundColorButton, &QPushButton::clicked, this, [this]() {
        const QColor chosen = QColorDialog::getColor(this->backgroundColor, this, QStringLiteral("Background Colour"),
                                                     QColorDialog::ShowAlphaChannel);
        if (chosen.isValid()) {
            this->backgroundColor = chosen;
            updateColorButton(this->backgroundColorButton, this->backgroundColor);
        }
    });
    appearanceLayout->addRow(QStringLiteral("Canvas background colour:"), this->backgroundColorButton);

    this->highlightPositionCheck = new QCheckBox(appearancePage);
    this->highlightPositionCheck->setChecked(current.highlightPosition);
    appearanceLayout->addRow(QStringLiteral("Cursor position highlight:"), this->highlightPositionCheck);

    this->cursorHighlightColorButton = makeColorButton(appearancePage, this->cursorHighlightColor);
    QObject::connect(this->cursorHighlightColorButton, &QPushButton::clicked, this, [this]() {
        const QColor chosen = QColorDialog::getColor(this->cursorHighlightColor, this,
                                                     QStringLiteral("Cursor Highlight Colour"),
                                                     QColorDialog::ShowAlphaChannel);
        if (chosen.isValid()) {
            this->cursorHighlightColor = chosen;
            updateColorButton(this->cursorHighlightColorButton, this->cursorHighlightColor);
        }
    });
    appearanceLayout->addRow(QStringLiteral("Cursor highlight colour:"), this->cursorHighlightColorButton);

    this->cursorHighlightBorderColorButton = makeColorButton(appearancePage, this->cursorHighlightBorderColor);
    QObject::connect(this->cursorHighlightBorderColorButton, &QPushButton::clicked, this, [this]() {
        const QColor chosen = QColorDialog::getColor(this->cursorHighlightBorderColor, this,
                                                     QStringLiteral("Cursor Highlight Border Colour"),
                                                     QColorDialog::ShowAlphaChannel);
        if (chosen.isValid()) {
            this->cursorHighlightBorderColor = chosen;
            updateColorButton(this->cursorHighlightBorderColorButton, this->cursorHighlightBorderColor);
        }
    });
    appearanceLayout->addRow(QStringLiteral("Cursor highlight border:"), this->cursorHighlightBorderColorButton);

    this->cursorHighlightRadiusSpin = new QSpinBox(appearancePage);
    this->cursorHighlightRadiusSpin->setRange(1, 500);
    this->cursorHighlightRadiusSpin->setValue(current.cursorHighlightRadius);
    this->cursorHighlightRadiusSpin->setSuffix(QStringLiteral(" px"));
    appearanceLayout->addRow(QStringLiteral("Cursor highlight radius:"), this->cursorHighlightRadiusSpin);

    this->cursorHighlightBorderWidthSpin = new QSpinBox(appearancePage);
    this->cursorHighlightBorderWidthSpin->setRange(0, 50);
    this->cursorHighlightBorderWidthSpin->setValue(current.cursorHighlightBorderWidth);
    this->cursorHighlightBorderWidthSpin->setSuffix(QStringLiteral(" px"));
    appearanceLayout->addRow(QStringLiteral("Cursor highlight border width:"), this->cursorHighlightBorderWidthSpin);

    this->recolorMainViewCheck = new QCheckBox(appearancePage);
    this->recolorMainViewCheck->setChecked(current.recolorMainView);
    appearanceLayout->addRow(QStringLiteral("Recolor drawing area:"), this->recolorMainViewCheck);
    this->recolorSidebarCheck = new QCheckBox(appearancePage);
    this->recolorSidebarCheck->setChecked(current.recolorSidebarMiniatures);
    appearanceLayout->addRow(QStringLiteral("Recolor sidebar previews:"), this->recolorSidebarCheck);

    this->recolorLightColor = colorToQColor(current.recolorLight);
    this->recolorDarkColor = colorToQColor(current.recolorDark);
    this->recolorLightButton = makeColorButton(appearancePage, this->recolorLightColor);
    this->recolorDarkButton = makeColorButton(appearancePage, this->recolorDarkColor);
    QObject::connect(this->recolorLightButton, &QPushButton::clicked, this, [this]() {
        const QColor chosen = QColorDialog::getColor(this->recolorLightColor, this, QStringLiteral("Recolor Light"),
                                                     QColorDialog::ShowAlphaChannel);
        if (chosen.isValid()) {
            this->recolorLightColor = chosen;
            updateColorButton(this->recolorLightButton, this->recolorLightColor);
        }
    });
    QObject::connect(this->recolorDarkButton, &QPushButton::clicked, this, [this]() {
        const QColor chosen = QColorDialog::getColor(this->recolorDarkColor, this, QStringLiteral("Recolor Dark"),
                                                     QColorDialog::ShowAlphaChannel);
        if (chosen.isValid()) {
            this->recolorDarkColor = chosen;
            updateColorButton(this->recolorDarkButton, this->recolorDarkColor);
        }
    });
    appearanceLayout->addRow(QStringLiteral("Recolor light:"), this->recolorLightButton);
    appearanceLayout->addRow(QStringLiteral("Recolor dark:"), this->recolorDarkButton);

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
}

void QtSettingsDialog::addToolbarTab(QTabWidget* tabs, const QtSettings& current, const std::vector<QtToolbarProfileOption>& toolbarProfiles) {
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
}

void QtSettingsDialog::addPdfTab(QTabWidget* tabs, const QtSettings& current) {
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
    this->pdfPageRerenderThresholdSpin = new QDoubleSpinBox(pdfPage);
    this->pdfPageRerenderThresholdSpin->setRange(0.0, 100.0);
    this->pdfPageRerenderThresholdSpin->setDecimals(1);
    this->pdfPageRerenderThresholdSpin->setSingleStep(0.5);
    this->pdfPageRerenderThresholdSpin->setValue(current.pdfPageRerenderThreshold);
    this->pdfPageRerenderThresholdSpin->setSuffix(QStringLiteral("%"));
    pdfLayout->addRow(QStringLiteral("PDF rerender threshold:"), this->pdfPageRerenderThresholdSpin);
    this->emptyLastPageAppendCombo = new QComboBox(pdfPage);
    this->emptyLastPageAppendCombo->addItem(QStringLiteral("Disabled"), QStringLiteral("disabled"));
    this->emptyLastPageAppendCombo->addItem(QStringLiteral("When drawing on last page"),
                                            QStringLiteral("onDrawOfLastPage"));
    this->emptyLastPageAppendCombo->addItem(QStringLiteral("When scrolling to end"),
                                            QStringLiteral("onScrollOfLastPage"));
    const int appendIndex =
            this->emptyLastPageAppendCombo->findData(QString::fromStdString(current.emptyLastPageAppend));
    this->emptyLastPageAppendCombo->setCurrentIndex(appendIndex >= 0 ? appendIndex : 0);
    pdfLayout->addRow(QStringLiteral("Empty last page append:"), this->emptyLastPageAppendCombo);
    pdfPage->setLayout(pdfLayout);
    tabs->addTab(pdfPage, QStringLiteral("PDF"));
}

void QtSettingsDialog::addLatexTab(QTabWidget* tabs, const QtSettings& current) {
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

    this->latexAutoCheckDependenciesCheck = new QCheckBox(latexPage);
    this->latexAutoCheckDependenciesCheck->setChecked(current.latexAutoCheckDependencies);
    latexLayout->addRow(QStringLiteral("Auto-check dependencies:"), this->latexAutoCheckDependenciesCheck);

    this->latexDefaultTextEdit = new QLineEdit(QString::fromStdString(current.latexDefaultText), latexPage);
    latexLayout->addRow(QStringLiteral("Default text:"), this->latexDefaultTextEdit);

    this->latexGenCmdEdit = new QLineEdit(QString::fromStdString(current.latexGenCmd), latexPage);
    latexLayout->addRow(QStringLiteral("Generation command:"), this->latexGenCmdEdit);

    this->latexSourceViewThemeIdEdit = new QLineEdit(QString::fromStdString(current.latexSourceViewThemeId), latexPage);
    latexLayout->addRow(QStringLiteral("Source theme id:"), this->latexSourceViewThemeIdEdit);

    this->latexSourceViewAutoIndentCheck = new QCheckBox(latexPage);
    this->latexSourceViewAutoIndentCheck->setChecked(current.latexSourceViewAutoIndent);
    latexLayout->addRow(QStringLiteral("Source auto-indent:"), this->latexSourceViewAutoIndentCheck);

    this->latexSourceViewSyntaxHighlightCheck = new QCheckBox(latexPage);
    this->latexSourceViewSyntaxHighlightCheck->setChecked(current.latexSourceViewSyntaxHighlight);
    latexLayout->addRow(QStringLiteral("Source syntax highlight:"), this->latexSourceViewSyntaxHighlightCheck);

    this->latexSourceViewShowLineNumbersCheck = new QCheckBox(latexPage);
    this->latexSourceViewShowLineNumbersCheck->setChecked(current.latexSourceViewShowLineNumbers);
    latexLayout->addRow(QStringLiteral("Source line numbers:"), this->latexSourceViewShowLineNumbersCheck);

    this->latexEditorFontEdit = new QLineEdit(QString::fromStdString(current.latexEditorFont), latexPage);
    latexLayout->addRow(QStringLiteral("Editor font:"), this->latexEditorFontEdit);

    this->latexUseCustomEditorFontCheck = new QCheckBox(latexPage);
    this->latexUseCustomEditorFontCheck->setChecked(current.latexUseCustomEditorFont);
    latexLayout->addRow(QStringLiteral("Use custom editor font:"), this->latexUseCustomEditorFontCheck);

    this->latexEditorWordWrapCheck = new QCheckBox(latexPage);
    this->latexEditorWordWrapCheck->setChecked(current.latexEditorWordWrap);
    latexLayout->addRow(QStringLiteral("Editor word wrap:"), this->latexEditorWordWrapCheck);

    this->latexUseExternalEditorCheck = new QCheckBox(latexPage);
    this->latexUseExternalEditorCheck->setChecked(current.latexUseExternalEditor);
    latexLayout->addRow(QStringLiteral("Use external editor:"), this->latexUseExternalEditorCheck);

    this->latexExternalEditorAutoConfirmCheck = new QCheckBox(latexPage);
    this->latexExternalEditorAutoConfirmCheck->setChecked(current.latexExternalEditorAutoConfirm);
    latexLayout->addRow(QStringLiteral("External editor auto-confirm:"), this->latexExternalEditorAutoConfirmCheck);

    this->latexExternalEditorCmdEdit = new QLineEdit(QString::fromStdString(current.latexExternalEditorCmd), latexPage);
    latexLayout->addRow(QStringLiteral("External editor command:"), this->latexExternalEditorCmdEdit);

    this->latexTemporaryFileExtEdit = new QLineEdit(QString::fromStdString(current.latexTemporaryFileExt), latexPage);
    latexLayout->addRow(QStringLiteral("Temporary file extension:"), this->latexTemporaryFileExtEdit);

    latexPage->setLayout(latexLayout);
    tabs->addTab(latexPage, QStringLiteral("LaTeX"));
}

void QtSettingsDialog::addAudioTab(QTabWidget* tabs, const QtSettings& current, const std::vector<QtAudioDeviceOption>& audioInputDevices, const std::vector<QtAudioDeviceOption>& audioOutputDevices) {
    // --- Audio tab ---
    auto* audioPage = new QWidget(this);
    auto* audioLayout = new QFormLayout(audioPage);

    this->disableAudioCheck = new QCheckBox(audioPage);
    this->disableAudioCheck->setChecked(current.disableAudio);
    audioLayout->addRow(QStringLiteral("Disable audio:"), this->disableAudioCheck);

    this->audioInputDeviceCombo = new QComboBox(audioPage);
    populateAudioDeviceCombo(this->audioInputDeviceCombo, audioInputDevices, current.audioInputDevice);
    audioLayout->addRow(QStringLiteral("Input device:"), this->audioInputDeviceCombo);

    this->audioOutputDeviceCombo = new QComboBox(audioPage);
    populateAudioDeviceCombo(this->audioOutputDeviceCombo, audioOutputDevices, current.audioOutputDevice);
    audioLayout->addRow(QStringLiteral("Output device:"), this->audioOutputDeviceCombo);

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
}

void QtSettingsDialog::addDevicesTab(QTabWidget* tabs, const QtSettings& current) {
    // --- Devices tab ---
    auto* devicesPage = new QWidget(this);
    auto* devicesLayout = new QVBoxLayout(devicesPage);
    auto* devicesForm = new QFormLayout();
    this->eraserCursorHiddenCheck = new QCheckBox(devicesPage);
    this->eraserCursorHiddenCheck->setChecked(current.eraserCursorHidden);
    devicesForm->addRow(QStringLiteral("Hide eraser cursor:"), this->eraserCursorHiddenCheck);

    this->ignoredStylusEventsSpin = new QSpinBox(devicesPage);
    this->ignoredStylusEventsSpin->setRange(0, 20);
    this->ignoredStylusEventsSpin->setValue(current.ignoredStylusEvents);
    devicesForm->addRow(QStringLiteral("Ignore first stylus events:"), this->ignoredStylusEventsSpin);

    this->inputSystemTPCButtonCheck = new QCheckBox(devicesPage);
    this->inputSystemTPCButtonCheck->setChecked(current.inputSystemTPCButton);
    this->inputSystemTPCButtonCheck->setToolTip(
            QStringLiteral("Stored for settings parity; Qt uses native tablet button state."));
    devicesForm->addRow(QStringLiteral("TPC button emulation:"), this->inputSystemTPCButtonCheck);

    this->inputSystemDrawOutsideWindowCheck = new QCheckBox(devicesPage);
    this->inputSystemDrawOutsideWindowCheck->setChecked(current.inputSystemDrawOutsideWindow);
    this->inputSystemDrawOutsideWindowCheck->setToolTip(
            QStringLiteral("Stored for settings parity; Qt receives tablet events through the active window."));
    devicesForm->addRow(QStringLiteral("Draw outside window:"), this->inputSystemDrawOutsideWindowCheck);

    this->eraserTipActionCombo = new QComboBox(devicesPage);
    populatePointerActionCombo(this->eraserTipActionCombo, current.buttonMatrix.eraserTipAction);
    devicesForm->addRow(QStringLiteral("Eraser tip:"), this->eraserTipActionCombo);
    this->stylusButton1ActionCombo = new QComboBox(devicesPage);
    populatePointerActionCombo(this->stylusButton1ActionCombo, current.buttonMatrix.stylusButton1Action);
    devicesForm->addRow(QStringLiteral("Stylus button 1:"), this->stylusButton1ActionCombo);
    this->stylusButton2ActionCombo = new QComboBox(devicesPage);
    populatePointerActionCombo(this->stylusButton2ActionCombo, current.buttonMatrix.stylusButton2Action);
    devicesForm->addRow(QStringLiteral("Stylus button 2:"), this->stylusButton2ActionCombo);
    this->mouseLeftActionCombo = new QComboBox(devicesPage);
    populatePointerActionCombo(this->mouseLeftActionCombo, current.buttonMatrix.mouseLeftAction);
    devicesForm->addRow(QStringLiteral("Mouse left:"), this->mouseLeftActionCombo);
    this->mouseMiddleActionCombo = new QComboBox(devicesPage);
    populatePointerActionCombo(this->mouseMiddleActionCombo, current.buttonMatrix.mouseMiddleAction);
    devicesForm->addRow(QStringLiteral("Mouse middle:"), this->mouseMiddleActionCombo);
    this->mouseRightActionCombo = new QComboBox(devicesPage);
    populatePointerActionCombo(this->mouseRightActionCombo, current.buttonMatrix.mouseRightAction);
    devicesForm->addRow(QStringLiteral("Mouse right:"), this->mouseRightActionCombo);
    this->mouseBackActionCombo = new QComboBox(devicesPage);
    populatePointerActionCombo(this->mouseBackActionCombo, current.buttonMatrix.mouseBackAction);
    devicesForm->addRow(QStringLiteral("Mouse back:"), this->mouseBackActionCombo);
    this->mouseForwardActionCombo = new QComboBox(devicesPage);
    populatePointerActionCombo(this->mouseForwardActionCombo, current.buttonMatrix.mouseForwardAction);
    devicesForm->addRow(QStringLiteral("Mouse forward:"), this->mouseForwardActionCombo);
    this->touchActionCombo = new QComboBox(devicesPage);
    populatePointerActionCombo(this->touchActionCombo, current.buttonMatrix.touchAction);
    devicesForm->addRow(QStringLiteral("Touch contact:"), this->touchActionCombo);
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

    devicesLayout->addWidget(new QLabel(QStringLiteral("Per-device button matrix:"), devicesPage));
    this->inputDeviceMatrixTable = new QTableWidget(devicesPage);
    this->inputDeviceMatrixTable->setColumnCount(12);
    this->inputDeviceMatrixTable->setHorizontalHeaderLabels(
            {QStringLiteral("Device"), QStringLiteral("Type"), QStringLiteral("Custom"), QStringLiteral("Eraser tip"),
             QStringLiteral("Stylus 1"), QStringLiteral("Stylus 2"), QStringLiteral("Mouse left"),
             QStringLiteral("Mouse middle"), QStringLiteral("Mouse right"), QStringLiteral("Mouse back"),
             QStringLiteral("Mouse forward"), QStringLiteral("Touch")});
    this->inputDeviceMatrixTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    this->inputDeviceMatrixTable->setEditTriggers(QAbstractItemView::NoEditTriggers);

    std::vector<QtInputDeviceButtonProfile> deviceProfiles = current.inputDeviceButtonProfiles;
    for (const auto* device: devices) {
        if (!device) {
            continue;
        }
        const QString key = qtInputDeviceKey(device);
        const auto existing =
                std::find_if(deviceProfiles.begin(), deviceProfiles.end(),
                             [&key](const QtInputDeviceButtonProfile& profile) {
                                 return QString::fromStdString(profile.key) == key;
                             });
        if (existing == deviceProfiles.end()) {
            deviceProfiles.push_back({.key = key.toStdString(),
                                      .displayName = device->name().toStdString(),
                                      .deviceType = qtInputDeviceTypeName(device).toStdString(),
                                      .customButtonMatrix = false,
                                      .buttonMatrix = current.buttonMatrix});
        }
    }

    for (const auto& profile: deviceProfiles) {
        addDeviceMatrixRow(this->inputDeviceMatrixTable, profile, current.buttonMatrix);
    }
    this->inputDeviceMatrixTable->resizeColumnsToContents();
    devicesLayout->addWidget(this->inputDeviceMatrixTable, 1);
    devicesLayout->addStretch(1);
    devicesPage->setLayout(devicesLayout);
    tabs->addTab(devicesPage, QStringLiteral("Devices"));
}
