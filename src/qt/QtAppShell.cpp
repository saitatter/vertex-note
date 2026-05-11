/*
 * VertexNote
 *
 * Qt app shell bootstrap.
 */

#include "QtAppShell.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <exception>
#include <thread>
#include <unordered_set>
#include <utility>
#include <variant>
#include <vector>

#include <QApplication>
#include <QAction>
#include <QByteArray>
#include <QColorDialog>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDesktopServices>
#include <QDir>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QFontComboBox>
#include <QFontDialog>
#include <QFormLayout>
#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
#include <QMenuBar>
#include <QMessageBox>
#include <QMenu>
#include <QMetaObject>
#include <QPlainTextEdit>
#include <QPalette>
#include <QPointer>
#include <QSettings>
#include <QShortcut>
#include <QSignalBlocker>
#include <QDoubleSpinBox>
#include <QStatusBar>
#include <QString>
#include <QStringList>
#include <QStyle>
#include <QHeaderView>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QToolBar>
#include <QToolButton>
#include <QTimer>
#include <QUrl>
#include <QComboBox>
#include <QSlider>
#include <QSpinBox>
#include <QVBoxLayout>
#include <QWidget>
#include <QSizePolicy>
#include <QFrame>

#include "config-paths.h"
#include "config.h"
#include "control/latex/LatexGenerator.h"
#include "control/settings/LatexSettings.h"
#include "model/TexImage.h"
#include "QtBackgroundDialog.h"
#include "QtIconResources.h"
#include "QtPageSidebar.h"
#include "QtSettingsDialog.h"
#include "QtToolFamilies.h"
#include "vertexnote/update/GithubReleaseParser.h"
#include "vertexnote/update/ReleaseAssetSelector.h"
#include "vertexnote/update/ReleaseFetcher.h"
#include "vertexnote/update/VersionComparator.h"
#include "filesystem.h"
#include "util/PathUtil.h"



