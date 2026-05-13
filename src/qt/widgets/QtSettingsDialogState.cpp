/*
 * VertexNote
 *
 * Qt settings/preferences dialog state extraction.
 */

#include "QtSettingsDialog.h"

#include <cstdint>
#include <string>
#include <utility>
#include <vector>

#include <QCheckBox>
#include <QColor>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QFontComboBox>
#include <QLineEdit>
#include <QSpinBox>
#include <QTableWidget>
#include <QTableWidgetItem>

#include "model/FormatDefinitions.h"

namespace {

auto qColorToColor(const QColor& color) -> Color {
    return Color{static_cast<uint8_t>(color.red()), static_cast<uint8_t>(color.green()),
                 static_cast<uint8_t>(color.blue()), static_cast<uint8_t>(color.alpha())};
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

auto pointerActionFromCombo(const QComboBox* combo) -> QtPointerButtonAction {
    return combo ? static_cast<QtPointerButtonAction>(combo->currentData().toInt()) : QtPointerButtonAction::None;
}

auto rowDeviceMatrix(const QTableWidget* table, int row) -> QtPointerButtonMatrix {
    return {.eraserTipAction = pointerActionFromCombo(qobject_cast<QComboBox*>(table->cellWidget(row, 3))),
            .stylusButton1Action = pointerActionFromCombo(qobject_cast<QComboBox*>(table->cellWidget(row, 4))),
            .stylusButton2Action = pointerActionFromCombo(qobject_cast<QComboBox*>(table->cellWidget(row, 5))),
            .mouseLeftAction = pointerActionFromCombo(qobject_cast<QComboBox*>(table->cellWidget(row, 6))),
            .mouseMiddleAction = pointerActionFromCombo(qobject_cast<QComboBox*>(table->cellWidget(row, 7))),
            .mouseRightAction = pointerActionFromCombo(qobject_cast<QComboBox*>(table->cellWidget(row, 8))),
            .mouseBackAction = pointerActionFromCombo(qobject_cast<QComboBox*>(table->cellWidget(row, 9))),
            .mouseForwardAction = pointerActionFromCombo(qobject_cast<QComboBox*>(table->cellWidget(row, 10))),
            .touchAction = pointerActionFromCombo(qobject_cast<QComboBox*>(table->cellWidget(row, 11)))};
}

}  // namespace

auto QtSettingsDialog::settings() const -> QtSettings {
    std::vector<QtInputDeviceButtonProfile> inputDeviceButtonProfiles;
    if (this->inputDeviceMatrixTable) {
        inputDeviceButtonProfiles.reserve(static_cast<std::size_t>(this->inputDeviceMatrixTable->rowCount()));
        for (int row = 0; row < this->inputDeviceMatrixTable->rowCount(); ++row) {
            const auto* customItem = this->inputDeviceMatrixTable->item(row, 2);
            if (!customItem || customItem->checkState() != Qt::Checked) {
                continue;
            }
            const auto* deviceItem = this->inputDeviceMatrixTable->item(row, 0);
            const auto* typeItem = this->inputDeviceMatrixTable->item(row, 1);
            inputDeviceButtonProfiles.push_back(
                    {.key = deviceItem ? deviceItem->data(Qt::UserRole).toString().toStdString() : std::string(),
                     .displayName = deviceItem ? deviceItem->text().toStdString() : std::string(),
                     .deviceType = typeItem ? typeItem->text().toStdString() : std::string(),
                     .customButtonMatrix = true,
                     .buttonMatrix = rowDeviceMatrix(this->inputDeviceMatrixTable, row)});
        }
    }

    return {
            .defaultPenWidth = this->penWidthSpin->value(),
            .defaultHighlighterWidth = this->highlighterWidthSpin->value(),
            .defaultEraserWidth = this->eraserWidthSpin->value(),
            .defaultFontName = this->defaultFontCombo->currentFont().family().toStdString(),
            .defaultFontSize = this->defaultFontSizeSpin->value(),
            .defaultPressureSensitive = this->pressureCheck->isChecked(),
            .defaultEraserMode = this->eraserModeCombo->currentIndex() == 1 ? QtEraserMode::Segment
                                                                            : QtEraserMode::Standard,
            .defaultPageWidth = this->pageWidthSpin->value() * currentSizeUnitScale(this->sizeUnitCombo),
            .defaultPageHeight = this->pageHeightSpin->value() * currentSizeUnitScale(this->sizeUnitCombo),
            .undoHistoryLimit = this->undoLimitSpin->value(),
            .autosaveEnabled = this->autosaveEnabledCheck->isChecked(),
            .autosaveTimeoutMinutes = this->autosaveTimeoutSpin->value(),
            .autoloadMostRecent = this->autoloadMostRecentCheck->isChecked(),
            .preferredLocale = this->preferredLocaleCombo->currentData().toString().toStdString(),
            .automaticUpdateCheckEnabled = this->automaticUpdateCheckEnabledCheck->isChecked(),
            .presentationModeDefault = this->presentationModeDefaultCheck->isChecked(),
            .displayDpi = this->displayDpiSpin->value(),
            .addHorizontalSpace = this->addHorizontalSpaceCheck->isChecked(),
            .addHorizontalSpaceAmountRight = this->addHorizontalSpaceRightSpin->value(),
            .addHorizontalSpaceAmountLeft = this->addHorizontalSpaceLeftSpin->value(),
            .addVerticalSpace = this->addVerticalSpaceCheck->isChecked(),
            .addVerticalSpaceAmountAbove = this->addVerticalSpaceAboveSpin->value(),
            .addVerticalSpaceAmountBelow = this->addVerticalSpaceBelowSpin->value(),
            .geometrySnapDefault = this->geoSnapCheck->isChecked(),
            .gridSnapDefault = this->gridSnapCheck->isChecked(),
            .rotationSnapDefault = this->rotationSnapCheck->isChecked(),
            .rotationSnapTolerance = this->rotationSnapToleranceSpin->value(),
            .drawDirModsEnabled = this->drawDirModsEnabledCheck->isChecked(),
            .drawDirModsRadius = this->drawDirModsRadiusSpin->value(),
            .zoomStepPercent = this->zoomStepSpin->value(),
            .zoomStepScrollPercent = this->zoomStepScrollSpin->value(),
            .zoomGesturesEnabled = this->zoomGesturesEnabledCheck->isChecked(),
            .touchZoomStartThreshold = this->touchZoomStartThresholdSpin->value(),
            .touchInertialScrolling = this->touchInertialScrollingCheck->isChecked(),
            .unlimitedScrolling = this->unlimitedScrollingCheck->isChecked(),
            .touchDrawingDefault = this->touchDrawingCheck->isChecked(),
            .minimumPressure = this->minimumPressureSpin->value(),
            .pressureMultiplier = this->pressureMultiplierSpin->value(),
            .pressureGuessing = this->pressureGuessingCheck->isChecked(),
            .strokeStabilizerEnabled = this->strokeStabilizerEnabledCheck->isChecked(),
            .strokeStabilizerSamples = this->strokeStabilizerSamplesSpin->value(),
            .strokeStabilizerStrength = this->strokeStabilizerStrengthSpin->value(),
            .strokeStabilizerFinalizeStroke = this->strokeStabilizerFinalizeCheck->isChecked(),
            .strokeStabilizerAveragingMethod = this->strokeStabilizerAveragingCombo->currentData().toInt(),
            .strokeStabilizerPreprocessor = this->strokeStabilizerPreprocessorCombo->currentData().toInt(),
            .strokeStabilizerSigma = this->strokeStabilizerSigmaSpin->value(),
            .strokeStabilizerDeadzoneRadius = this->strokeStabilizerDeadzoneRadiusSpin->value(),
            .strokeStabilizerDrag = this->strokeStabilizerDragSpin->value(),
            .strokeStabilizerMass = this->strokeStabilizerMassSpin->value(),
            .strokeStabilizerCuspDetection = this->strokeStabilizerCuspDetectionCheck->isChecked(),
            .restoreLineWidthEnabled = this->restoreLineWidthCheck->isChecked(),
            .snapGridTolerance = this->snapGridToleranceSpin->value(),
            .snapGridSize = this->snapGridSizeSpin->value(),
            .vertexSnapMarkerSize = this->vertexSnapMarkerSizeSpin->value(),
            .geometrySelectionModeDefault = static_cast<QtGeometrySelectionMode>(
                    this->geometrySelectionModeCombo->currentData().toInt()),
            .strokeRecognizerMinSize = this->strokeRecognizerMinSizeSpin->value(),
            .snapRecognizedShapesEnabled = this->snapRecognizedShapesCheck->isChecked(),
            .laserPointerFadeOutMs = this->laserPointerFadeOutSpin->value(),
            .useSpacesForTab = this->useSpacesForTabCheck->isChecked(),
            .numberOfSpacesForTab = this->numberOfSpacesForTabSpin->value(),
            .edgePanSpeed = this->edgePanSpeedSpin->value(),
            .edgePanMaxMult = this->edgePanMaxMultSpin->value(),
            .strokeFilterEnabled = this->strokeFilterEnabledCheck->isChecked(),
            .strokeFilterIgnoreTime = this->strokeFilterIgnoreTimeSpin->value(),
            .strokeFilterIgnoreLength = this->strokeFilterIgnoreLengthSpin->value(),
            .strokeFilterSuccessiveTime = this->strokeFilterSuccessiveTimeSpin->value(),
            .doActionOnStrokeFiltered = this->doActionOnStrokeFilteredCheck->isChecked(),
            .trySelectOnStrokeFiltered = this->trySelectOnStrokeFilteredCheck->isChecked(),
            .eraserCursorHidden = this->eraserCursorHiddenCheck->isChecked(),
            .ignoredStylusEvents = this->ignoredStylusEventsSpin->value(),
            .inputSystemTPCButton = this->inputSystemTPCButtonCheck->isChecked(),
            .inputSystemDrawOutsideWindow = this->inputSystemDrawOutsideWindowCheck->isChecked(),
            .buttonMatrix = {.eraserTipAction = static_cast<QtPointerButtonAction>(this->eraserTipActionCombo->currentData().toInt()),
                             .stylusButton1Action =
                                     static_cast<QtPointerButtonAction>(this->stylusButton1ActionCombo->currentData().toInt()),
                             .stylusButton2Action =
                                     static_cast<QtPointerButtonAction>(this->stylusButton2ActionCombo->currentData().toInt()),
                             .mouseLeftAction =
                                     static_cast<QtPointerButtonAction>(this->mouseLeftActionCombo->currentData().toInt()),
                             .mouseMiddleAction =
                                     static_cast<QtPointerButtonAction>(this->mouseMiddleActionCombo->currentData().toInt()),
                             .mouseRightAction =
                                     static_cast<QtPointerButtonAction>(this->mouseRightActionCombo->currentData().toInt()),
                             .mouseBackAction =
                                     static_cast<QtPointerButtonAction>(this->mouseBackActionCombo->currentData().toInt()),
                             .mouseForwardAction =
                                     static_cast<QtPointerButtonAction>(this->mouseForwardActionCombo->currentData().toInt()),
                             .touchAction = static_cast<QtPointerButtonAction>(this->touchActionCombo->currentData().toInt())},
            .inputDeviceButtonProfiles = std::move(inputDeviceButtonProfiles),
            .showFilePathInTitlebar = this->showFilePathInTitlebarCheck->isChecked(),
            .showPageNumberInTitlebar = this->showPageNumberInTitlebarCheck->isChecked(),
            .showPageShadow = this->showPageShadowCheck->isChecked(),
            .sidebarWidth = this->sidebarWidthSpin->value(),
            .sidebarOnRight = this->sidebarOnRightCheck->isChecked(),
            .scrollbarOnLeft = this->scrollbarOnLeftCheck->isChecked(),
            .sidebarNumberingStyle = this->sidebarNumberingStyleCombo->currentData().toInt(),
            .scrollbarHideType = this->scrollbarHideTypeCombo->currentData().toInt(),
            .disableScrollbarFadeout = this->disableScrollbarFadeoutCheck->isChecked(),
            .themeVariant = this->themeVariantCombo->currentData().toString().toStdString(),
            .iconTheme = this->iconThemeCombo->currentData().toString().toStdString(),
            .selectionColor = qColorToColor(this->selectionColor),
            .backgroundColor = qColorToColor(this->backgroundColor),
            .highlightPosition = this->highlightPositionCheck->isChecked(),
            .cursorHighlightColor = qColorToColor(this->cursorHighlightColor),
            .cursorHighlightBorderColor = qColorToColor(this->cursorHighlightBorderColor),
            .cursorHighlightRadius = this->cursorHighlightRadiusSpin->value(),
            .cursorHighlightBorderWidth = this->cursorHighlightBorderWidthSpin->value(),
            .recolorMainView = this->recolorMainViewCheck->isChecked(),
            .recolorSidebarMiniatures = this->recolorSidebarCheck->isChecked(),
            .recolorLight = qColorToColor(this->recolorLightColor),
            .recolorDark = qColorToColor(this->recolorDarkColor),
            .colorPalettePath = this->colorPalettePathEdit->text().trimmed().toStdString(),
            .autoloadPdfXoj = this->autoloadPdfXojCheck->isChecked(),
            .defaultPdfExportName = this->defaultPdfExportNameEdit->text().trimmed().toStdString(),
            .pdfPageCacheSize = this->pdfPageCacheSizeSpin->value(),
            .pdfPreloadPagesBefore = this->pdfPreloadBeforeSpin->value(),
            .pdfPreloadPagesAfter = this->pdfPreloadAfterSpin->value(),
            .pdfEagerPageCleanup = this->pdfEagerCleanupCheck->isChecked(),
            .pdfPageRerenderThreshold = this->pdfPageRerenderThresholdSpin->value(),
            .emptyLastPageAppend = this->emptyLastPageAppendCombo->currentData().toString().toStdString(),
            .sizeUnit = currentSizeUnitName(this->sizeUnitCombo),
            .latexTemplatePath = this->latexTemplatePathEdit->text().trimmed().toStdString(),
            .latexAutoCheckDependencies = this->latexAutoCheckDependenciesCheck->isChecked(),
            .latexDefaultText = this->latexDefaultTextEdit->text().toStdString(),
            .latexGenCmd = this->latexGenCmdEdit->text().trimmed().toStdString(),
            .latexSourceViewThemeId = this->latexSourceViewThemeIdEdit->text().trimmed().toStdString(),
            .latexSourceViewAutoIndent = this->latexSourceViewAutoIndentCheck->isChecked(),
            .latexSourceViewSyntaxHighlight = this->latexSourceViewSyntaxHighlightCheck->isChecked(),
            .latexSourceViewShowLineNumbers = this->latexSourceViewShowLineNumbersCheck->isChecked(),
            .latexEditorFont = this->latexEditorFontEdit->text().trimmed().toStdString(),
            .latexUseCustomEditorFont = this->latexUseCustomEditorFontCheck->isChecked(),
            .latexEditorWordWrap = this->latexEditorWordWrapCheck->isChecked(),
            .latexUseExternalEditor = this->latexUseExternalEditorCheck->isChecked(),
            .latexExternalEditorAutoConfirm = this->latexExternalEditorAutoConfirmCheck->isChecked(),
            .latexExternalEditorCmd = this->latexExternalEditorCmdEdit->text().trimmed().toStdString(),
            .latexTemporaryFileExt = this->latexTemporaryFileExtEdit->text().trimmed().toStdString(),
            .audioFolder = this->audioFolderEdit->text().trimmed().toStdString(),
            .lastOpenPath = this->lastOpenPath,
            .lastSavePath = this->lastSavePath,
            .lastImagePath = this->lastImagePath,
            .lastPdfPath = this->lastPdfPath,
            .lastExportPath = this->lastExportPath,
            .disableAudio = this->disableAudioCheck->isChecked(),
            .audioInputDevice = this->audioInputDeviceCombo->currentData().toInt(),
            .audioOutputDevice = this->audioOutputDeviceCombo->currentData().toInt(),
            .audioSampleRate = this->audioSampleRateSpin->value(),
            .audioGain = this->audioGainSpin->value(),
            .defaultSeekTimeSeconds = this->defaultSeekTimeSpin->value(),
            .toolbarProfileId = this->toolbarProfileCombo->currentData().toString().toStdString(),
    };
}
