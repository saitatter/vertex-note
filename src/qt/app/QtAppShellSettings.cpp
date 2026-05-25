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
#include <string_view>

#include <QApplication>
#include <QColor>
#include <QComboBox>
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

auto isKnownWorkspaceId(std::string_view workspaceId) -> bool {
    return workspaceId == "notes" || workspaceId == "geometry" || workspaceId == "3d";
}

auto workspaceIdForProfile(std::string_view profileId) -> std::string_view {
    if (profileId == QT_GEOMETRY_PROFILE_ID) {
        return "geometry";
    }
    if (profileId == QT_3D_PROFILE_ID) {
        return "3d";
    }
    return "notes";
}

auto panelModeForWorkspace(std::string_view workspaceId) -> QtWorkspacePanelMode {
    if (workspaceId == "geometry") {
        return QtWorkspacePanelMode::Geometry;
    }
    if (workspaceId == "3d") {
        return QtWorkspacePanelMode::ThreeD;
    }
    return QtWorkspacePanelMode::Notes;
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

void QtAppShell::applyGeometryPanelVisibility(bool visible) {
    this->window.geometryPanel()->setVisible(visible);
    this->window.commandHost()->setCommandChecked("view.show-geometry-panel", visible);
}

void QtAppShell::applyWorkspacePreset(std::string_view profileId, std::string_view displayName, bool showGeometryPanel,
                                      bool wireframeView, bool vertexHandles, bool linkedMarkers, bool faceFills,
                                      QtGeometrySelectionMode selectionMode) {
    QtWorkspaceViewState state;
    state.initialized = true;
    state.showGeometryPanel = showGeometryPanel;
    state.wireframeView = wireframeView;
    state.vertexHandles = vertexHandles;
    state.linkedMarkers = linkedMarkers;
    state.faceFills = faceFills;
    state.selectionMode = selectionMode;
    applyWorkspacePreset(profileId, displayName, state);
}

void QtAppShell::applyWorkspacePreset(std::string_view profileId, std::string_view displayName,
                                      const QtWorkspaceViewState& state) {
    this->currentSettings.workspaceId = std::string(workspaceIdForProfile(profileId));
    this->currentSettings.toolbarProfileId = std::string(profileId);
    this->currentSettings.geometryWireframeView = state.wireframeView;
    this->currentSettings.geometryHighlightVertices = state.vertexHandles;
    this->currentSettings.geometryHighlightLinkedVertices = state.linkedMarkers;
    this->currentSettings.geometryShowFaceFills = state.faceFills;
    this->currentSettings.geometrySelectionModeDefault = state.selectionMode;

    auto* canvas = this->window.canvas();
    auto& toolState = canvas->toolState();
    toolState.geometrySelectionMode = state.selectionMode;
    canvas->setGeometryWireframeViewEnabled(state.wireframeView);
    canvas->setGeometryVertexOverlayEnabled(state.vertexHandles);
    canvas->setGeometryLinkedVertexOverlayEnabled(state.linkedMarkers);
    canvas->setGeometryFaceFillVisible(state.faceFills);

    rebuildToolbar();
    this->window.mainToolBar()->setVisible(true);
    this->window.toolsToolBar()->setVisible(true);
    this->window.footerToolBar()->setVisible(true);
    applyAuxiliaryToolBarVisibility(true);
    this->window.commandHost()->setCommandChecked("view.show-toolbar", true);
    applySidebarVisibility(this->window.commandHost()->actionForCommand("view.show-sidebar")
                                   ? this->window.commandHost()->actionForCommand("view.show-sidebar")->isChecked()
                                   : this->persistedShowSidebar);
    this->window.geometryPanel()->setWorkspaceMode(panelModeForWorkspace(activeWorkspaceId()));
    applyGeometryPanelVisibility(state.showGeometryPanel);

    updateToolCommandStates();
    syncWorkspaceCommandStates();
    updateStatusBarLabels();
    savePersistentUiState();
    this->window.canvas()->update();
    this->window.statusBar()->showMessage(
            QStringLiteral("%1 workspace").arg(QString::fromUtf8(displayName.data(),
                                                                 static_cast<qsizetype>(displayName.size()))),
            3000);
}

auto QtAppShell::defaultWorkspaceViewState(std::string_view workspaceId) const -> QtWorkspaceViewState {
    QtWorkspaceViewState state;
    state.initialized = true;
    if (workspaceId == "geometry") {
        state.showGeometryPanel = true;
        state.wireframeView = false;
        state.vertexHandles = true;
        state.linkedMarkers = true;
        state.faceFills = true;
        state.selectionMode = QtGeometrySelectionMode::Vertex;
        return state;
    }
    if (workspaceId == "3d") {
        state.showGeometryPanel = true;
        state.wireframeView = true;
        state.vertexHandles = true;
        state.linkedMarkers = true;
        state.faceFills = true;
        state.selectionMode = QtGeometrySelectionMode::Object;
        return state;
    }
    state.showGeometryPanel = true;
    state.wireframeView = false;
    state.vertexHandles = false;
    state.linkedMarkers = true;
    state.faceFills = true;
    state.selectionMode = QtGeometrySelectionMode::Vertex;
    return state;
}

auto QtAppShell::workspaceViewState(std::string_view workspaceId) -> QtWorkspaceViewState& {
    if (workspaceId == "geometry") {
        return this->geometryWorkspaceState;
    }
    if (workspaceId == "3d") {
        return this->threeDWorkspaceState;
    }
    return this->notesWorkspaceState;
}

auto QtAppShell::workspaceViewState(std::string_view workspaceId) const -> const QtWorkspaceViewState& {
    if (workspaceId == "geometry") {
        return this->geometryWorkspaceState;
    }
    if (workspaceId == "3d") {
        return this->threeDWorkspaceState;
    }
    return this->notesWorkspaceState;
}

void QtAppShell::rememberCurrentWorkspaceViewState() {
    auto& state = workspaceViewState(activeWorkspaceId());
    auto* canvas = this->window.canvas();
    state.initialized = true;
    state.showGeometryPanel = this->window.geometryPanel()->isVisible();
    state.wireframeView = canvas->isGeometryWireframeViewEnabled();
    state.vertexHandles = canvas->isGeometryVertexOverlayEnabled();
    state.linkedMarkers = canvas->isGeometryLinkedVertexOverlayEnabled();
    state.faceFills = canvas->isGeometryFaceFillVisible();
    state.selectionMode = canvas->toolState().geometrySelectionMode;
}

void QtAppShell::applyWorkspace(std::string_view workspaceId) {
    if (!isKnownWorkspaceId(workspaceId)) {
        workspaceId = "notes";
    }
    rememberCurrentWorkspaceViewState();

    if (workspaceId == "geometry") {
        auto& state = workspaceViewState(workspaceId);
        if (!state.initialized) {
            state = defaultWorkspaceViewState(workspaceId);
        }
        applyWorkspacePreset(QT_GEOMETRY_PROFILE_ID, "Geometry", state);
        this->currentSettings.workspaceId = "geometry";
        syncWorkspaceCommandStates();
        return;
    }
    if (workspaceId == "3d") {
        auto& state = workspaceViewState(workspaceId);
        if (!state.initialized) {
            state = defaultWorkspaceViewState(workspaceId);
        }
        applyWorkspacePreset(QT_3D_PROFILE_ID, "3D", state);
        this->currentSettings.workspaceId = "3d";
        syncWorkspaceCommandStates();
        return;
    }

    auto& state = workspaceViewState(workspaceId);
    if (!state.initialized) {
        state = defaultWorkspaceViewState(workspaceId);
    }
    applyWorkspacePreset(QT_GTK_PARITY_PROFILE_ID, "Write", state);
    this->currentSettings.workspaceId = "notes";
    syncWorkspaceCommandStates();
}

void QtAppShell::syncWorkspaceCommandStates() {
    if (!isKnownWorkspaceId(this->currentSettings.workspaceId)) {
        this->currentSettings.workspaceId = std::string(workspaceIdForProfile(this->currentSettings.toolbarProfileId));
    }

    const auto workspaceId = activeWorkspaceId();
    this->window.commandHost()->setCommandChecked("view.workspace-notes", workspaceId == "notes");
    this->window.commandHost()->setCommandChecked("view.workspace-geometry", workspaceId == "geometry");
    this->window.commandHost()->setCommandChecked("view.workspace-3d", workspaceId == "3d");
    this->window.geometryPanel()->setWorkspaceMode(panelModeForWorkspace(workspaceId));

    if (this->workspaceCombo) {
        this->suppressWorkspaceComboSync = true;
        const QString wanted = QString::fromUtf8(workspaceId.data(), static_cast<qsizetype>(workspaceId.size()));
        const int index = this->workspaceCombo->findData(wanted);
        if (index >= 0) {
            this->workspaceCombo->setCurrentIndex(index);
        }
        this->suppressWorkspaceComboSync = false;
    }
}

auto QtAppShell::activeWorkspaceId() const -> std::string_view {
    return isKnownWorkspaceId(this->currentSettings.workspaceId) ? std::string_view(this->currentSettings.workspaceId)
                                                                 : std::string_view("notes");
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
    ts.geometrySelectionMode = this->currentSettings.geometrySelectionModeDefault;

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
    canvas->setVertexSnapMarkerSize(this->currentSettings.vertexSnapMarkerSize);
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
    canvas->setGeometryWireframeViewEnabled(this->currentSettings.geometryWireframeView);
    canvas->setGeometryVertexOverlayEnabled(this->currentSettings.geometryHighlightVertices);
    canvas->setGeometryLinkedVertexOverlayEnabled(this->currentSettings.geometryHighlightLinkedVertices);
    canvas->setGeometryFaceFillVisible(this->currentSettings.geometryShowFaceFills);
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