QtAppShell::QtAppShell():
        dialogs(&this->window),
        updates(&this->window, this->window.statusBar()),
        plugins(this->window.commandHost(), &this->window),
        luaPlugins(&this->plugins, this->window.commandHost(), &this->window) {
    this->autosaveTimer = new QTimer(&this->window);
    QObject::connect(this->autosaveTimer, &QTimer::timeout, &this->window, [this]() { autosaveNow(); });
    this->currentSettings.audioFolder = Util::getDataSubfolder("audio").string();
    this->availableToolbarProfiles = availableToolbarProfileOptions();
    loadPersistentUiState();
    this->session.newDocument();
    this->window.canvas()->setDocumentController(&this->documentController);
    applyRuntimeSettings();
    this->audioController.applySettings(this->currentSettings);
    this->window.layerPanel()->setDocumentController(&this->documentController);
    this->window.toolPalette()->setCompactToolbarMode(true);

    // Wire page sidebar
    auto* sidebar = this->window.pageSidebar();
    sidebar->setDocumentController(&this->documentController);
    sidebar->setContentRenderer(this->window.canvas()->contentRenderer());
    sidebar->setWindowIcon(bundledQtIcon("xopp-sidebar-page-preview.svg"));
    sidebar->setCurrentPage(0U);
    this->window.layerPanel()->setWindowIcon(bundledQtIcon("xopp-sidebar-layerstack.svg"));
    this->window.layerPanel()->setCurrentPage(0U);

    registerBootstrapCommands();
    this->luaPlugins.configureDocumentAccess(
            &this->documentController, [this]() { return this->window.canvas()->currentPageIndex(); },
            [this](std::size_t pageIndex) {
                goToPage(pageIndex);
                this->window.pageSidebar()->setCurrentPage(pageIndex);
                this->window.layerPanel()->setCurrentPage(pageIndex);
                updateStatusBarLabels();
            },
            [this]() {
                this->window.canvas()->update();
                this->window.pageSidebar()->refresh();
                this->window.layerPanel()->refresh();
                updateStatusBarLabels();
            },
            [this]() { markSessionDirty(); });
    this->luaPlugins.configureExportAccess(
            [this](const std::filesystem::path& path, std::string* errorMessage) {
                auto* renderer = this->window.canvas()->contentRenderer();
                if (!renderer) {
                    if (errorMessage) {
                        *errorMessage = "No renderer available.";
                    }
                    return false;
                }
                QtDocumentExporter exporter(renderer);
                return exporter.exportPdf(path, this->documentController.snapshotPages(), errorMessage);
            },
            [this](const std::filesystem::path& path, std::string* errorMessage) {
                auto* renderer = this->window.canvas()->contentRenderer();
                if (!renderer) {
                    if (errorMessage) {
                        *errorMessage = "No renderer available.";
                    }
                    return false;
                }
                const auto& pages = this->documentController.snapshotPages();
                if (pages.empty()) {
                    if (errorMessage) {
                        *errorMessage = "No pages to export.";
                    }
                    return false;
                }
                QtDocumentExporter exporter(renderer);
                if (pages.size() == 1U) {
                    return exporter.exportPng(path, pages.front(), 2.0, errorMessage);
                }
                const auto directory = path.parent_path() / path.stem();
                std::filesystem::create_directories(directory);
                return exporter.exportAllPagesPng(directory, pages, 2.0, errorMessage);
            });
    this->luaPlugins.configureToolAccess(
            [this](uint32_t rgb, const std::string& tool, bool selection) {
                const Color color(rgb | 0xff000000U);
                auto& toolState = this->window.canvas()->toolState();
                auto normalizedTool = tool;
                std::ranges::transform(normalizedTool, normalizedTool.begin(),
                                       [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
                if (normalizedTool == "highlighter") {
                    toolState.highlighterColor = color;
                } else if (normalizedTool == "pen") {
                    toolState.penColor = color;
                } else if (normalizedTool.empty()) {
                    if (toolState.activeTool == QtToolType::Highlighter ||
                        toolState.activeTool == QtToolType::LaserPointerHighlighter) {
                        toolState.highlighterColor = color;
                    } else {
                        toolState.penColor = color;
                    }
                }
                if (selection && this->documentController.colorSelectedElements(color)) {
                    markSessionDirty();
                }
                this->window.toolPalette()->syncFromToolState(toolState);
                syncToolbarWidgets();
                this->window.canvas()->update();
            },
            [this]() { return this->window.canvas()->toolState(); });
    this->luaPlugins.configureColorPaletteAccess([this]() { return this->activeColorPalette; });
    this->luaPlugins.configureViewAccess(
            [this]() { return this->window.canvas()->zoom(); },
            [this](double zoom) {
                this->window.canvas()->setZoom(zoom);
                updateStatusBarLabels();
            },
            [this]() { return this->window.canvas()->layoutColumnsRows(); });
    this->luaPlugins.configureDisplayAccess([this]() { return this->currentSettings.displayDpi; });
    this->luaPlugins.configureViewportAccess(
            [this]() { return this->window.canvas()->viewport(); },
            [this](double x, double y, bool relative) {
                const auto viewport = this->window.canvas()->viewport();
                this->window.canvas()->setViewportState(viewport.zoom, relative ? viewport.scrollX + x : x,
                                                        relative ? viewport.scrollY + y : y);
                updateStatusBarLabels();
            });
    this->luaPlugins.configureFontAccess(
            [this]() {
                const auto& toolState = this->window.canvas()->toolState();
                return std::pair<std::string, double>{toolState.fontName, toolState.fontSize};
            },
            [this](std::string name, double size) {
                auto& toolState = this->window.canvas()->toolState();
                toolState.fontName = std::move(name);
                toolState.fontSize = size;
                this->window.toolPalette()->syncFromToolState(toolState);
                syncToolbarWidgets();
                this->window.canvas()->update();
            });
    this->luaPlugins.configureFileAccess([this](const std::filesystem::path& path, int pageIndex) {
        if (!openPath(path, false)) {
            return false;
        }
        if (pageIndex >= 0 && this->documentController.pageCount() > 0U) {
            const auto wantedPage =
                    std::min<std::size_t>(static_cast<std::size_t>(pageIndex), this->documentController.pageCount() - 1U);
            goToPage(wantedPage);
            this->window.pageSidebar()->setCurrentPage(wantedPage);
            this->window.layerPanel()->setCurrentPage(wantedPage);
            updateStatusBarLabels();
        }
        return true;
    });
    this->luaPlugins.loadEnabledPlugins();
    this->window.commandHost()->setCommandChecked("view.show-toolbar", this->persistedShowToolbar);
    this->window.commandHost()->setCommandChecked("view.show-menubar", this->persistedShowMenubar);
    this->window.commandHost()->setCommandChecked("view.show-sidebar", this->persistedShowSidebar);
    this->window.commandHost()->setCommandChecked("view.paired-pages", this->persistedPairedPages);
    this->window.commandHost()->setCommandChecked("view.layout-horizontal", !this->persistedVerticalLayout);
    this->window.commandHost()->setCommandChecked("view.layout-vertical", this->persistedVerticalLayout);
    this->window.commandHost()->setCommandChecked("view.layout-ltr", !this->persistedLayoutRtl);
    this->window.commandHost()->setCommandChecked("view.layout-rtl", this->persistedLayoutRtl);
    this->window.commandHost()->setCommandChecked("view.layout-ttb", !this->persistedLayoutBtt);
    this->window.commandHost()->setCommandChecked("view.layout-btt", this->persistedLayoutBtt);
    if (this->persistedLayoutColumnsRows < 0) {
        this->window.canvas()->setLayoutRows(std::abs(this->persistedLayoutColumnsRows));
    } else {
        this->window.canvas()->setLayoutColumns(std::max(1, this->persistedLayoutColumnsRows));
    }
    this->window.canvas()->setPairOffset(this->persistedPairOffset);
    this->window.canvas()->setVerticalLayout(this->persistedVerticalLayout);
    this->window.canvas()->setRightToLeftLayout(this->persistedLayoutRtl);
    this->window.canvas()->setBottomToTopLayout(this->persistedLayoutBtt);
    syncLayoutSpanCommandStates();
    wireWindowState();
    rebuildToolbar();
    rebuildRecentDocumentsMenu();
    if (!this->persistedWindowGeometry.isEmpty()) {
        this->window.restoreGeometry(this->persistedWindowGeometry);
    }
    if (!this->persistedWindowState.isEmpty()) {
        this->window.restoreState(this->persistedWindowState);
    }
    for (std::size_t index = 0; index < this->window.floatingToolBars().size(); ++index) {
        auto* floatingToolBar = this->window.floatingToolBars()[index];
        const bool hasSavedGeometry =
                index < this->persistedFloatingToolBarGeometries.size() &&
                !this->persistedFloatingToolBarGeometries[index].isEmpty();
        floatingToolBar->setProperty("vertexHasSavedGeometry", hasSavedGeometry);
        const bool userHidden =
                index < this->persistedFloatingToolBarUserHidden.size() && this->persistedFloatingToolBarUserHidden[index];
        floatingToolBar->setProperty("vertexUserHidden", userHidden);
        if (hasSavedGeometry) {
            floatingToolBar->restoreGeometry(this->persistedFloatingToolBarGeometries[index]);
        }
    }
    bool openedMostRecent = false;
    if (this->currentSettings.autoloadMostRecent) {
        for (const auto& recentPath: this->recentFiles.recentFiles()) {
            if (openPath(recentPath, true)) {
                openedMostRecent = true;
                break;
            }
        }
    }
    if (!openedMostRecent) {
        this->window.canvas()->newBlankDocument();
        this->window.canvas()->fitWidth();
    }
    applySidebarSettings();
    this->window.cascadeFloatingToolBars();
    this->window.menuBar()->setVisible(this->persistedShowMenubar);
    applySidebarVisibility(this->persistedShowSidebar);
    this->window.mainToolBar()->setVisible(this->persistedShowToolbar);
    this->window.toolsToolBar()->setVisible(this->persistedShowToolbar);
    this->window.footerToolBar()->setVisible(this->persistedShowToolbar);
    applyAuxiliaryToolBarVisibility(this->persistedShowToolbar);
    sidebar->refresh();
    updateWindowTitle();
    updateEditCommandStates();
    configureAutosave();
    if (this->currentSettings.automaticUpdateCheckEnabled) {
        QTimer::singleShot(0, &this->window, [this]() { checkForUpdates(true); });
    }
}

QtAppShell::~QtAppShell() { savePersistentUiState(); }

auto QtAppShell::commandHost() -> vn::ui::common::ICommandHost* { return this->window.commandHost(); }

auto QtAppShell::canvasHost() -> vn::ui::common::ICanvasHost* { return this->window.canvas(); }

auto QtAppShell::clipboardService() -> vn::ui::common::IClipboardService* { return &this->clipboard; }

auto QtAppShell::dialogService() -> vn::ui::common::IDialogService* { return &this->dialogs; }

auto QtAppShell::recentFilesService() -> vn::ui::common::IRecentFilesService* { return &this->recentFiles; }

auto QtAppShell::updatePresentationService() -> vn::ui::common::IUpdatePresentationService* {
    return &this->updates;
}

auto QtAppShell::pluginUiBridge() -> vn::ui::common::IPluginUiBridge* { return &this->plugins; }

auto QtAppShell::nativeMainWindowHandle() const -> void* {
    return reinterpret_cast<void*>(const_cast<QtMainWindow*>(&this->window));
}

void QtAppShell::showMainWindow() {
    this->window.show();
    this->window.cascadeFloatingToolBars();
    if (this->currentSettings.presentationModeDefault && !this->presentationMode) {
        togglePresentationMode();
    }
}

void QtAppShell::requestQuit() { QApplication::quit(); }

void QtAppShell::setMainWindowTitle(std::string_view title) {
    this->window.setWindowTitle(QString::fromUtf8(title.data(), static_cast<int>(title.size())));
}

void QtAppShell::updateEditCommandStates() {
    this->window.commandHost()->setCommandEnabled("edit.undo-geometry", this->window.canvas()->canUndo());
    this->window.commandHost()->setCommandEnabled("edit.redo-geometry", this->window.canvas()->canRedo());
    const auto currentPage = this->window.canvas()->currentPageIndex();
    const bool hasDocument = this->documentController.hasDocument();
    const auto pageCount = this->documentController.pageCount();
    this->window.commandHost()->setCommandEnabled("page.add-before", hasDocument);
    this->window.commandHost()->setCommandEnabled("page.add", hasDocument);
    this->window.commandHost()->setCommandEnabled("page.add-end", hasDocument);
    this->window.commandHost()->setCommandEnabled("page.duplicate", hasDocument && pageCount > 0U);
    this->window.commandHost()->setCommandEnabled("page.move-up", hasDocument && currentPage > 0U);
    this->window.commandHost()->setCommandEnabled("page.move-down", hasDocument && currentPage + 1U < pageCount);
    this->window.commandHost()->setCommandEnabled("page.delete", hasDocument && pageCount > 1U);
    this->window.commandHost()->setCommandEnabled("journal.append-new-pdf-pages", hasDocument);
    this->window.commandHost()->setCommandEnabled("page.format", this->documentController.canResizePage(currentPage));
    this->window.commandHost()->setCommandEnabled("edit.move-selection-layer-up",
                                                  this->documentController.canMoveSelectionToAdjacentLayer(+1));
    this->window.commandHost()->setCommandEnabled("edit.move-selection-layer-down",
                                                  this->documentController.canMoveSelectionToAdjacentLayer(-1));
    updateToolCommandStates();
    updateAudioCommandStates();
}

void QtAppShell::setGeometrySnapEnabled(bool enabled) {
    this->window.canvas()->setGeometrySnapEnabled(enabled);
    this->window.commandHost()->setCommandChecked("view.toggle-geometry-snap", enabled);
    this->window.statusBar()->showMessage(
            enabled ? QStringLiteral("Geometry snap enabled") : QStringLiteral("Geometry snap disabled"), 2500);
}

void QtAppShell::setGridSnapEnabled(bool enabled) {
    this->window.canvas()->setGridSnapEnabled(enabled);
    this->window.commandHost()->setCommandChecked("view.toggle-grid-snap", enabled);
    this->window.statusBar()->showMessage(
            enabled ? QStringLiteral("Grid snap enabled") : QStringLiteral("Grid snap disabled"), 2500);
}

void QtAppShell::selectTool(QtToolType tool) {
    this->window.canvas()->setActiveTool(tool);
    updateToolCommandStates();
    this->window.toolPalette()->syncFromToolState(this->window.canvas()->toolState());
    syncToolbarWidgets();
    this->window.statusBar()->showMessage(
            QString::fromStdString("Tool: " + this->window.canvas()->toolState().activeToolName()), 2500);
}

void QtAppShell::updateToolCommandStates() {
    const auto active = this->window.canvas()->activeTool();
    this->window.commandHost()->setCommandChecked("tool.hand", active == QtToolType::Hand);
    this->window.commandHost()->setCommandChecked("tool.pen", active == QtToolType::Pen);
    this->window.commandHost()->setCommandChecked("tool.laser-pointer-pen", active == QtToolType::LaserPointerPen);
    this->window.commandHost()->setCommandChecked("tool.laser-pointer-highlighter",
                                                  active == QtToolType::LaserPointerHighlighter);
    this->window.commandHost()->setCommandChecked("tool.setsquare", active == QtToolType::Setsquare);
    this->window.commandHost()->setCommandChecked("tool.compass", active == QtToolType::Compass);
    this->window.commandHost()->setCommandChecked("tool.eraser", active == QtToolType::Eraser);
    this->window.commandHost()->setCommandChecked("tool.highlighter", active == QtToolType::Highlighter);
    this->window.commandHost()->setCommandChecked("tool.select", active == QtToolType::SelectRect);
    this->window.commandHost()->setCommandChecked("tool.select-pdf-text-linear", active == QtToolType::PdfTextLinear);
    this->window.commandHost()->setCommandChecked("tool.select-pdf-text-rect", active == QtToolType::PdfTextRect);
    this->window.commandHost()->setCommandChecked("tool.select-region", active == QtToolType::SelectRegion);
    this->window.commandHost()->setCommandChecked("tool.select-multilayer-rect", active == QtToolType::SelectMultiLayerRect);
    this->window.commandHost()->setCommandChecked("tool.select-multilayer-region", active == QtToolType::SelectMultiLayerRegion);
    this->window.commandHost()->setCommandChecked("tool.select-object", active == QtToolType::SelectObject);
    this->window.commandHost()->setCommandChecked("tool.vertical-space", active == QtToolType::VerticalSpace);
    this->window.commandHost()->setCommandChecked("tool.text", active == QtToolType::Text);
    this->window.commandHost()->setCommandChecked("tool.draw-line", active == QtToolType::DrawLine);
    this->window.commandHost()->setCommandChecked("tool.draw-rectangle", active == QtToolType::DrawRectangle);
    this->window.commandHost()->setCommandChecked("tool.draw-circle", active == QtToolType::DrawCircle);
    this->window.commandHost()->setCommandChecked("tool.draw-ellipse", active == QtToolType::DrawEllipse);
    this->window.commandHost()->setCommandChecked("tool.draw-arrow", active == QtToolType::DrawArrow);
    this->window.commandHost()->setCommandChecked("tool.draw-double-arrow", active == QtToolType::DrawDoubleArrow);
    this->window.commandHost()->setCommandChecked("tool.draw-coordinate-system", active == QtToolType::DrawCoordinateSystem);
    this->window.commandHost()->setCommandChecked("tool.draw-spline", active == QtToolType::DrawSpline);
    this->window.commandHost()->setCommandChecked("tool.draw-shape-recognizer", active == QtToolType::ShapeRecognizer);
    this->window.commandHost()->setCommandChecked("tool.draw-arc", active == QtToolType::DrawArc);
    this->window.commandHost()->setCommandChecked("tool.draw-polyline", active == QtToolType::DrawPolyline);
    this->window.commandHost()->setCommandChecked("tool.draw-construction-line", active == QtToolType::DrawConstructionLine);
    this->window.commandHost()->setCommandChecked("tool.draw-construction-circle", active == QtToolType::DrawConstructionCircle);
    syncToolbarWidgets();
}

void QtAppShell::updateAudioCommandStates() {
    const bool audioAvailable = this->audioController.isAudioAvailable();
    const bool canPlayTarget =
            audioAvailable && this->audioController.canStartPlayback(selectedElementsForAudioPlayback(),
                                                                    this->documentController.sourcePath());
    const bool canSeekOrStop = audioAvailable &&
                               (this->audioController.hasCurrentPlayback() || this->audioController.isPlaying() ||
                                this->audioController.isPaused());

    this->window.commandHost()->setCommandEnabled("audio.record", audioAvailable);
    this->window.commandHost()->setCommandChecked("audio.record", this->audioController.isRecording());
    this->window.commandHost()->setCommandEnabled("audio.pause-playback", audioAvailable && canPlayTarget);
    this->window.commandHost()->setCommandChecked("audio.pause-playback", this->audioController.isPaused());
    this->window.commandHost()->setCommandEnabled("audio.play-object", audioAvailable && canPlayTarget);
    this->window.commandHost()->setCommandEnabled("audio.seek-backwards", canSeekOrStop);
    this->window.commandHost()->setCommandEnabled("audio.seek-forwards", canSeekOrStop);
    this->window.commandHost()->setCommandEnabled("audio.stop-playback", canSeekOrStop);
}

void QtAppShell::toggleFullscreen() {
    const bool isFullscreen = this->window.isFullScreen();
    if (isFullscreen) {
        this->window.showNormal();
    } else {
        this->window.showFullScreen();
    }
    this->window.commandHost()->setCommandChecked("view.fullscreen", !isFullscreen);

    // If leaving fullscreen while in presentation mode, exit presentation too
    if (isFullscreen && this->presentationMode) {
        const bool showToolbars = this->window.commandHost()->actionForCommand("view.show-toolbar") &&
                                  this->window.commandHost()->actionForCommand("view.show-toolbar")->isChecked();
        const bool showSidebars = this->window.commandHost()->actionForCommand("view.show-sidebar") &&
                                  this->window.commandHost()->actionForCommand("view.show-sidebar")->isChecked();
        this->presentationMode = false;
        this->window.commandHost()->setCommandChecked("view.presentation", false);
        this->window.mainToolBar()->setVisible(showToolbars);
        this->window.toolsToolBar()->setVisible(showToolbars);
        this->window.footerToolBar()->setVisible(showToolbars);
        applyAuxiliaryToolBarVisibility(showToolbars);
        applySidebarVisibility(showSidebars);
    }
}

void QtAppShell::togglePresentationMode() {
    this->presentationMode = !this->presentationMode;
    this->window.commandHost()->setCommandChecked("view.presentation", this->presentationMode);

    if (this->presentationMode) {
        // Enter presentation: fullscreen, hide sidebar + layer panel + toolbar, fit page
        if (!this->window.isFullScreen()) {
            this->window.showFullScreen();
            this->window.commandHost()->setCommandChecked("view.fullscreen", true);
        }
        this->window.mainToolBar()->setVisible(false);
        this->window.toolsToolBar()->setVisible(false);
        this->window.footerToolBar()->setVisible(false);
        this->window.leftPrimaryToolBar()->setVisible(false);
        this->window.leftSecondaryToolBar()->setVisible(false);
        this->window.rightPrimaryToolBar()->setVisible(false);
        syncFloatingToolBarsVisibility(false);
        applySidebarVisibility(false);
        this->window.canvas()->fitPage(false);
        this->window.statusBar()->showMessage(QStringLiteral("Presentation mode — press F5 or Escape to exit"), 4000);
    } else {
        // Exit presentation: restore toolbar + sidebar, leave fullscreen
        const bool showToolbars = this->window.commandHost()->actionForCommand("view.show-toolbar") &&
                                  this->window.commandHost()->actionForCommand("view.show-toolbar")->isChecked();
        const bool showSidebars = this->window.commandHost()->actionForCommand("view.show-sidebar") &&
                                  this->window.commandHost()->actionForCommand("view.show-sidebar")->isChecked();
        this->window.mainToolBar()->setVisible(showToolbars);
        this->window.toolsToolBar()->setVisible(showToolbars);
        this->window.footerToolBar()->setVisible(showToolbars);
        applyAuxiliaryToolBarVisibility(showToolbars);
        applySidebarVisibility(showSidebars);
        if (this->window.isFullScreen()) {
            this->window.showNormal();
            this->window.commandHost()->setCommandChecked("view.fullscreen", false);
        }
        this->window.statusBar()->showMessage(QStringLiteral("Exited presentation mode"), 3000);
    }
}

void QtAppShell::addPage() {
    if (!this->documentController.hasDocument()) {
        return;
    }

    // Add after the last page
    const std::size_t pageCount = this->documentController.pageCount();
    this->documentController.addPageAfter(pageCount > 0 ? pageCount - 1 : 0);
    this->window.canvas()->update();
    this->window.pageSidebar()->refresh();
    updateEditCommandStates();
    markSessionDirty();
    this->window.statusBar()->showMessage(QStringLiteral("Page added"), 3000);
}

void QtAppShell::deletePage() {
    if (!this->documentController.hasDocument()) {
        return;
    }

    const std::size_t pageCount = this->documentController.pageCount();
    if (pageCount <= 1) {
        this->window.statusBar()->showMessage(QStringLiteral("Cannot delete the only page"), 3000);
        return;
    }

    // Delete the last page (simple policy for now)
    this->documentController.deletePage(pageCount - 1);
    this->window.canvas()->update();
    this->window.pageSidebar()->refresh();
    updateEditCommandStates();
    markSessionDirty();
    this->window.statusBar()->showMessage(QStringLiteral("Page deleted"), 3000);
}

void QtAppShell::duplicatePage() {
    if (!this->documentController.hasDocument()) {
        return;
    }

    const std::size_t pageCount = this->documentController.pageCount();
    if (pageCount == 0) {
        return;
    }

    // Duplicate the last page
    this->documentController.duplicatePage(pageCount - 1);
    this->window.canvas()->update();
    this->window.pageSidebar()->refresh();
    updateEditCommandStates();
    markSessionDirty();
    this->window.statusBar()->showMessage(QStringLiteral("Page duplicated"), 3000);
}

void QtAppShell::applyConstraint(vn::geom::ConstraintKind kind) {
    if (!this->documentController.hasDocument()) {
        return;
    }
    if (this->documentController.applyConstraint(kind)) {
        this->window.canvas()->update();
        markSessionDirty();
        this->window.statusBar()->showMessage(QStringLiteral("Constraint applied"), 3000);
    } else {
        this->window.statusBar()->showMessage(QStringLiteral("Cannot apply constraint — check selection"), 3000);
    }
}

void QtAppShell::deleteConstraints() {
    if (!this->documentController.hasDocument()) {
        return;
    }
    (void)this->documentController.deleteSelectedConstraints();
    this->window.canvas()->update();
    markSessionDirty();
    this->window.statusBar()->showMessage(QStringLiteral("Constraints deleted"), 3000);
}

void QtAppShell::editFixedLengthConstraint() {
    if (!this->documentController.hasDocument()) {
        return;
    }
    const auto existing = this->documentController.selectedFixedLengthConstraint();
    if (!existing) {
        this->window.statusBar()->showMessage(
                QStringLiteral("No fixed-length or radius constraint on selection"), 3000);
        return;
    }

    bool ok = false;
    const double newValue = QInputDialog::getDouble(&this->window, QStringLiteral("Edit Constraint Value"),
                                                    QStringLiteral("Value:"), existing->value, 0.01, 100000.0, 2, &ok);
    if (!ok) {
        return;
    }

    (void)this->documentController.updateFixedLengthConstraint(newValue);
    this->window.canvas()->update();
    markSessionDirty();
    this->window.statusBar()->showMessage(QStringLiteral("Constraint value updated"), 3000);
}

void QtAppShell::toggleAudioRecording() {
    if (this->audioController.toggleRecording()) {
        updateAudioCommandStates();
    }
}

void QtAppShell::toggleAudioPausePlayback() {
    if (this->audioController.togglePausePlayback(selectedElementsForAudioPlayback(),
                                                  this->documentController.sourcePath())) {
        updateAudioCommandStates();
    }
}

void QtAppShell::stopAudioPlayback() {
    if (this->audioController.stopPlayback()) {
        updateAudioCommandStates();
    }
}

void QtAppShell::seekAudioBackwards() {
    if (this->audioController.seekBackwards()) {
        updateAudioCommandStates();
    }
}

void QtAppShell::seekAudioForwards() {
    if (this->audioController.seekForwards()) {
        updateAudioCommandStates();
    }
}
