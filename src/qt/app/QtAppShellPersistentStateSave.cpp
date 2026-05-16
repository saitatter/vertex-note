/*
 * VertexNote
 *
 * Qt app shell persisted UI state saving.
 */

#include "QtAppShell.h"

#include <cstdint>
#include <string>

#include <QSettings>
#include <QString>
#include <QStringList>
#include <QToolBar>

namespace {

constexpr int QT_SHELL_LAYOUT_VERSION = 6;

}  // namespace
void QtAppShell::savePersistentUiState() const {
    QSettings settings(QStringLiteral("VertexNote"), QStringLiteral("VertexNoteQtShell"));
    const auto* commandHost = this->window.commandHost();
    const auto* canvas = this->window.canvas();
    const bool showToolbar =
            commandHost->actionForCommand("view.show-toolbar")
                    ? commandHost->actionForCommand("view.show-toolbar")->isChecked()
                    : this->persistedShowToolbar;
    const bool showMenubar =
            commandHost->actionForCommand("view.show-menubar")
                    ? commandHost->actionForCommand("view.show-menubar")->isChecked()
                    : this->persistedShowMenubar;
    const bool showSidebar =
            commandHost->actionForCommand("view.show-sidebar")
                    ? commandHost->actionForCommand("view.show-sidebar")->isChecked()
                    : this->persistedShowSidebar;
    const bool showGeometryPanel =
            commandHost->actionForCommand("view.show-geometry-panel")
                    ? commandHost->actionForCommand("view.show-geometry-panel")->isChecked()
                    : this->persistedShowGeometryPanel;

    settings.setValue(QStringLiteral("tools/defaultPenWidth"), this->currentSettings.defaultPenWidth);
    settings.setValue(QStringLiteral("tools/defaultHighlighterWidth"), this->currentSettings.defaultHighlighterWidth);
    settings.setValue(QStringLiteral("tools/defaultEraserWidth"), this->currentSettings.defaultEraserWidth);
    settings.setValue(QStringLiteral("tools/defaultPressureSensitive"), this->currentSettings.defaultPressureSensitive);
    settings.setValue(QStringLiteral("tools/defaultEraserMode"), static_cast<int>(this->currentSettings.defaultEraserMode));
    settings.setValue(QStringLiteral("tools/minimumPressure"), this->currentSettings.minimumPressure);
    settings.setValue(QStringLiteral("tools/pressureMultiplier"), this->currentSettings.pressureMultiplier);
    settings.setValue(QStringLiteral("tools/pressureGuessing"), this->currentSettings.pressureGuessing);
    settings.setValue(QStringLiteral("tools/strokeStabilizerEnabled"), this->currentSettings.strokeStabilizerEnabled);
    settings.setValue(QStringLiteral("tools/strokeStabilizerSamples"), this->currentSettings.strokeStabilizerSamples);
    settings.setValue(QStringLiteral("tools/strokeStabilizerStrength"), this->currentSettings.strokeStabilizerStrength);
    settings.setValue(QStringLiteral("tools/strokeStabilizerFinalizeStroke"),
                      this->currentSettings.strokeStabilizerFinalizeStroke);
    settings.setValue(QStringLiteral("tools/strokeStabilizerAveragingMethod"),
                      this->currentSettings.strokeStabilizerAveragingMethod);
    settings.setValue(QStringLiteral("tools/strokeStabilizerPreprocessor"),
                      this->currentSettings.strokeStabilizerPreprocessor);
    settings.setValue(QStringLiteral("tools/strokeStabilizerSigma"), this->currentSettings.strokeStabilizerSigma);
    settings.setValue(QStringLiteral("tools/strokeStabilizerDeadzoneRadius"),
                      this->currentSettings.strokeStabilizerDeadzoneRadius);
    settings.setValue(QStringLiteral("tools/strokeStabilizerDrag"), this->currentSettings.strokeStabilizerDrag);
    settings.setValue(QStringLiteral("tools/strokeStabilizerMass"), this->currentSettings.strokeStabilizerMass);
    settings.setValue(QStringLiteral("tools/strokeStabilizerCuspDetection"),
                      this->currentSettings.strokeStabilizerCuspDetection);
    settings.setValue(QStringLiteral("tools/restoreLineWidthEnabled"), this->currentSettings.restoreLineWidthEnabled);
    settings.setValue(QStringLiteral("tools/snapGridTolerance"), this->currentSettings.snapGridTolerance);
    settings.setValue(QStringLiteral("tools/snapGridSize"), this->currentSettings.snapGridSize);
    settings.setValue(QStringLiteral("tools/vertexSnapMarkerSize"), this->currentSettings.vertexSnapMarkerSize);
    settings.setValue(QStringLiteral("tools/geometrySelectionMode"),
                      static_cast<int>(canvas->toolState().geometrySelectionMode));
    settings.setValue(QStringLiteral("page/defaultWidth"), this->currentSettings.defaultPageWidth);
    settings.setValue(QStringLiteral("page/defaultHeight"), this->currentSettings.defaultPageHeight);
    settings.setValue(QStringLiteral("page/sizeUnit"), QString::fromStdString(this->currentSettings.sizeUnit));
    settings.setValue(QStringLiteral("tools/defaultFontName"),
                      QString::fromStdString(this->currentSettings.defaultFontName));
    settings.setValue(QStringLiteral("tools/defaultFontSize"), this->currentSettings.defaultFontSize);
    settings.setValue(QStringLiteral("page/addHorizontalSpace"), this->currentSettings.addHorizontalSpace);
    settings.setValue(QStringLiteral("page/addHorizontalSpaceAmountRight"),
                      this->currentSettings.addHorizontalSpaceAmountRight);
    settings.setValue(QStringLiteral("page/addHorizontalSpaceAmountLeft"),
                      this->currentSettings.addHorizontalSpaceAmountLeft);
    settings.setValue(QStringLiteral("page/addVerticalSpace"), this->currentSettings.addVerticalSpace);
    settings.setValue(QStringLiteral("page/addVerticalSpaceAmountAbove"),
                      this->currentSettings.addVerticalSpaceAmountAbove);
    settings.setValue(QStringLiteral("page/addVerticalSpaceAmountBelow"),
                      this->currentSettings.addVerticalSpaceAmountBelow);
    settings.setValue(QStringLiteral("general/undoHistoryLimit"), this->currentSettings.undoHistoryLimit);
    settings.setValue(QStringLiteral("general/autosaveEnabled"), this->currentSettings.autosaveEnabled);
    settings.setValue(QStringLiteral("general/autosaveTimeoutMinutes"), this->currentSettings.autosaveTimeoutMinutes);
    settings.setValue(QStringLiteral("general/autoloadMostRecent"), this->currentSettings.autoloadMostRecent);
    settings.setValue(QStringLiteral("general/preferredLocale"),
                      QString::fromStdString(this->currentSettings.preferredLocale));
    settings.setValue(QStringLiteral("general/automaticUpdateCheckEnabled"),
                      this->currentSettings.automaticUpdateCheckEnabled);
    settings.setValue(QStringLiteral("view/presentationModeDefault"), this->currentSettings.presentationModeDefault);
    settings.setValue(QStringLiteral("view/displayDpi"), this->currentSettings.displayDpi);
    settings.setValue(QStringLiteral("general/geometrySnap"), this->currentSettings.geometrySnapDefault);
    settings.setValue(QStringLiteral("general/gridSnap"), this->currentSettings.gridSnapDefault);
    settings.setValue(QStringLiteral("general/rotationSnap"), this->currentSettings.rotationSnapDefault);
    settings.setValue(QStringLiteral("general/rotationSnapTolerance"), this->currentSettings.rotationSnapTolerance);
    settings.setValue(QStringLiteral("tools/drawDirModsEnabled"), this->currentSettings.drawDirModsEnabled);
    settings.setValue(QStringLiteral("tools/drawDirModsRadius"), this->currentSettings.drawDirModsRadius);
    settings.setValue(QStringLiteral("view/zoomStepPercent"), this->currentSettings.zoomStepPercent);
    settings.setValue(QStringLiteral("view/zoomStepScrollPercent"), this->currentSettings.zoomStepScrollPercent);
    settings.setValue(QStringLiteral("view/zoomGesturesEnabled"), this->currentSettings.zoomGesturesEnabled);
    settings.setValue(QStringLiteral("view/touchZoomStartThreshold"), this->currentSettings.touchZoomStartThreshold);
    settings.setValue(QStringLiteral("view/touchInertialScrolling"), this->currentSettings.touchInertialScrolling);
    settings.setValue(QStringLiteral("view/unlimitedScrolling"), this->currentSettings.unlimitedScrolling);
    settings.setValue(QStringLiteral("general/touchDrawing"), this->currentSettings.touchDrawingDefault);
    settings.setValue(QStringLiteral("general/strokeRecognizerMinSize"), this->currentSettings.strokeRecognizerMinSize);
    settings.setValue(QStringLiteral("tools/snapRecognizedShapesEnabled"),
                      this->currentSettings.snapRecognizedShapesEnabled);
    settings.setValue(QStringLiteral("general/laserPointerFadeOutMs"), this->currentSettings.laserPointerFadeOutMs);
    settings.setValue(QStringLiteral("tools/useSpacesForTab"), this->currentSettings.useSpacesForTab);
    settings.setValue(QStringLiteral("tools/numberOfSpacesForTab"), this->currentSettings.numberOfSpacesForTab);
    settings.setValue(QStringLiteral("view/edgePanSpeed"), this->currentSettings.edgePanSpeed);
    settings.setValue(QStringLiteral("view/edgePanMaxMult"), this->currentSettings.edgePanMaxMult);
    settings.setValue(QStringLiteral("tools/strokeFilterEnabled"), this->currentSettings.strokeFilterEnabled);
    settings.setValue(QStringLiteral("tools/strokeFilterIgnoreTime"), this->currentSettings.strokeFilterIgnoreTime);
    settings.setValue(QStringLiteral("tools/strokeFilterIgnoreLength"), this->currentSettings.strokeFilterIgnoreLength);
    settings.setValue(QStringLiteral("tools/strokeFilterSuccessiveTime"),
                      this->currentSettings.strokeFilterSuccessiveTime);
    settings.setValue(QStringLiteral("tools/doActionOnStrokeFiltered"),
                      this->currentSettings.doActionOnStrokeFiltered);
    settings.setValue(QStringLiteral("tools/trySelectOnStrokeFiltered"),
                      this->currentSettings.trySelectOnStrokeFiltered);
    settings.setValue(QStringLiteral("devices/eraserCursorHidden"), this->currentSettings.eraserCursorHidden);
    settings.setValue(QStringLiteral("devices/ignoredStylusEvents"), this->currentSettings.ignoredStylusEvents);
    settings.setValue(QStringLiteral("devices/inputSystemTPCButton"), this->currentSettings.inputSystemTPCButton);
    settings.setValue(QStringLiteral("devices/inputSystemDrawOutsideWindow"),
                      this->currentSettings.inputSystemDrawOutsideWindow);
    settings.setValue(QStringLiteral("devices/buttonMatrix/eraserTip"),
                      static_cast<int>(this->currentSettings.buttonMatrix.eraserTipAction));
    settings.setValue(QStringLiteral("devices/buttonMatrix/stylusButton1"),
                      static_cast<int>(this->currentSettings.buttonMatrix.stylusButton1Action));
    settings.setValue(QStringLiteral("devices/buttonMatrix/stylusButton2"),
                      static_cast<int>(this->currentSettings.buttonMatrix.stylusButton2Action));
    settings.setValue(QStringLiteral("devices/buttonMatrix/mouseLeft"),
                      static_cast<int>(this->currentSettings.buttonMatrix.mouseLeftAction));
    settings.setValue(QStringLiteral("devices/buttonMatrix/mouseMiddle"),
                      static_cast<int>(this->currentSettings.buttonMatrix.mouseMiddleAction));
    settings.setValue(QStringLiteral("devices/buttonMatrix/mouseRight"),
                      static_cast<int>(this->currentSettings.buttonMatrix.mouseRightAction));
    settings.setValue(QStringLiteral("devices/buttonMatrix/mouseBack"),
                      static_cast<int>(this->currentSettings.buttonMatrix.mouseBackAction));
    settings.setValue(QStringLiteral("devices/buttonMatrix/mouseForward"),
                      static_cast<int>(this->currentSettings.buttonMatrix.mouseForwardAction));
    settings.setValue(QStringLiteral("devices/buttonMatrix/touch"),
                      static_cast<int>(this->currentSettings.buttonMatrix.touchAction));
    settings.beginWriteArray(QStringLiteral("devices/inputDeviceButtonProfiles"),
                             static_cast<int>(this->currentSettings.inputDeviceButtonProfiles.size()));
    for (int i = 0; i < static_cast<int>(this->currentSettings.inputDeviceButtonProfiles.size()); ++i) {
        const auto& profile = this->currentSettings.inputDeviceButtonProfiles[static_cast<std::size_t>(i)];
        settings.setArrayIndex(i);
        settings.setValue(QStringLiteral("key"), QString::fromStdString(profile.key));
        settings.setValue(QStringLiteral("displayName"), QString::fromStdString(profile.displayName));
        settings.setValue(QStringLiteral("deviceType"), QString::fromStdString(profile.deviceType));
        settings.setValue(QStringLiteral("customButtonMatrix"), profile.customButtonMatrix);
        settings.setValue(QStringLiteral("eraserTip"), static_cast<int>(profile.buttonMatrix.eraserTipAction));
        settings.setValue(QStringLiteral("stylusButton1"), static_cast<int>(profile.buttonMatrix.stylusButton1Action));
        settings.setValue(QStringLiteral("stylusButton2"), static_cast<int>(profile.buttonMatrix.stylusButton2Action));
        settings.setValue(QStringLiteral("mouseLeft"), static_cast<int>(profile.buttonMatrix.mouseLeftAction));
        settings.setValue(QStringLiteral("mouseMiddle"), static_cast<int>(profile.buttonMatrix.mouseMiddleAction));
        settings.setValue(QStringLiteral("mouseRight"), static_cast<int>(profile.buttonMatrix.mouseRightAction));
        settings.setValue(QStringLiteral("mouseBack"), static_cast<int>(profile.buttonMatrix.mouseBackAction));
        settings.setValue(QStringLiteral("mouseForward"), static_cast<int>(profile.buttonMatrix.mouseForwardAction));
        settings.setValue(QStringLiteral("touch"), static_cast<int>(profile.buttonMatrix.touchAction));
    }
    settings.endArray();
    settings.setValue(QStringLiteral("appearance/showFilePathInTitlebar"),
                      this->currentSettings.showFilePathInTitlebar);
    settings.setValue(QStringLiteral("appearance/showPageNumberInTitlebar"),
                      this->currentSettings.showPageNumberInTitlebar);
    settings.setValue(QStringLiteral("appearance/showPageShadow"), this->currentSettings.showPageShadow);
    settings.setValue(QStringLiteral("appearance/geometryWireframeView"),
                      this->currentSettings.geometryWireframeView);
    settings.setValue(QStringLiteral("appearance/geometryHighlightVertices"),
                      this->currentSettings.geometryHighlightVertices);
    settings.setValue(QStringLiteral("appearance/geometryHighlightLinkedVertices"),
                      this->currentSettings.geometryHighlightLinkedVertices);
    settings.setValue(QStringLiteral("appearance/geometryShowFaceFills"),
                      this->currentSettings.geometryShowFaceFills);
    settings.setValue(QStringLiteral("appearance/sidebarWidth"), this->currentSettings.sidebarWidth);
    settings.setValue(QStringLiteral("appearance/sidebarOnRight"), this->currentSettings.sidebarOnRight);
    settings.setValue(QStringLiteral("appearance/scrollbarOnLeft"), this->currentSettings.scrollbarOnLeft);
    settings.setValue(QStringLiteral("appearance/sidebarNumberingStyle"), this->currentSettings.sidebarNumberingStyle);
    settings.setValue(QStringLiteral("appearance/scrollbarHideType"), this->currentSettings.scrollbarHideType);
    settings.setValue(QStringLiteral("appearance/disableScrollbarFadeout"),
                      this->currentSettings.disableScrollbarFadeout);
    settings.setValue(QStringLiteral("appearance/themeVariant"), QString::fromStdString(this->currentSettings.themeVariant));
    settings.setValue(QStringLiteral("appearance/iconTheme"), QString::fromStdString(this->currentSettings.iconTheme));
    settings.setValue(QStringLiteral("appearance/selectionColor"),
                      static_cast<uint>(static_cast<uint32_t>(this->currentSettings.selectionColor)));
    settings.setValue(QStringLiteral("appearance/backgroundColor"),
                      static_cast<uint>(static_cast<uint32_t>(this->currentSettings.backgroundColor)));
    settings.setValue(QStringLiteral("appearance/highlightPosition"), this->currentSettings.highlightPosition);
    settings.setValue(QStringLiteral("appearance/cursorHighlightColor"),
                      static_cast<uint>(static_cast<uint32_t>(this->currentSettings.cursorHighlightColor)));
    settings.setValue(QStringLiteral("appearance/cursorHighlightBorderColor"),
                      static_cast<uint>(static_cast<uint32_t>(this->currentSettings.cursorHighlightBorderColor)));
    settings.setValue(QStringLiteral("appearance/cursorHighlightRadius"),
                      this->currentSettings.cursorHighlightRadius);
    settings.setValue(QStringLiteral("appearance/cursorHighlightBorderWidth"),
                      this->currentSettings.cursorHighlightBorderWidth);
    settings.setValue(QStringLiteral("appearance/recolorMainView"), this->currentSettings.recolorMainView);
    settings.setValue(QStringLiteral("appearance/recolorSidebarMiniatures"),
                      this->currentSettings.recolorSidebarMiniatures);
    settings.setValue(QStringLiteral("appearance/recolorLight"),
                      static_cast<uint>(static_cast<uint32_t>(this->currentSettings.recolorLight)));
    settings.setValue(QStringLiteral("appearance/recolorDark"),
                      static_cast<uint>(static_cast<uint32_t>(this->currentSettings.recolorDark)));
    settings.setValue(QStringLiteral("appearance/colorPalettePath"),
                      QString::fromStdString(this->currentSettings.colorPalettePath));
    settings.setValue(QStringLiteral("pdf/autoloadPdfXoj"), this->currentSettings.autoloadPdfXoj);
    settings.setValue(QStringLiteral("pdf/defaultExportName"), QString::fromStdString(this->currentSettings.defaultPdfExportName));
    settings.setValue(QStringLiteral("pdf/pageCacheSize"), this->currentSettings.pdfPageCacheSize);
    settings.setValue(QStringLiteral("pdf/preloadPagesBefore"), this->currentSettings.pdfPreloadPagesBefore);
    settings.setValue(QStringLiteral("pdf/preloadPagesAfter"), this->currentSettings.pdfPreloadPagesAfter);
    settings.setValue(QStringLiteral("pdf/eagerPageCleanup"), this->currentSettings.pdfEagerPageCleanup);
    settings.setValue(QStringLiteral("pdf/pageRerenderThreshold"),
                      this->currentSettings.pdfPageRerenderThreshold);
    settings.setValue(QStringLiteral("page/emptyLastPageAppend"),
                      QString::fromStdString(this->currentSettings.emptyLastPageAppend));
    settings.setValue(QStringLiteral("latex/templatePath"), QString::fromStdString(this->currentSettings.latexTemplatePath));
    settings.setValue(QStringLiteral("latex/autoCheckDependencies"), this->currentSettings.latexAutoCheckDependencies);
    settings.setValue(QStringLiteral("latex/defaultText"), QString::fromStdString(this->currentSettings.latexDefaultText));
    settings.setValue(QStringLiteral("latex/genCmd"), QString::fromStdString(this->currentSettings.latexGenCmd));
    settings.setValue(QStringLiteral("latex/sourceViewThemeId"),
                      QString::fromStdString(this->currentSettings.latexSourceViewThemeId));
    settings.setValue(QStringLiteral("latex/sourceViewAutoIndent"), this->currentSettings.latexSourceViewAutoIndent);
    settings.setValue(QStringLiteral("latex/sourceViewSyntaxHighlight"),
                      this->currentSettings.latexSourceViewSyntaxHighlight);
    settings.setValue(QStringLiteral("latex/sourceViewShowLineNumbers"),
                      this->currentSettings.latexSourceViewShowLineNumbers);
    settings.setValue(QStringLiteral("latex/editorFont"), QString::fromStdString(this->currentSettings.latexEditorFont));
    settings.setValue(QStringLiteral("latex/useCustomEditorFont"), this->currentSettings.latexUseCustomEditorFont);
    settings.setValue(QStringLiteral("latex/editorWordWrap"), this->currentSettings.latexEditorWordWrap);
    settings.setValue(QStringLiteral("latex/useExternalEditor"), this->currentSettings.latexUseExternalEditor);
    settings.setValue(QStringLiteral("latex/externalEditorAutoConfirm"),
                      this->currentSettings.latexExternalEditorAutoConfirm);
    settings.setValue(QStringLiteral("latex/externalEditorCmd"),
                      QString::fromStdString(this->currentSettings.latexExternalEditorCmd));
    settings.setValue(QStringLiteral("latex/temporaryFileExt"),
                      QString::fromStdString(this->currentSettings.latexTemporaryFileExt));
    settings.setValue(QStringLiteral("general/uiLayoutVersion"), QT_SHELL_LAYOUT_VERSION);
    settings.setValue(QStringLiteral("general/toolbarProfileId"), QString::fromStdString(this->currentSettings.toolbarProfileId));
    settings.setValue(QStringLiteral("general/workspaceId"), QString::fromStdString(this->currentSettings.workspaceId));
    settings.setValue(QStringLiteral("view/showToolbar"), showToolbar);
    settings.setValue(QStringLiteral("view/showMenubar"), showMenubar);
    settings.setValue(QStringLiteral("view/showSidebar"), showSidebar);
    settings.setValue(QStringLiteral("view/showGeometryPanel"), showGeometryPanel);
    settings.setValue(QStringLiteral("view/pairedPages"), canvas->isPairedPagesEnabled());
    settings.setValue(QStringLiteral("view/pairOffset"), canvas->pairOffset());
    settings.setValue(QStringLiteral("view/layoutColumnsRows"), canvas->layoutColumnsRows());
    settings.setValue(QStringLiteral("view/verticalLayout"), canvas->isVerticalLayout());
    settings.setValue(QStringLiteral("view/layoutRtl"), canvas->isRightToLeftLayout());
    settings.setValue(QStringLiteral("view/layoutBtt"), canvas->isBottomToTopLayout());
    settings.setValue(QStringLiteral("audio/folder"), QString::fromStdString(this->currentSettings.audioFolder));
    settings.setValue(QStringLiteral("paths/lastOpen"), QString::fromStdString(this->currentSettings.lastOpenPath));
    settings.setValue(QStringLiteral("paths/lastSave"), QString::fromStdString(this->currentSettings.lastSavePath));
    settings.setValue(QStringLiteral("paths/lastImage"), QString::fromStdString(this->currentSettings.lastImagePath));
    settings.setValue(QStringLiteral("paths/lastPdf"), QString::fromStdString(this->currentSettings.lastPdfPath));
    settings.setValue(QStringLiteral("paths/lastExport"), QString::fromStdString(this->currentSettings.lastExportPath));
    settings.setValue(QStringLiteral("audio/sampleRate"), this->currentSettings.audioSampleRate);
    settings.setValue(QStringLiteral("audio/gain"), this->currentSettings.audioGain);
    settings.setValue(QStringLiteral("audio/defaultSeekTimeSeconds"), this->currentSettings.defaultSeekTimeSeconds);
    settings.setValue(QStringLiteral("audio/disabled"), this->currentSettings.disableAudio);
    settings.setValue(QStringLiteral("audio/inputDevice"), this->currentSettings.audioInputDevice);
    settings.setValue(QStringLiteral("audio/outputDevice"), this->currentSettings.audioOutputDevice);

    QStringList recentEntries;
    for (const auto& path: this->recentFiles.recentFiles()) {
        recentEntries.push_back(QString::fromStdString(path.string()));
    }
    settings.setValue(QStringLiteral("recentDocuments/files"), recentEntries);
    settings.setValue(QStringLiteral("window/geometry"), this->window.saveGeometry());
    settings.setValue(QStringLiteral("window/state"), this->window.saveState());
    for (std::size_t index = 0; index < this->window.floatingToolBars().size(); ++index) {
        auto* floatingToolBar = this->window.floatingToolBars()[index];
        settings.setValue(QStringLiteral("window/floatingToolbar%1Geometry").arg(index), floatingToolBar->saveGeometry());
        settings.setValue(QStringLiteral("window/floatingToolbar%1UserHidden").arg(index),
                          floatingToolBar->property("vertexUserHidden").toBool());
    }
    settings.sync();
}
