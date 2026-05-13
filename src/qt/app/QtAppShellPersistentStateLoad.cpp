/*
 * VertexNote
 *
 * Qt app shell persisted UI state loading.
 */

#include "QtAppShell.h"

#include <algorithm>
#include <cstdlib>
#include <filesystem>
#include <string>
#include <vector>

#include <QByteArray>
#include <QSettings>
#include <QString>
#include <QStringList>

#include "QtToolbarProfileStore.h"
#include "util/PathUtil.h"

namespace {

constexpr int QT_SHELL_LAYOUT_VERSION = 6;

auto settingsPointerAction(QSettings& settings, const QString& key, QtPointerButtonAction fallback)
        -> QtPointerButtonAction {
    return static_cast<QtPointerButtonAction>(settings.value(key, static_cast<int>(fallback)).toInt());
}

auto settingsGeometrySelectionMode(QSettings& settings, const QString& key,
                                   QtGeometrySelectionMode fallback) -> QtGeometrySelectionMode {
    const int value = settings.value(key, static_cast<int>(fallback)).toInt();
    if (value < static_cast<int>(QtGeometrySelectionMode::Vertex) ||
        value > static_cast<int>(QtGeometrySelectionMode::Object)) {
        return fallback;
    }
    return static_cast<QtGeometrySelectionMode>(value);
}

void applyQtPreferredLocale(const std::string& preferredLocale) {
    qputenv("LANGUAGE", QByteArray(preferredLocale.c_str(), static_cast<qsizetype>(preferredLocale.size())));
}

}  // namespace
void QtAppShell::loadPersistentUiState() {
    QSettings settings(QStringLiteral("VertexNote"), QStringLiteral("VertexNoteQtShell"));
    const int savedLayoutVersion = settings.value(QStringLiteral("general/uiLayoutVersion"), 0).toInt();

    this->currentSettings.defaultPenWidth =
            settings.value(QStringLiteral("tools/defaultPenWidth"), this->currentSettings.defaultPenWidth).toDouble();
    this->currentSettings.defaultHighlighterWidth =
            settings.value(QStringLiteral("tools/defaultHighlighterWidth"), this->currentSettings.defaultHighlighterWidth)
                    .toDouble();
    this->currentSettings.defaultEraserWidth =
            settings.value(QStringLiteral("tools/defaultEraserWidth"), this->currentSettings.defaultEraserWidth)
                    .toDouble();
    this->currentSettings.defaultPressureSensitive =
            settings.value(QStringLiteral("tools/defaultPressureSensitive"), this->currentSettings.defaultPressureSensitive)
                    .toBool();
    this->currentSettings.defaultEraserMode = settings.value(
                                                      QStringLiteral("tools/defaultEraserMode"),
                                                      static_cast<int>(this->currentSettings.defaultEraserMode))
                                                      .toInt() == static_cast<int>(QtEraserMode::Segment)
                                                      ? QtEraserMode::Segment
                                                      : QtEraserMode::Standard;
    this->currentSettings.defaultPageWidth =
            settings.value(QStringLiteral("page/defaultWidth"), this->currentSettings.defaultPageWidth).toDouble();
    this->currentSettings.defaultPageHeight =
            settings.value(QStringLiteral("page/defaultHeight"), this->currentSettings.defaultPageHeight).toDouble();
    this->currentSettings.defaultFontName =
            settings.value(QStringLiteral("tools/defaultFontName"),
                           QString::fromStdString(this->currentSettings.defaultFontName))
                    .toString()
                    .toStdString();
    this->currentSettings.defaultFontSize =
            settings.value(QStringLiteral("tools/defaultFontSize"), this->currentSettings.defaultFontSize).toDouble();
    this->currentSettings.addHorizontalSpace =
            settings.value(QStringLiteral("page/addHorizontalSpace"), this->currentSettings.addHorizontalSpace).toBool();
    this->currentSettings.addHorizontalSpaceAmountRight =
            settings.value(QStringLiteral("page/addHorizontalSpaceAmountRight"),
                           this->currentSettings.addHorizontalSpaceAmountRight)
                    .toInt();
    this->currentSettings.addHorizontalSpaceAmountLeft =
            settings.value(QStringLiteral("page/addHorizontalSpaceAmountLeft"),
                           this->currentSettings.addHorizontalSpaceAmountLeft)
                    .toInt();
    this->currentSettings.addVerticalSpace =
            settings.value(QStringLiteral("page/addVerticalSpace"), this->currentSettings.addVerticalSpace).toBool();
    this->currentSettings.addVerticalSpaceAmountAbove =
            settings.value(QStringLiteral("page/addVerticalSpaceAmountAbove"),
                           this->currentSettings.addVerticalSpaceAmountAbove)
                    .toInt();
    this->currentSettings.addVerticalSpaceAmountBelow =
            settings.value(QStringLiteral("page/addVerticalSpaceAmountBelow"),
                           this->currentSettings.addVerticalSpaceAmountBelow)
                    .toInt();
    this->currentSettings.sizeUnit =
            settings.value(QStringLiteral("page/sizeUnit"), QString::fromStdString(this->currentSettings.sizeUnit))
                    .toString()
                    .toStdString();
    this->currentSettings.undoHistoryLimit =
            settings.value(QStringLiteral("general/undoHistoryLimit"), this->currentSettings.undoHistoryLimit).toInt();
    this->currentSettings.autosaveEnabled =
            settings.value(QStringLiteral("general/autosaveEnabled"), this->currentSettings.autosaveEnabled).toBool();
    this->currentSettings.autosaveTimeoutMinutes =
            settings.value(QStringLiteral("general/autosaveTimeoutMinutes"), this->currentSettings.autosaveTimeoutMinutes)
                    .toInt();
    this->currentSettings.autoloadMostRecent =
            settings.value(QStringLiteral("general/autoloadMostRecent"), this->currentSettings.autoloadMostRecent).toBool();
    this->currentSettings.preferredLocale =
            settings.value(QStringLiteral("general/preferredLocale"),
                           QString::fromStdString(this->currentSettings.preferredLocale))
                    .toString()
                    .toStdString();
    this->currentSettings.automaticUpdateCheckEnabled =
            settings.value(QStringLiteral("general/automaticUpdateCheckEnabled"),
                           this->currentSettings.automaticUpdateCheckEnabled)
                    .toBool();
    this->currentSettings.presentationModeDefault =
            settings.value(QStringLiteral("view/presentationModeDefault"), this->currentSettings.presentationModeDefault)
                    .toBool();
    this->currentSettings.displayDpi =
            settings.value(QStringLiteral("view/displayDpi"), this->currentSettings.displayDpi).toInt();
    this->currentSettings.geometrySnapDefault =
            settings.value(QStringLiteral("general/geometrySnap"), this->currentSettings.geometrySnapDefault).toBool();
    this->currentSettings.gridSnapDefault =
            settings.value(QStringLiteral("general/gridSnap"), this->currentSettings.gridSnapDefault).toBool();
    this->currentSettings.rotationSnapDefault =
            settings.value(QStringLiteral("general/rotationSnap"), this->currentSettings.rotationSnapDefault).toBool();
    this->currentSettings.rotationSnapTolerance =
            settings.value(QStringLiteral("general/rotationSnapTolerance"), this->currentSettings.rotationSnapTolerance)
                    .toDouble();
    this->currentSettings.drawDirModsEnabled =
            settings.value(QStringLiteral("tools/drawDirModsEnabled"), this->currentSettings.drawDirModsEnabled).toBool();
    this->currentSettings.drawDirModsRadius =
            settings.value(QStringLiteral("tools/drawDirModsRadius"), this->currentSettings.drawDirModsRadius).toInt();
    this->currentSettings.zoomStepPercent =
            settings.value(QStringLiteral("view/zoomStepPercent"), this->currentSettings.zoomStepPercent).toDouble();
    this->currentSettings.zoomStepScrollPercent =
            settings.value(QStringLiteral("view/zoomStepScrollPercent"), this->currentSettings.zoomStepScrollPercent)
                    .toDouble();
    this->currentSettings.zoomGesturesEnabled =
            settings.value(QStringLiteral("view/zoomGesturesEnabled"),
                           this->currentSettings.zoomGesturesEnabled)
                    .toBool();
    this->currentSettings.touchZoomStartThreshold =
            settings.value(QStringLiteral("view/touchZoomStartThreshold"),
                           this->currentSettings.touchZoomStartThreshold)
                    .toDouble();
    this->currentSettings.touchInertialScrolling =
            settings.value(QStringLiteral("view/touchInertialScrolling"),
                           this->currentSettings.touchInertialScrolling)
                    .toBool();
    this->currentSettings.unlimitedScrolling =
            settings.value(QStringLiteral("view/unlimitedScrolling"), this->currentSettings.unlimitedScrolling).toBool();
    this->currentSettings.touchDrawingDefault =
            settings.value(QStringLiteral("general/touchDrawing"), this->currentSettings.touchDrawingDefault).toBool();
    this->currentSettings.minimumPressure =
            settings.value(QStringLiteral("tools/minimumPressure"), this->currentSettings.minimumPressure).toDouble();
    this->currentSettings.pressureMultiplier =
            settings.value(QStringLiteral("tools/pressureMultiplier"), this->currentSettings.pressureMultiplier).toDouble();
    this->currentSettings.pressureGuessing =
            settings.value(QStringLiteral("tools/pressureGuessing"), this->currentSettings.pressureGuessing).toBool();
    this->currentSettings.strokeStabilizerEnabled =
            settings.value(QStringLiteral("tools/strokeStabilizerEnabled"),
                           this->currentSettings.strokeStabilizerEnabled)
                    .toBool();
    this->currentSettings.strokeStabilizerSamples =
            settings.value(QStringLiteral("tools/strokeStabilizerSamples"), this->currentSettings.strokeStabilizerSamples)
                    .toInt();
    this->currentSettings.strokeStabilizerStrength =
            settings.value(QStringLiteral("tools/strokeStabilizerStrength"),
                           this->currentSettings.strokeStabilizerStrength)
                    .toDouble();
    this->currentSettings.strokeStabilizerFinalizeStroke =
            settings.value(QStringLiteral("tools/strokeStabilizerFinalizeStroke"),
                           this->currentSettings.strokeStabilizerFinalizeStroke)
                    .toBool();
    this->currentSettings.strokeStabilizerAveragingMethod =
            std::clamp(settings.value(QStringLiteral("tools/strokeStabilizerAveragingMethod"),
                                      this->currentSettings.strokeStabilizerAveragingMethod)
                               .toInt(),
                       0, 2);
    this->currentSettings.strokeStabilizerPreprocessor =
            std::clamp(settings.value(QStringLiteral("tools/strokeStabilizerPreprocessor"),
                                      this->currentSettings.strokeStabilizerPreprocessor)
                               .toInt(),
                       0, 2);
    this->currentSettings.strokeStabilizerSigma =
            settings.value(QStringLiteral("tools/strokeStabilizerSigma"),
                           this->currentSettings.strokeStabilizerSigma)
                    .toDouble();
    this->currentSettings.strokeStabilizerDeadzoneRadius =
            settings.value(QStringLiteral("tools/strokeStabilizerDeadzoneRadius"),
                           this->currentSettings.strokeStabilizerDeadzoneRadius)
                    .toDouble();
    this->currentSettings.strokeStabilizerDrag =
            settings.value(QStringLiteral("tools/strokeStabilizerDrag"),
                           this->currentSettings.strokeStabilizerDrag)
                    .toDouble();
    this->currentSettings.strokeStabilizerMass =
            settings.value(QStringLiteral("tools/strokeStabilizerMass"),
                           this->currentSettings.strokeStabilizerMass)
                    .toDouble();
    this->currentSettings.strokeStabilizerCuspDetection =
            settings.value(QStringLiteral("tools/strokeStabilizerCuspDetection"),
                           this->currentSettings.strokeStabilizerCuspDetection)
                    .toBool();
    this->currentSettings.restoreLineWidthEnabled =
            settings.value(QStringLiteral("tools/restoreLineWidthEnabled"),
                           this->currentSettings.restoreLineWidthEnabled)
                    .toBool();
    this->currentSettings.snapGridTolerance =
            settings.value(QStringLiteral("tools/snapGridTolerance"), this->currentSettings.snapGridTolerance).toDouble();
    this->currentSettings.snapGridSize =
            settings.value(QStringLiteral("tools/snapGridSize"), this->currentSettings.snapGridSize).toDouble();
    this->currentSettings.vertexSnapMarkerSize =
            std::clamp(settings.value(QStringLiteral("tools/vertexSnapMarkerSize"),
                                      this->currentSettings.vertexSnapMarkerSize)
                               .toInt(),
                       8, 48);
    this->currentSettings.geometrySelectionModeDefault =
            settingsGeometrySelectionMode(settings, QStringLiteral("tools/geometrySelectionMode"),
                                          this->currentSettings.geometrySelectionModeDefault);
    this->currentSettings.strokeRecognizerMinSize =
            settings.value(QStringLiteral("general/strokeRecognizerMinSize"), this->currentSettings.strokeRecognizerMinSize)
                    .toDouble();
    this->currentSettings.snapRecognizedShapesEnabled =
            settings.value(QStringLiteral("tools/snapRecognizedShapesEnabled"),
                           this->currentSettings.snapRecognizedShapesEnabled)
                    .toBool();
    this->currentSettings.laserPointerFadeOutMs =
            settings.value(QStringLiteral("general/laserPointerFadeOutMs"), this->currentSettings.laserPointerFadeOutMs)
                    .toInt();
    this->currentSettings.useSpacesForTab =
            settings.value(QStringLiteral("tools/useSpacesForTab"), this->currentSettings.useSpacesForTab).toBool();
    this->currentSettings.numberOfSpacesForTab =
            settings.value(QStringLiteral("tools/numberOfSpacesForTab"), this->currentSettings.numberOfSpacesForTab)
                    .toInt();
    this->currentSettings.edgePanSpeed =
            settings.value(QStringLiteral("view/edgePanSpeed"), this->currentSettings.edgePanSpeed).toDouble();
    this->currentSettings.edgePanMaxMult =
            settings.value(QStringLiteral("view/edgePanMaxMult"), this->currentSettings.edgePanMaxMult).toDouble();
    this->currentSettings.strokeFilterEnabled =
            settings.value(QStringLiteral("tools/strokeFilterEnabled"), this->currentSettings.strokeFilterEnabled).toBool();
    this->currentSettings.strokeFilterIgnoreTime =
            settings.value(QStringLiteral("tools/strokeFilterIgnoreTime"), this->currentSettings.strokeFilterIgnoreTime)
                    .toInt();
    this->currentSettings.strokeFilterIgnoreLength =
            settings.value(QStringLiteral("tools/strokeFilterIgnoreLength"), this->currentSettings.strokeFilterIgnoreLength)
                    .toDouble();
    this->currentSettings.strokeFilterSuccessiveTime =
            settings.value(QStringLiteral("tools/strokeFilterSuccessiveTime"),
                           this->currentSettings.strokeFilterSuccessiveTime)
                    .toInt();
    this->currentSettings.doActionOnStrokeFiltered =
            settings.value(QStringLiteral("tools/doActionOnStrokeFiltered"),
                           this->currentSettings.doActionOnStrokeFiltered)
                    .toBool();
    this->currentSettings.trySelectOnStrokeFiltered =
            settings.value(QStringLiteral("tools/trySelectOnStrokeFiltered"),
                           this->currentSettings.trySelectOnStrokeFiltered)
                    .toBool();
    this->currentSettings.eraserCursorHidden =
            settings.value(QStringLiteral("devices/eraserCursorHidden"), this->currentSettings.eraserCursorHidden).toBool();
    this->currentSettings.ignoredStylusEvents =
            std::clamp(settings.value(QStringLiteral("devices/ignoredStylusEvents"),
                                      this->currentSettings.ignoredStylusEvents)
                               .toInt(),
                       0, 20);
    this->currentSettings.inputSystemTPCButton =
            settings.value(QStringLiteral("devices/inputSystemTPCButton"),
                           this->currentSettings.inputSystemTPCButton)
                    .toBool();
    this->currentSettings.inputSystemDrawOutsideWindow =
            settings.value(QStringLiteral("devices/inputSystemDrawOutsideWindow"),
                           this->currentSettings.inputSystemDrawOutsideWindow)
                    .toBool();
    this->currentSettings.buttonMatrix.eraserTipAction =
            static_cast<QtPointerButtonAction>(
                    settings.value(QStringLiteral("devices/buttonMatrix/eraserTip"),
                                   static_cast<int>(this->currentSettings.buttonMatrix.eraserTipAction))
                            .toInt());
    this->currentSettings.buttonMatrix.stylusButton1Action =
            static_cast<QtPointerButtonAction>(
                    settings.value(QStringLiteral("devices/buttonMatrix/stylusButton1"),
                                   static_cast<int>(this->currentSettings.buttonMatrix.stylusButton1Action))
                            .toInt());
    this->currentSettings.buttonMatrix.stylusButton2Action =
            static_cast<QtPointerButtonAction>(
                    settings.value(QStringLiteral("devices/buttonMatrix/stylusButton2"),
                                   static_cast<int>(this->currentSettings.buttonMatrix.stylusButton2Action))
                            .toInt());
    this->currentSettings.buttonMatrix.mouseLeftAction =
            static_cast<QtPointerButtonAction>(
                    settings.value(QStringLiteral("devices/buttonMatrix/mouseLeft"),
                                   static_cast<int>(this->currentSettings.buttonMatrix.mouseLeftAction))
                            .toInt());
    this->currentSettings.buttonMatrix.mouseMiddleAction =
            static_cast<QtPointerButtonAction>(
                    settings.value(QStringLiteral("devices/buttonMatrix/mouseMiddle"),
                                   settings.value(QStringLiteral("devices/middleButtonAction"),
                                                  static_cast<int>(this->currentSettings.buttonMatrix.mouseMiddleAction)))
                            .toInt());
    this->currentSettings.buttonMatrix.mouseRightAction =
            static_cast<QtPointerButtonAction>(
                    settings.value(QStringLiteral("devices/buttonMatrix/mouseRight"),
                                   settings.value(QStringLiteral("devices/rightButtonAction"),
                                                  static_cast<int>(this->currentSettings.buttonMatrix.mouseRightAction)))
                            .toInt());
    this->currentSettings.buttonMatrix.mouseBackAction =
            static_cast<QtPointerButtonAction>(
                    settings.value(QStringLiteral("devices/buttonMatrix/mouseBack"),
                                   static_cast<int>(this->currentSettings.buttonMatrix.mouseBackAction))
                            .toInt());
    this->currentSettings.buttonMatrix.mouseForwardAction =
            static_cast<QtPointerButtonAction>(
                    settings.value(QStringLiteral("devices/buttonMatrix/mouseForward"),
                                   static_cast<int>(this->currentSettings.buttonMatrix.mouseForwardAction))
                            .toInt());
    this->currentSettings.buttonMatrix.touchAction =
            static_cast<QtPointerButtonAction>(
                    settings.value(QStringLiteral("devices/buttonMatrix/touch"),
                                   static_cast<int>(this->currentSettings.buttonMatrix.touchAction))
                            .toInt());
    this->currentSettings.inputDeviceButtonProfiles.clear();
    const int inputDeviceProfileCount = settings.beginReadArray(QStringLiteral("devices/inputDeviceButtonProfiles"));
    this->currentSettings.inputDeviceButtonProfiles.reserve(static_cast<std::size_t>(inputDeviceProfileCount));
    for (int i = 0; i < inputDeviceProfileCount; ++i) {
        settings.setArrayIndex(i);
        QtInputDeviceButtonProfile profile;
        profile.key = settings.value(QStringLiteral("key")).toString().toStdString();
        profile.displayName = settings.value(QStringLiteral("displayName")).toString().toStdString();
        profile.deviceType = settings.value(QStringLiteral("deviceType")).toString().toStdString();
        profile.customButtonMatrix = settings.value(QStringLiteral("customButtonMatrix"), true).toBool();
        profile.buttonMatrix.eraserTipAction =
                settingsPointerAction(settings, QStringLiteral("eraserTip"),
                                      this->currentSettings.buttonMatrix.eraserTipAction);
        profile.buttonMatrix.stylusButton1Action =
                settingsPointerAction(settings, QStringLiteral("stylusButton1"),
                                      this->currentSettings.buttonMatrix.stylusButton1Action);
        profile.buttonMatrix.stylusButton2Action =
                settingsPointerAction(settings, QStringLiteral("stylusButton2"),
                                      this->currentSettings.buttonMatrix.stylusButton2Action);
        profile.buttonMatrix.mouseLeftAction =
                settingsPointerAction(settings, QStringLiteral("mouseLeft"),
                                      this->currentSettings.buttonMatrix.mouseLeftAction);
        profile.buttonMatrix.mouseMiddleAction =
                settingsPointerAction(settings, QStringLiteral("mouseMiddle"),
                                      this->currentSettings.buttonMatrix.mouseMiddleAction);
        profile.buttonMatrix.mouseRightAction =
                settingsPointerAction(settings, QStringLiteral("mouseRight"),
                                      this->currentSettings.buttonMatrix.mouseRightAction);
        profile.buttonMatrix.mouseBackAction =
                settingsPointerAction(settings, QStringLiteral("mouseBack"),
                                      this->currentSettings.buttonMatrix.mouseBackAction);
        profile.buttonMatrix.mouseForwardAction =
                settingsPointerAction(settings, QStringLiteral("mouseForward"),
                                      this->currentSettings.buttonMatrix.mouseForwardAction);
        profile.buttonMatrix.touchAction =
                settingsPointerAction(settings, QStringLiteral("touch"), this->currentSettings.buttonMatrix.touchAction);
        if (!profile.key.empty()) {
            this->currentSettings.inputDeviceButtonProfiles.push_back(std::move(profile));
        }
    }
    settings.endArray();
    this->currentSettings.showFilePathInTitlebar =
            settings.value(QStringLiteral("appearance/showFilePathInTitlebar"),
                           this->currentSettings.showFilePathInTitlebar)
                    .toBool();
    this->currentSettings.showPageNumberInTitlebar =
            settings.value(QStringLiteral("appearance/showPageNumberInTitlebar"),
                           this->currentSettings.showPageNumberInTitlebar)
                    .toBool();
    this->currentSettings.showPageShadow =
            settings.value(QStringLiteral("appearance/showPageShadow"), this->currentSettings.showPageShadow).toBool();
    this->currentSettings.sidebarWidth =
            std::clamp(settings.value(QStringLiteral("appearance/sidebarWidth"), this->currentSettings.sidebarWidth)
                               .toInt(),
                       76, 600);
    this->currentSettings.sidebarOnRight =
            settings.value(QStringLiteral("appearance/sidebarOnRight"), this->currentSettings.sidebarOnRight).toBool();
    this->currentSettings.scrollbarOnLeft =
            settings.value(QStringLiteral("appearance/scrollbarOnLeft"), this->currentSettings.scrollbarOnLeft).toBool();
    this->currentSettings.sidebarNumberingStyle =
            std::clamp(settings.value(QStringLiteral("appearance/sidebarNumberingStyle"),
                                      this->currentSettings.sidebarNumberingStyle)
                               .toInt(),
                       0, 3);
    this->currentSettings.scrollbarHideType =
            settings.value(QStringLiteral("appearance/scrollbarHideType"), this->currentSettings.scrollbarHideType).toInt() & 6;
    this->currentSettings.disableScrollbarFadeout =
            settings.value(QStringLiteral("appearance/disableScrollbarFadeout"),
                           this->currentSettings.disableScrollbarFadeout)
                    .toBool();
    this->currentSettings.themeVariant =
            settings.value(QStringLiteral("appearance/themeVariant"), QString::fromStdString(this->currentSettings.themeVariant))
                    .toString()
                    .toStdString();
    this->currentSettings.iconTheme =
            settings.value(QStringLiteral("appearance/iconTheme"), QString::fromStdString(this->currentSettings.iconTheme))
                    .toString()
                    .toStdString();
    this->currentSettings.selectionColor =
            Color(settings.value(QStringLiteral("appearance/selectionColor"),
                                 static_cast<uint>(static_cast<uint32_t>(this->currentSettings.selectionColor)))
                          .toUInt());
    this->currentSettings.backgroundColor =
            Color(settings.value(QStringLiteral("appearance/backgroundColor"),
                                 static_cast<uint>(static_cast<uint32_t>(this->currentSettings.backgroundColor)))
                          .toUInt());
    this->currentSettings.highlightPosition =
            settings.value(QStringLiteral("appearance/highlightPosition"),
                           this->currentSettings.highlightPosition)
                    .toBool();
    this->currentSettings.cursorHighlightColor =
            Color(settings.value(QStringLiteral("appearance/cursorHighlightColor"),
                                 static_cast<uint>(static_cast<uint32_t>(this->currentSettings.cursorHighlightColor)))
                          .toUInt());
    this->currentSettings.cursorHighlightBorderColor =
            Color(settings.value(QStringLiteral("appearance/cursorHighlightBorderColor"),
                                 static_cast<uint>(
                                         static_cast<uint32_t>(this->currentSettings.cursorHighlightBorderColor)))
                          .toUInt());
    this->currentSettings.cursorHighlightRadius =
            settings.value(QStringLiteral("appearance/cursorHighlightRadius"),
                           this->currentSettings.cursorHighlightRadius)
                    .toInt();
    this->currentSettings.cursorHighlightBorderWidth =
            settings.value(QStringLiteral("appearance/cursorHighlightBorderWidth"),
                           this->currentSettings.cursorHighlightBorderWidth)
                    .toInt();
    this->currentSettings.recolorMainView =
            settings.value(QStringLiteral("appearance/recolorMainView"), this->currentSettings.recolorMainView).toBool();
    this->currentSettings.recolorSidebarMiniatures =
            settings.value(QStringLiteral("appearance/recolorSidebarMiniatures"),
                           this->currentSettings.recolorSidebarMiniatures)
                    .toBool();
    this->currentSettings.recolorLight =
            Color(settings.value(QStringLiteral("appearance/recolorLight"),
                                 static_cast<uint>(static_cast<uint32_t>(this->currentSettings.recolorLight)))
                          .toUInt());
    this->currentSettings.recolorDark =
            Color(settings.value(QStringLiteral("appearance/recolorDark"),
                                 static_cast<uint>(static_cast<uint32_t>(this->currentSettings.recolorDark)))
                          .toUInt());
    this->currentSettings.colorPalettePath =
            settings.value(QStringLiteral("appearance/colorPalettePath"),
                           QString::fromStdString(this->currentSettings.colorPalettePath))
                    .toString()
                    .toStdString();
    this->currentSettings.autoloadPdfXoj =
            settings.value(QStringLiteral("pdf/autoloadPdfXoj"), this->currentSettings.autoloadPdfXoj).toBool();
    this->currentSettings.defaultPdfExportName =
            settings.value(QStringLiteral("pdf/defaultExportName"),
                           QString::fromStdString(this->currentSettings.defaultPdfExportName))
                    .toString()
                    .toStdString();
    this->currentSettings.pdfPageCacheSize =
            settings.value(QStringLiteral("pdf/pageCacheSize"), this->currentSettings.pdfPageCacheSize).toInt();
    this->currentSettings.pdfPreloadPagesBefore =
            settings.value(QStringLiteral("pdf/preloadPagesBefore"), this->currentSettings.pdfPreloadPagesBefore).toInt();
    this->currentSettings.pdfPreloadPagesAfter =
            settings.value(QStringLiteral("pdf/preloadPagesAfter"), this->currentSettings.pdfPreloadPagesAfter).toInt();
    this->currentSettings.pdfEagerPageCleanup =
            settings.value(QStringLiteral("pdf/eagerPageCleanup"), this->currentSettings.pdfEagerPageCleanup).toBool();
    this->currentSettings.pdfPageRerenderThreshold =
            settings.value(QStringLiteral("pdf/pageRerenderThreshold"),
                           this->currentSettings.pdfPageRerenderThreshold)
                    .toDouble();
    this->currentSettings.emptyLastPageAppend =
            settings.value(QStringLiteral("page/emptyLastPageAppend"),
                           QString::fromStdString(this->currentSettings.emptyLastPageAppend))
                    .toString()
                    .toStdString();
    this->currentSettings.latexTemplatePath =
            settings.value(QStringLiteral("latex/templatePath"), QString::fromStdString(this->currentSettings.latexTemplatePath))
                    .toString()
                    .toStdString();
    this->currentSettings.latexAutoCheckDependencies =
            settings.value(QStringLiteral("latex/autoCheckDependencies"),
                           this->currentSettings.latexAutoCheckDependencies)
                    .toBool();
    this->currentSettings.latexDefaultText =
            settings.value(QStringLiteral("latex/defaultText"), QString::fromStdString(this->currentSettings.latexDefaultText))
                    .toString()
                    .toStdString();
    this->currentSettings.latexGenCmd =
            settings.value(QStringLiteral("latex/genCmd"), QString::fromStdString(this->currentSettings.latexGenCmd))
                    .toString()
                    .toStdString();
    this->currentSettings.latexSourceViewThemeId =
            settings.value(QStringLiteral("latex/sourceViewThemeId"),
                           QString::fromStdString(this->currentSettings.latexSourceViewThemeId))
                    .toString()
                    .toStdString();
    this->currentSettings.latexSourceViewAutoIndent =
            settings.value(QStringLiteral("latex/sourceViewAutoIndent"),
                           this->currentSettings.latexSourceViewAutoIndent)
                    .toBool();
    this->currentSettings.latexSourceViewSyntaxHighlight =
            settings.value(QStringLiteral("latex/sourceViewSyntaxHighlight"),
                           this->currentSettings.latexSourceViewSyntaxHighlight)
                    .toBool();
    this->currentSettings.latexSourceViewShowLineNumbers =
            settings.value(QStringLiteral("latex/sourceViewShowLineNumbers"),
                           this->currentSettings.latexSourceViewShowLineNumbers)
                    .toBool();
    this->currentSettings.latexEditorFont =
            settings.value(QStringLiteral("latex/editorFont"), QString::fromStdString(this->currentSettings.latexEditorFont))
                    .toString()
                    .toStdString();
    this->currentSettings.latexUseCustomEditorFont =
            settings.value(QStringLiteral("latex/useCustomEditorFont"),
                           this->currentSettings.latexUseCustomEditorFont)
                    .toBool();
    this->currentSettings.latexEditorWordWrap =
            settings.value(QStringLiteral("latex/editorWordWrap"), this->currentSettings.latexEditorWordWrap).toBool();
    this->currentSettings.latexUseExternalEditor =
            settings.value(QStringLiteral("latex/useExternalEditor"),
                           this->currentSettings.latexUseExternalEditor)
                    .toBool();
    this->currentSettings.latexExternalEditorAutoConfirm =
            settings.value(QStringLiteral("latex/externalEditorAutoConfirm"),
                           this->currentSettings.latexExternalEditorAutoConfirm)
                    .toBool();
    this->currentSettings.latexExternalEditorCmd =
            settings.value(QStringLiteral("latex/externalEditorCmd"),
                           QString::fromStdString(this->currentSettings.latexExternalEditorCmd))
                    .toString()
                    .toStdString();
    this->currentSettings.latexTemporaryFileExt =
            settings.value(QStringLiteral("latex/temporaryFileExt"),
                           QString::fromStdString(this->currentSettings.latexTemporaryFileExt))
                    .toString()
                    .toStdString();
    this->currentSettings.audioFolder =
            settings.value(QStringLiteral("audio/folder"), QString::fromStdString(this->currentSettings.audioFolder))
                    .toString()
                    .toStdString();
    this->currentSettings.lastOpenPath =
            settings.value(QStringLiteral("paths/lastOpen"), QString::fromStdString(this->currentSettings.lastOpenPath))
                    .toString()
                    .toStdString();
    this->currentSettings.lastSavePath =
            settings.value(QStringLiteral("paths/lastSave"), QString::fromStdString(this->currentSettings.lastSavePath))
                    .toString()
                    .toStdString();
    this->currentSettings.lastImagePath =
            settings.value(QStringLiteral("paths/lastImage"), QString::fromStdString(this->currentSettings.lastImagePath))
                    .toString()
                    .toStdString();
    this->currentSettings.lastPdfPath =
            settings.value(QStringLiteral("paths/lastPdf"), QString::fromStdString(this->currentSettings.lastPdfPath))
                    .toString()
                    .toStdString();
    this->currentSettings.lastExportPath =
            settings.value(QStringLiteral("paths/lastExport"), QString::fromStdString(this->currentSettings.lastExportPath))
                    .toString()
                    .toStdString();
    this->currentSettings.audioSampleRate =
            settings.value(QStringLiteral("audio/sampleRate"), this->currentSettings.audioSampleRate).toDouble();
    this->currentSettings.audioGain =
            settings.value(QStringLiteral("audio/gain"), this->currentSettings.audioGain).toDouble();
    this->currentSettings.defaultSeekTimeSeconds =
            settings.value(QStringLiteral("audio/defaultSeekTimeSeconds"), this->currentSettings.defaultSeekTimeSeconds)
                    .toInt();
    this->currentSettings.disableAudio =
            settings.value(QStringLiteral("audio/disabled"), this->currentSettings.disableAudio).toBool();
    this->currentSettings.audioInputDevice =
            settings.value(QStringLiteral("audio/inputDevice"), this->currentSettings.audioInputDevice).toInt();
    this->currentSettings.audioOutputDevice =
            settings.value(QStringLiteral("audio/outputDevice"), this->currentSettings.audioOutputDevice).toInt();
    this->currentSettings.toolbarProfileId = settings.value(
                                                     QStringLiteral("general/toolbarProfileId"),
                                                     QString::fromStdString(this->currentSettings.toolbarProfileId))
                                                     .toString()
                                                     .toStdString();
    if (this->currentSettings.toolbarProfileId.empty()) {
        this->currentSettings.toolbarProfileId = QT_GTK_PARITY_PROFILE_ID;
    }
    this->persistedShowToolbar = settings.value(QStringLiteral("view/showToolbar"), true).toBool();
    this->persistedShowMenubar = settings.value(QStringLiteral("view/showMenubar"), true).toBool();
    this->persistedShowSidebar = settings.value(QStringLiteral("view/showSidebar"), true).toBool();
    this->persistedPairedPages = settings.value(QStringLiteral("view/pairedPages"), false).toBool();
    this->persistedPairOffset = settings.value(QStringLiteral("view/pairOffset"), 0).toInt();
    if (this->persistedPairOffset < 0 || this->persistedPairOffset > 1) {
        this->persistedPairOffset = 0;
    }
    this->persistedLayoutColumnsRows =
            settings.value(QStringLiteral("view/layoutColumnsRows"), this->persistedPairedPages ? 2 : 1).toInt();
    if (this->persistedLayoutColumnsRows == 0 || std::abs(this->persistedLayoutColumnsRows) > 8) {
        this->persistedLayoutColumnsRows = this->persistedPairedPages ? 2 : 1;
    }
    this->persistedVerticalLayout = settings.value(QStringLiteral("view/verticalLayout"), true).toBool();
    this->persistedLayoutRtl = settings.value(QStringLiteral("view/layoutRtl"), false).toBool();
    this->persistedLayoutBtt = settings.value(QStringLiteral("view/layoutBtt"), false).toBool();

    std::vector<std::filesystem::path> recentPaths;
    const auto recentEntries = settings.value(QStringLiteral("recentDocuments/files")).toStringList();
    recentPaths.reserve(static_cast<std::size_t>(recentEntries.size()));
    for (const auto& entry: recentEntries) {
        if (!entry.trimmed().isEmpty()) {
            recentPaths.emplace_back(entry.toStdString());
        }
    }
    this->recentFiles.setRecentFiles(recentPaths);
    this->persistedWindowGeometry = settings.value(QStringLiteral("window/geometry")).toByteArray();
    this->persistedWindowState = settings.value(QStringLiteral("window/state")).toByteArray();
    this->persistedFloatingToolBarGeometries.clear();
    this->persistedFloatingToolBarUserHidden.clear();
    for (std::size_t index = 0; index < this->window.floatingToolBars().size(); ++index) {
        this->persistedFloatingToolBarGeometries.push_back(
                settings.value(QStringLiteral("window/floatingToolbar%1Geometry").arg(index)).toByteArray());
        this->persistedFloatingToolBarUserHidden.push_back(
                settings.value(QStringLiteral("window/floatingToolbar%1UserHidden").arg(index), false).toBool());
    }

    if (savedLayoutVersion < QT_SHELL_LAYOUT_VERSION) {
        // Rebase every older Qt shell layout onto the GTK-like portrait profile once.
        // This clears old left/right/floating toolbar state that made startup diverge
        // from the target shell composition.
        this->currentSettings.toolbarProfileId = QT_GTK_PARITY_PROFILE_ID;
        this->persistedWindowState.clear();
        this->persistedFloatingToolBarGeometries.clear();
        this->persistedFloatingToolBarUserHidden.clear();
        this->persistedShowToolbar = true;
        this->persistedShowMenubar = true;
        this->persistedShowSidebar = true;
        this->persistedPairedPages = false;
        this->persistedPairOffset = 0;
        this->persistedLayoutColumnsRows = 1;
        this->persistedVerticalLayout = true;
        this->persistedLayoutRtl = false;
        this->persistedLayoutBtt = false;
    }

    if (this->currentSettings.audioFolder.empty()) {
        this->currentSettings.audioFolder = Util::getDataSubfolder("audio").string();
    }
    applyQtPreferredLocale(this->currentSettings.preferredLocale);
}
