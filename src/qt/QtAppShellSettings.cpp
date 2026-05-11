/*
 * VertexNote
 *
 * Qt app shell runtime settings.
 */

#include "QtAppShell.h"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <string>

#include <QApplication>
#include <QColor>
#include <QPalette>
#include <QStatusBar>
#include <QStyle>
#include <QString>
#include <QTimer>
#include <QToolBar>

#include "QtColorPalette.h"
#include "QtIconResources.h"
#include "QtToolbarProfileStore.h"

namespace {

auto lowerExtension(const std::filesystem::path& path) -> std::string {
    auto ext = path.extension().string();
    std::ranges::transform(ext, ext.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return ext;
}

auto isAutosavableDocumentPath(const std::filesystem::path& path) -> bool {
    const auto ext = lowerExtension(path);
    return ext == ".xopp" || ext == ".xoj" || ext == ".xopt";
}

auto lightPalette() -> QPalette {
    QPalette palette;
    palette.setColor(QPalette::Window, QColor(244, 244, 244));
    palette.setColor(QPalette::WindowText, QColor(32, 32, 32));
    palette.setColor(QPalette::Base, QColor(255, 255, 255));
    palette.setColor(QPalette::AlternateBase, QColor(238, 238, 238));
    palette.setColor(QPalette::Text, QColor(32, 32, 32));
    palette.setColor(QPalette::Button, QColor(244, 244, 244));
    palette.setColor(QPalette::ButtonText, QColor(32, 32, 32));
    palette.setColor(QPalette::Highlight, QColor(47, 102, 255));
    palette.setColor(QPalette::HighlightedText, QColor(255, 255, 255));
    return palette;
}

auto darkPalette() -> QPalette {
    QPalette palette;
    palette.setColor(QPalette::Window, QColor(43, 45, 48));
    palette.setColor(QPalette::WindowText, QColor(236, 236, 236));
    palette.setColor(QPalette::Base, QColor(31, 32, 35));
    palette.setColor(QPalette::AlternateBase, QColor(52, 54, 58));
    palette.setColor(QPalette::Text, QColor(236, 236, 236));
    palette.setColor(QPalette::Button, QColor(52, 54, 58));
    palette.setColor(QPalette::ButtonText, QColor(236, 236, 236));
    palette.setColor(QPalette::Highlight, QColor(88, 140, 255));
    palette.setColor(QPalette::HighlightedText, QColor(255, 255, 255));
    return palette;
}

}  // namespace

void QtAppShell::syncFloatingToolBarsVisibility(bool showToolbars) {
    const bool allowFloatingToolBars = profileUsesFloatingToolBars(this->activeToolbarProfile);
    for (auto* floatingToolBar: this->window.floatingToolBars()) {
        const bool hasActions = !floatingToolBar->actions().isEmpty();
        if (!hasActions) {
            floatingToolBar->setProperty("vertexUserHidden", false);
        }
        const bool userHidden = floatingToolBar->property("vertexUserHidden").toBool();
        floatingToolBar->setProperty("vertexProgrammaticVisibilityChange", true);
        floatingToolBar->setVisible(showToolbars && allowFloatingToolBars && hasActions && !userHidden);
        floatingToolBar->setProperty("vertexProgrammaticVisibilityChange", false);
    }
}

void QtAppShell::applyAuxiliaryToolBarVisibility(bool showToolbars) {
    this->window.leftPrimaryToolBar()->setVisible(showToolbars && !this->window.leftPrimaryToolBar()->actions().isEmpty());
    this->window.leftSecondaryToolBar()->setVisible(showToolbars &&
                                                    !this->window.leftSecondaryToolBar()->actions().isEmpty());
    this->window.rightPrimaryToolBar()->setVisible(showToolbars &&
                                                   !this->window.rightPrimaryToolBar()->actions().isEmpty());
    syncFloatingToolBarsVisibility(showToolbars);
}

void QtAppShell::applySidebarVisibility(bool visible) {
    const bool gtkParity = isGtkParityProfileId(this->currentSettings.toolbarProfileId);
    this->window.setGtkParitySidebarMode(gtkParity);
    this->window.pageSidebar()->setVisible(visible);
    this->window.layerPanel()->setVisible(!gtkParity && visible);
}

void QtAppShell::configureAutosave() {
    if (!this->autosaveTimer) {
        return;
    }
    this->autosaveTimer->stop();
    if (!this->currentSettings.autosaveEnabled) {
        return;
    }
    const int intervalMinutes = std::clamp(this->currentSettings.autosaveTimeoutMinutes, 1, 120);
    this->autosaveTimer->setInterval(intervalMinutes * 60 * 1000);
    this->autosaveTimer->start();
}

void QtAppShell::autosaveNow() {
    if (!this->currentSettings.autosaveEnabled || !this->session.isDirty() || !this->documentController.hasDocument()) {
        return;
    }
    const auto source = this->documentController.sourcePath();
    if (!source || !isAutosavableDocumentPath(*source)) {
        return;
    }

    std::string errorMsg;
    if (this->documentController.saveDocument(*source, &errorMsg)) {
        this->session.markDirty(false);
        updateWindowTitle();
        this->window.statusBar()->showMessage(QStringLiteral("Document autosaved"), 2000);
    } else {
        this->window.statusBar()->showMessage(QStringLiteral("Autosave failed: %1").arg(QString::fromStdString(errorMsg)),
                                              5000);
    }
}

void QtAppShell::applyRuntimeSettings() {
    auto* canvas = this->window.canvas();
    auto& ts = canvas->toolState();
    ts.penWidth = this->currentSettings.defaultPenWidth;
    ts.highlighterWidth = this->currentSettings.defaultHighlighterWidth;
    ts.eraserWidth = this->currentSettings.defaultEraserWidth;
    ts.fontName = this->currentSettings.defaultFontName;
    ts.fontSize = this->currentSettings.defaultFontSize;
    ts.pressureSensitive = this->currentSettings.defaultPressureSensitive;
    ts.eraserMode = this->currentSettings.defaultEraserMode;

    canvas->setPressureOptions(this->currentSettings.minimumPressure, this->currentSettings.pressureMultiplier,
                               this->currentSettings.pressureGuessing);
    canvas->setStrokeStabilizerOptions(this->currentSettings.strokeStabilizerEnabled,
                                       this->currentSettings.strokeStabilizerSamples,
                                       this->currentSettings.strokeStabilizerStrength,
                                       this->currentSettings.strokeStabilizerFinalizeStroke,
                                       this->currentSettings.strokeStabilizerAveragingMethod,
                                       this->currentSettings.strokeStabilizerPreprocessor,
                                       this->currentSettings.strokeStabilizerSigma,
                                       this->currentSettings.strokeStabilizerDeadzoneRadius,
                                       this->currentSettings.strokeStabilizerDrag,
                                       this->currentSettings.strokeStabilizerMass,
                                       this->currentSettings.strokeStabilizerCuspDetection);
    canvas->setRestoreLineWidthOnScale(this->currentSettings.restoreLineWidthEnabled);
    canvas->setGridSnapOptions(this->currentSettings.snapGridSize, this->currentSettings.snapGridTolerance);
    canvas->setViewInteractionOptions(this->currentSettings.zoomStepPercent,
                                      this->currentSettings.zoomStepScrollPercent,
                                      this->currentSettings.rotationSnapTolerance);
    canvas->setTouchGestureOptions(this->currentSettings.zoomGesturesEnabled,
                                   this->currentSettings.touchZoomStartThreshold,
                                   this->currentSettings.touchInertialScrolling);
    canvas->setDrawDirectionModifiers(this->currentSettings.drawDirModsEnabled,
                                      this->currentSettings.drawDirModsRadius);
    canvas->setUnlimitedScrolling(this->currentSettings.unlimitedScrolling);
    canvas->setPageSpaceOptions(this->currentSettings.addHorizontalSpace,
                                this->currentSettings.addHorizontalSpaceAmountLeft,
                                this->currentSettings.addHorizontalSpaceAmountRight,
                                this->currentSettings.addVerticalSpace,
                                this->currentSettings.addVerticalSpaceAmountAbove,
                                this->currentSettings.addVerticalSpaceAmountBelow);
    canvas->setEraserCursorHidden(this->currentSettings.eraserCursorHidden);
    canvas->setInputSystemOptions(this->currentSettings.ignoredStylusEvents,
                                  this->currentSettings.inputSystemTPCButton,
                                  this->currentSettings.inputSystemDrawOutsideWindow);
    canvas->setPointerButtonActions(this->currentSettings.buttonMatrix);
    canvas->setInputDeviceButtonProfiles(this->currentSettings.inputDeviceButtonProfiles);
    canvas->setPageShadowEnabled(this->currentSettings.showPageShadow);
    canvas->setSelectionColor(this->currentSettings.selectionColor);
    canvas->setCanvasBackgroundColor(this->currentSettings.backgroundColor);
    canvas->setCursorHighlightOptions(this->currentSettings.highlightPosition,
                                      this->currentSettings.cursorHighlightColor,
                                      this->currentSettings.cursorHighlightBorderColor,
                                      this->currentSettings.cursorHighlightRadius,
                                      this->currentSettings.cursorHighlightBorderWidth);
    canvas->setRecolorOptions(this->currentSettings.recolorMainView, this->currentSettings.recolorLight,
                              this->currentSettings.recolorDark);
    this->window.pageSidebar()->setRecolorOptions(this->currentSettings.recolorSidebarMiniatures,
                                                  this->currentSettings.recolorLight, this->currentSettings.recolorDark);
    canvas->setGeometrySnapEnabled(this->currentSettings.geometrySnapDefault);
    canvas->setGridSnapEnabled(this->currentSettings.gridSnapDefault);
    canvas->setRotationSnapEnabled(this->currentSettings.rotationSnapDefault);
    canvas->setTouchDrawingEnabled(this->currentSettings.touchDrawingDefault);
    canvas->setShapeRecognizerMinSize(this->currentSettings.strokeRecognizerMinSize);
    canvas->setSnapRecognizedShapesEnabled(this->currentSettings.snapRecognizedShapesEnabled);
    canvas->setLaserPointerFadeOutMs(this->currentSettings.laserPointerFadeOutMs);
    canvas->setTextEditorTabOptions(this->currentSettings.useSpacesForTab,
                                    this->currentSettings.numberOfSpacesForTab);
    canvas->setEdgePanOptions(this->currentSettings.edgePanSpeed, this->currentSettings.edgePanMaxMult);
    canvas->setStrokeFilterOptions(this->currentSettings.strokeFilterEnabled,
                                   this->currentSettings.strokeFilterIgnoreTime,
                                   this->currentSettings.strokeFilterIgnoreLength,
                                   this->currentSettings.strokeFilterSuccessiveTime,
                                   this->currentSettings.doActionOnStrokeFiltered,
                                   this->currentSettings.trySelectOnStrokeFiltered);
    canvas->setEmptyLastPageAppendMode(this->currentSettings.emptyLastPageAppend);
    this->documentController.setPdfCacheOptions(this->currentSettings.pdfPageCacheSize,
                                                this->currentSettings.pdfPreloadPagesBefore,
                                                this->currentSettings.pdfPreloadPagesAfter,
                                                this->currentSettings.pdfEagerPageCleanup,
                                                this->currentSettings.pdfPageRerenderThreshold);
    applySidebarSettings();
    applyAppearanceSettings();
    reloadColorPalette();
}

void QtAppShell::applySidebarSettings() {
    this->window.setSidebarPreferences(this->currentSettings.sidebarWidth, this->currentSettings.sidebarOnRight,
                                       this->currentSettings.sidebarNumberingStyle,
                                       this->currentSettings.scrollbarHideType,
                                       this->currentSettings.scrollbarOnLeft,
                                       this->currentSettings.disableScrollbarFadeout);
}

void QtAppShell::applyAppearanceSettings() {
    auto* app = qobject_cast<QApplication*>(QApplication::instance());
    const auto theme = QString::fromStdString(this->currentSettings.themeVariant).toLower();
    if (app) {
        if (theme == QStringLiteral("light")) {
            app->setPalette(lightPalette());
        } else if (theme == QStringLiteral("dark")) {
            app->setPalette(darkPalette());
        } else {
            app->setPalette(app->style()->standardPalette());
        }
    }

    const auto iconTheme = QString::fromStdString(this->currentSettings.iconTheme).toLower() == QStringLiteral("lucide")
                                   ? std::string("lucide")
                                   : std::string("color");
    const auto iconTone = theme == QStringLiteral("dark") ? std::string("dark") : std::string("light");
    setQtIconAppearance(iconTheme, iconTone);
    this->window.layerPanel()->setIconAppearance(iconTheme, iconTone);
    updateWindowTitle();
    this->window.canvas()->update();
}

void QtAppShell::reloadColorPalette() {
    std::string errorMessage;
    this->activeColorPalette =
            qtLoadColorPaletteOrDefault(std::filesystem::path(this->currentSettings.colorPalettePath), &errorMessage);
    const auto colors = qtPaletteColorsOnly(this->activeColorPalette);
    this->window.toolPalette()->setQuickColors(colors);
    syncToolbarWidgets();

    if (!errorMessage.empty() && !this->currentSettings.colorPalettePath.empty()) {
        this->window.statusBar()->showMessage(
                QStringLiteral("Color palette fallback: %1").arg(QString::fromStdString(errorMessage)), 5000);
    }
}
