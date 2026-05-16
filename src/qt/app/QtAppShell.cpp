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
#include <QPoint>
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
    this->window.geometryPanel()->setWindowIcon(bundledQtIcon("xopp-geometry-tools.svg"));

    registerBootstrapCommands();
    this->window.geometryPanel()->bindCommandHost(this->window.commandHost());
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
    this->luaPlugins.configureSidebarAccess(
            [this]() { return this->pluginSidebarPageNo; },
            [this](int pageNo) {
                this->pluginSidebarPageNo = std::max(1, pageNo);
                applySidebarVisibility(true);
                if (pageNo == 1) {
                    this->window.pageSidebar()->raise();
                } else if (pageNo == 2) {
                    this->window.layerPanel()->raise();
                }
            });
    this->luaPlugins.configureFloatingToolboxAccess([this](double x, double y) {
        if (auto* action = this->window.commandHost()->actionForCommand("view.show-toolbar")) {
            action->setChecked(true);
        }
        applyAuxiliaryToolBarVisibility(true);
        syncFloatingToolBarsVisibility(true);
        this->window.cascadeFloatingToolBars();
        for (auto* floatingToolBar: this->window.floatingToolBars()) {
            if (floatingToolBar && floatingToolBar->isVisible() && !floatingToolBar->actions().isEmpty()) {
                if (std::isfinite(x) && std::isfinite(y) && (x != 0.0 || y != 0.0)) {
                    floatingToolBar->move(this->window.mapToGlobal(QPoint(static_cast<int>(x), static_cast<int>(y))));
                }
                floatingToolBar->raise();
                break;
            }
        }
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
    this->window.commandHost()->setCommandChecked("view.show-geometry-panel", this->persistedShowGeometryPanel);
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
    syncWorkspaceCommandStates();
    wireWindowState();
    rebuildToolbar();
    rebuildRecentDocumentsMenu();
    if (!this->persistedWindowGeometry.isEmpty()) {
        this->window.restoreGeometry(this->persistedWindowGeometry);
    }
    if (!this->persistedWindowState.isEmpty() && !this->window.restoreState(this->persistedWindowState)) {
        this->persistedWindowState.clear();
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
    applyGeometryPanelVisibility(this->persistedShowGeometryPanel);
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

QtAppShell::~QtAppShell() {
    if (this->persistentUiStateSavingEnabled) {
        savePersistentUiState();
    }
}

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

void QtAppShell::setPersistentUiStateSavingEnabled(bool enabled) {
    this->persistentUiStateSavingEnabled = enabled;
}

void QtAppShell::showMainWindow() {
    this->window.show();
    QTimer::singleShot(0, &this->window, [this]() { this->window.canvas()->fitWidth(); });
    this->window.cascadeFloatingToolBars();
    if (this->currentSettings.presentationModeDefault && !this->presentationMode) {
        togglePresentationMode();
    }
}

void QtAppShell::requestQuit() { QApplication::quit(); }

void QtAppShell::setMainWindowTitle(std::string_view title) {
    this->window.setWindowTitle(QString::fromUtf8(title.data(), static_cast<int>(title.size())));
}
