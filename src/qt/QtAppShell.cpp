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

namespace {

const std::vector<vn::ui::common::FileDialogFilter> SESSION_FILTERS = {
        {.label = "VertexNote Qt Session", .patterns = {"*.vnsession"}},
        {.label = "VertexNote Documents", .patterns = {"*.xopp", "*.xoj", "*.xopt", "*.pdf"}},
        {.label = "All Files", .patterns = {"*"}},
};

auto isSessionFile(const std::filesystem::path& path) -> bool { return path.extension() == ".vnsession"; }

auto joinFileDialogFilters(const std::vector<vn::ui::common::FileDialogFilter>& filters) -> QString {
    QStringList items;
    for (const auto& filter: filters) {
        QStringList patterns;
        for (const auto& pattern: filter.patterns) {
            patterns << QString::fromStdString(pattern);
        }
        items << QString::fromStdString(filter.label) + " (" + patterns.join(' ') + ")";
    }
    return items.join(";;");
}

auto defaultLatexTemplatePath() -> fs::path {
    return fs::path(PROJECT_SOURCE_DIR) / "resources" / "default_template.tex";
}

auto buildQtLatexSettings(const QtSettings& qtSettings) -> LatexSettings {
    LatexSettings settings;
    settings.autoCheckDependencies = qtSettings.latexAutoCheckDependencies;
    settings.defaultText = qtSettings.latexDefaultText;
    settings.globalTemplatePath =
            qtSettings.latexTemplatePath.empty() ? defaultLatexTemplatePath() : fs::path(qtSettings.latexTemplatePath);
    settings.genCmd = qtSettings.latexGenCmd;
    settings.sourceViewThemeId = qtSettings.latexSourceViewThemeId;
    settings.sourceViewAutoIndent = qtSettings.latexSourceViewAutoIndent;
    settings.sourceViewSyntaxHighlight = qtSettings.latexSourceViewSyntaxHighlight;
    settings.sourceViewShowLineNumbers = qtSettings.latexSourceViewShowLineNumbers;
    if (!qtSettings.latexEditorFont.empty()) {
        settings.editorFont = qtSettings.latexEditorFont;
    }
    settings.useCustomEditorFont = qtSettings.latexUseCustomEditorFont;
    settings.editorWordWrap = qtSettings.latexEditorWordWrap;
    settings.useExternalEditor = qtSettings.latexUseExternalEditor;
    settings.externalEditorAutoConfirm = qtSettings.latexExternalEditorAutoConfirm;
    settings.externalEditorCmd = qtSettings.latexExternalEditorCmd;
    settings.temporaryFileExt = qtSettings.latexTemporaryFileExt.empty() ? std::string("tex")
                                                                         : qtSettings.latexTemporaryFileExt;
    return settings;
}

auto loadLatexTemplate(const LatexSettings& settings) -> std::optional<std::string> {
    if (settings.globalTemplatePath.empty()) {
        return std::nullopt;
    }

    return Util::readString(settings.globalTemplatePath, false, std::ios::binary);
}

auto renderMathTex(const std::string& formula, const LatexSettings& settings, Color textColor, double x, double y)
        -> std::variant<std::unique_ptr<TexImage>, std::string> {
    const auto latexTemplate = loadLatexTemplate(settings);
    if (!latexTemplate) {
        return std::string("VertexNote could not load the LaTeX template file.");
    }

    auto texDir = Util::getTmpDirSubfolder("vertexnote-qt-tex");
    Util::ensureFolderExists(texDir);

    LatexGenerator generator(settings);
    const auto texContents = LatexGenerator::templateSub(formula, *latexTemplate, textColor);
    auto result = generator.asyncRun(texDir, texContents);
    if (auto* err = std::get_if<LatexGenerator::GenError>(&result)) {
        return err->message;
    }

    vn::util::GObjectSPtr<GSubprocess> process(std::get<GSubprocess*>(result), vn::util::adopt);
    GError* error = nullptr;
    char* stdoutBuffer = nullptr;
    const bool communicated =
            g_subprocess_communicate_utf8(process.get(), nullptr, nullptr, &stdoutBuffer, nullptr, &error);
    const std::string processOutput = stdoutBuffer ? stdoutBuffer : "";
    g_free(stdoutBuffer);

    if (!communicated) {
        const std::string message = error ? error->message : "VertexNote could not run the LaTeX generator.";
        if (error) {
            g_error_free(error);
        }
        return message;
    }

    const int exitStatus = g_subprocess_get_exit_status(process.get());
    if (exitStatus != 0) {
        if (!processOutput.empty()) {
            return processOutput;
        }
        return std::string("The LaTeX generator exited with an error.");
    }

    auto contents = Util::readString(texDir / "tex.pdf", false, std::ios::binary);
    if (!contents) {
        return std::string("VertexNote could not read the generated LaTeX PDF.");
    }

    auto image = std::make_unique<TexImage>();
    error = nullptr;
    const bool loaded = image->loadData(std::move(*contents), &error);
    if (error) {
        const std::string message = error->message;
        g_error_free(error);
        return message;
    }
    if (!loaded || !image->getPdf()) {
        return std::string("VertexNote could not load the generated LaTeX preview.");
    }

    image->setX(x);
    image->setY(y);
    image->setText(formula);
    return image;
}

}  // namespace

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

void QtAppShell::wireWindowState() {
    QObject::connect(&this->audioController, &QtAudioController::statusMessage, &this->window,
                     [this](const QString& text, int timeoutMs) { this->window.statusBar()->showMessage(text, timeoutMs); });
    QObject::connect(&this->audioController, &QtAudioController::audioStateChanged, &this->window,
                     [this]() { updateAudioCommandStates(); });

    QObject::connect(this->window.canvas(), &QtCanvas::statusHintChanged, &this->window,
                     [this](const QString& text) { this->window.statusBar()->showMessage(text); });

    QObject::connect(this->window.canvas(), &QtCanvas::viewportStateChanged, &this->window,
                     [this]() {
                         const auto currentPage = this->window.canvas()->currentPageIndex();
                         this->window.pageSidebar()->setCurrentPage(currentPage);
                         this->window.layerPanel()->setCurrentPage(currentPage);
                         updateEditCommandStates();
                         updateWindowTitle();
                         updateStatusBarLabels();
                         syncFooterWidgets();
                     });

    QObject::connect(this->window.canvas(), &QtCanvas::documentEdited, &this->window,
                     [this]() {
                         if (!this->suppressDirtyTracking) {
                             markSessionDirty();
                         }
                         updateEditCommandStates();
                         const auto currentPage = this->window.canvas()->currentPageIndex();
                         this->window.layerPanel()->setCurrentPage(currentPage);
                         this->window.pageSidebar()->setCurrentPage(currentPage);
                         this->window.layerPanel()->refresh();
                         this->window.pageSidebar()->refresh();
                         updateStatusBarLabels();
                         syncFooterWidgets();
                     });

    QObject::connect(this->window.canvas(), &QtCanvas::selectionStateChanged, &this->window,
                     [this]() { updateEditCommandStates(); });
    QObject::connect(this->window.canvas(), &QtCanvas::toolStateChanged, &this->window,
                     [this]() {
                         updateToolCommandStates();
                         this->window.toolPalette()->syncFromToolState(this->window.canvas()->toolState());
                         syncToolbarWidgets();
                     });

    QObject::connect(this->window.layerPanel(), &QtLayerPanel::layerChanged, &this->window,
                     [this]() {
                         this->window.canvas()->update();
                         if (!this->suppressDirtyTracking) {
                             markSessionDirty();
                         }
                         syncFooterWidgets();
                     });

    const auto syncSidebarVisibility = [this]() {
        const bool visible = isGtkParityProfileId(this->currentSettings.toolbarProfileId)
                                     ? this->window.pageSidebar()->isVisible()
                                     : (this->window.pageSidebar()->isVisible() || this->window.layerPanel()->isVisible());
        this->window.commandHost()->setCommandChecked("view.show-sidebar", visible);
        savePersistentUiState();
    };
    QObject::connect(this->window.pageSidebar(), &QDockWidget::visibilityChanged, &this->window,
                     [syncSidebarVisibility](bool) { syncSidebarVisibility(); });
    QObject::connect(this->window.layerPanel(), &QDockWidget::visibilityChanged, &this->window,
                     [syncSidebarVisibility](bool) { syncSidebarVisibility(); });

    for (auto* floatingToolBar: this->window.floatingToolBars()) {
        QObject::connect(floatingToolBar, &QToolBar::visibilityChanged, &this->window, [this, floatingToolBar](bool visible) {
            if (floatingToolBar->property("vertexProgrammaticVisibilityChange").toBool()) {
                return;
            }
            if (floatingToolBar->actions().isEmpty()) {
                return;
            }

            floatingToolBar->setProperty("vertexUserHidden", !visible);
            savePersistentUiState();
        });
    }

    // Sidebar page selection → scroll canvas to that page
    QObject::connect(this->window.pageSidebar(), &QtPageSidebar::pageSelected, &this->window,
                     [this](std::size_t pageIndex) {
                         this->window.canvas()->scrollToPage(pageIndex);
                         this->window.layerPanel()->setCurrentPage(pageIndex);
                         syncFooterWidgets();
                     });

    QObject::connect(this->window.footerPageSpin(), &QSpinBox::valueChanged, &this->window,
                     [this](int value) {
                         if (this->documentController.pageCount() == 0 || value <= 0) {
                             return;
                         }
                         this->window.canvas()->scrollToPage(static_cast<std::size_t>(value - 1));
                     });

    QObject::connect(this->window.footerLayerCombo(), &QComboBox::currentIndexChanged, &this->window,
                     [this](int index) {
                         if (index < 0 || !this->documentController.hasDocument()) {
                             return;
                         }
                         const auto currentPage = this->window.canvas()->currentPageIndex();
                         const auto layerIndex = static_cast<std::size_t>(
                                 this->window.footerLayerCombo()->itemData(index).toULongLong());
                         this->documentController.selectLayer(currentPage, layerIndex);
                         this->window.layerPanel()->setCurrentPage(currentPage);
                         this->window.layerPanel()->refresh();
                         this->window.canvas()->update();
                     });

    QObject::connect(this->window.footerZoomSlider(), &QSlider::valueChanged, &this->window,
                     [this](int value) {
                         const auto state = this->window.canvas()->sessionViewportState();
                         this->window.canvas()->setViewportState(static_cast<double>(value) / 100.0,
                                                                 state.scrollX, state.scrollY);
                     });

    // Tool palette → canvas tool state
    QObject::connect(this->window.toolPalette(), &QtToolPalette::colorChanged, &this->window,
                     [this](Color color) {
                         auto& ts = this->window.canvas()->toolState();
                         switch (ts.activeTool) {
                             case QtToolType::Pen:
                                 ts.penColor = color;
                                 break;
                             case QtToolType::Highlighter:
                                 ts.highlighterColor = color;
                                 break;
                             default:
                                 break;
                         }
                     });

    QObject::connect(this->window.toolPalette(), &QtToolPalette::widthChanged, &this->window,
                     [this](double width) {
                         auto& ts = this->window.canvas()->toolState();
                         switch (ts.activeTool) {
                             case QtToolType::Pen:
                                 ts.penWidth = width;
                                 break;
                             case QtToolType::Highlighter:
                                 ts.highlighterWidth = width;
                                 break;
                             case QtToolType::Eraser:
                                 ts.eraserWidth = width;
                                 break;
                             default:
                                 break;
                         }
                     });

    QObject::connect(this->window.toolPalette(), &QtToolPalette::pressureToggled, &this->window,
                     [this](bool enabled) { this->window.canvas()->toolState().pressureSensitive = enabled; });

    QObject::connect(this->window.toolPalette(), &QtToolPalette::eraserModeChanged, &this->window,
                     [this](QtEraserMode mode) { this->window.canvas()->toolState().eraserMode = mode; });

    if (this->fontFamilyCombo) {
        QObject::connect(this->fontFamilyCombo, &QFontComboBox::currentFontChanged, &this->window,
                         [this](const QFont& font) { this->window.canvas()->toolState().fontName = font.family().toStdString(); });
    }

    if (this->fontSizeSpinner) {
        QObject::connect(this->fontSizeSpinner, &QDoubleSpinBox::valueChanged, &this->window,
                         [this](double size) { this->window.canvas()->toolState().fontSize = size; });
    }

    updateEditCommandStates();
    updateAudioCommandStates();
    syncFooterWidgets();
}

void QtAppShell::newSession() {
    this->session.newDocument();
    this->documentController.newBlankDocument();
    this->suppressDirtyTracking = true;
    this->window.canvas()->newBlankDocument();
    this->window.canvas()->fitWidth();
    this->suppressDirtyTracking = false;
    updateEditCommandStates();
    this->window.layerPanel()->refresh();
    this->window.pageSidebar()->refresh();
    syncFooterWidgets();
    this->window.statusBar()->showMessage(QStringLiteral("Created a blank document"), 3000);
    updateWindowTitle();
}

void QtAppShell::rebuildRecentDocumentsMenu() {
    auto* menu = this->window.commandHost()->menuForPath("File>Recent Documents");
    menu->clear();

    const auto recentPaths = this->recentFiles.recentFiles();
    if (recentPaths.empty()) {
        auto* emptyAction = menu->addAction(QStringLiteral("No Recent Documents"));
        emptyAction->setEnabled(false);
        return;
    }

    for (std::size_t index = 0; index < recentPaths.size(); ++index) {
        const auto& path = recentPaths[index];
        const QString filename = QString::fromStdString(path.filename().string());
        const QString fullPath = QString::fromStdString(path.string());
        auto* action =
                menu->addAction(QStringLiteral("&%1 %2").arg(index + 1).arg(filename.isEmpty() ? fullPath : filename));
        action->setToolTip(fullPath);
        action->setStatusTip(fullPath);
        QObject::connect(action, &QAction::triggered, &this->window, [this, path]() { openPath(path, true); });
    }

    menu->addSeparator();
    auto* clearAction = menu->addAction(QStringLiteral("Clear Recent Documents"));
    QObject::connect(clearAction, &QAction::triggered, &this->window, [this]() {
        this->recentFiles.setRecentFiles({});
        rebuildRecentDocumentsMenu();
        savePersistentUiState();
        this->window.statusBar()->showMessage(QStringLiteral("Recent documents cleared"), 3000);
    });
}

auto QtAppShell::openPath(const std::filesystem::path& path, bool fromRecentDocuments) -> bool {
    if (!std::filesystem::exists(path)) {
        if (fromRecentDocuments) {
            auto recentPaths = this->recentFiles.recentFiles();
            recentPaths.erase(std::remove(recentPaths.begin(), recentPaths.end(), path), recentPaths.end());
            this->recentFiles.setRecentFiles(recentPaths);
            rebuildRecentDocumentsMenu();
            savePersistentUiState();
        }
        this->dialogs.showError("Open Failed",
                                "VertexNote could not find this recent document anymore. It was removed from the list.");
        return false;
    }

    if (isSessionFile(path)) {
        const auto sessionState = this->session.openFrom(path);
        if (!sessionState) {
            this->dialogs.showError("Open Failed", "VertexNote could not parse this Qt session file.");
            return false;
        }

        if (sessionState->linkedDocumentPath) {
            std::string error;
            if (!this->documentController.loadFrom(*sessionState->linkedDocumentPath, &error)) {
                this->dialogs.showError("Open Failed", error.empty() ? "VertexNote could not open the linked document."
                                                                     : error);
                return false;
            }
        } else {
            this->documentController.newBlankDocument();
        }

        this->suppressDirtyTracking = true;
        this->window.canvas()->setViewportState(sessionState->viewport.zoom, sessionState->viewport.scrollX,
                                                sessionState->viewport.scrollY);
        this->suppressDirtyTracking = false;
        this->recentFiles.addRecentFile(path);
        this->currentSettings.lastOpenPath = path.parent_path().string();
        rebuildRecentDocumentsMenu();
        savePersistentUiState();
        updateEditCommandStates();
        this->window.layerPanel()->refresh();
        this->window.pageSidebar()->refresh();
        syncFooterWidgets();
        this->window.statusBar()->showMessage(QString::fromStdString("Opened session " + path.filename().string()), 4000);
        updateWindowTitle();
        return true;
    }

    std::string error;
    if (!this->documentController.loadFrom(path, &error)) {
        this->dialogs.showError("Open Failed", error.empty() ? "VertexNote could not open this document." : error);
        return false;
    }

    this->session.newDocument();
    this->suppressDirtyTracking = true;
    this->window.canvas()->fitWidth();
    this->suppressDirtyTracking = false;
    this->recentFiles.addRecentFile(path);
    this->currentSettings.lastOpenPath = path.parent_path().string();
    rebuildRecentDocumentsMenu();
    savePersistentUiState();
    updateEditCommandStates();
    this->window.layerPanel()->refresh();
    this->window.pageSidebar()->refresh();
    syncFooterWidgets();
    this->window.statusBar()->showMessage(QString::fromStdString("Opened document " + path.filename().string()), 4000);
    updateWindowTitle();
    return true;
}

void QtAppShell::openSession() {
    const QString filePath = QFileDialog::getOpenFileName(&this->window, QStringLiteral("Open Document"),
                                                          dialogInitialDirectory(this->currentSettings.lastOpenPath),
                                                          joinFileDialogFilters(SESSION_FILTERS));
    if (filePath.isEmpty()) {
        return;
    }
    rememberDialogPath(this->currentSettings.lastOpenPath, filePath);
    openPath(std::filesystem::path(filePath.toStdWString()), false);
}

void QtAppShell::annotatePdf() {
    const QString filePath = QFileDialog::getOpenFileName(&this->window, QStringLiteral("Annotate PDF"),
                                                          dialogInitialDirectory(this->currentSettings.lastPdfPath),
                                                          QStringLiteral("PDF Files (*.pdf)"));
    if (filePath.isEmpty()) {
        return;
    }
    rememberDialogPath(this->currentSettings.lastPdfPath, filePath);

    const auto attachAnswer =
            QMessageBox::question(&this->window, QStringLiteral("Annotate PDF"),
                                  QStringLiteral("Attach the PDF data to the document when saving?"),
                                  QMessageBox::Yes | QMessageBox::No | QMessageBox::Cancel, QMessageBox::Yes);
    if (attachAnswer == QMessageBox::Cancel) {
        return;
    }

    std::string error;
    const auto path = std::filesystem::path(filePath.toStdString());
    if (!this->documentController.loadPdfAsDocument(path, attachAnswer == QMessageBox::Yes, &error)) {
        this->dialogs.showError("Annotate PDF Failed",
                                error.empty() ? "VertexNote could not open this PDF." : error);
        return;
    }

    this->session.newDocument();
    this->suppressDirtyTracking = true;
    this->window.canvas()->fitWidth();
    this->suppressDirtyTracking = false;
    this->recentFiles.addRecentFile(path);
    rebuildRecentDocumentsMenu();
    savePersistentUiState();
    updateEditCommandStates();
    this->window.layerPanel()->refresh();
    this->window.pageSidebar()->refresh();
    syncFooterWidgets();
    this->window.statusBar()->showMessage(QStringLiteral("PDF opened for annotation"), 4000);
    updateWindowTitle();
}

void QtAppShell::saveSessionAs() {
    const auto suggestedPath = this->session.currentPath().value_or(std::filesystem::path("session.vnsession"));
    QString initialPath = QString::fromStdWString(suggestedPath.wstring());
    if (!this->currentSettings.lastSavePath.empty() && !suggestedPath.is_absolute()) {
        initialPath = QDir(dialogInitialDirectory(this->currentSettings.lastSavePath)).filePath(initialPath);
    }
    const QString filePath = QFileDialog::getSaveFileName(&this->window, QStringLiteral("Save Document"), initialPath,
                                                          joinFileDialogFilters(SESSION_FILTERS));
    if (filePath.isEmpty()) {
        return;
    }
    rememberDialogPath(this->currentSettings.lastSavePath, filePath);
    const auto path = std::filesystem::path(filePath.toStdWString());

    const QtSessionState sessionState{.viewport = this->window.canvas()->sessionViewportState(),
                                                  .linkedDocumentPath = this->documentController.sourcePath()};
    if (!this->session.saveAs(path, sessionState)) {
        this->dialogs.showError("Save Failed", "VertexNote could not save the Qt session file.");
        return;
    }

    this->recentFiles.addRecentFile(path);
    rebuildRecentDocumentsMenu();
    savePersistentUiState();
    this->window.statusBar()->showMessage(QString::fromStdString("Saved " + path.filename().string()), 4000);
    updateWindowTitle();
}

void QtAppShell::markSessionDirty() {
    if (!this->session.isDirty()) {
        this->session.markDirty(true);
        updateWindowTitle();
    }
}

void QtAppShell::checkForUpdates(bool silentWhenCurrent) {
    this->updates.showCheckingForUpdates();
    QPointer<QtMainWindow> windowGuard(&this->window);
    std::thread([this, windowGuard, silentWhenCurrent]() {
        struct UpdateResult {
            bool ok = false;
            bool available = false;
            vn::ui::common::UpdateReleaseSummary summary;
            std::string error;
        } result;

        try {
            const auto payload = vn::update::fetchLatestReleaseJson();
            const auto release = vn::update::parseGithubRelease(payload);
            if (!release) {
                result.error = "Could not parse GitHub release response.";
            } else {
                result.ok = true;
                result.available = vn::update::isUpdateAvailable(PROJECT_VERSION, release->tagName);
                const auto asset = vn::update::selectBestAsset(*release, vn::update::currentReleasePlatform());
                result.summary = {.version = release->tagName,
                                  .title = release->name.empty() ? release->tagName : release->name,
                                  .notes = release->body,
                                  .downloadUrl = asset ? asset->downloadUrl : release->htmlUrl};
            }
        } catch (const std::exception& ex) {
            result.error = ex.what();
        } catch (...) {
            result.error = "Unknown update check failure.";
        }

        if (!windowGuard) {
            return;
        }

        QMetaObject::invokeMethod(windowGuard.data(), [this, windowGuard, silentWhenCurrent, result = std::move(result)]() {
            if (!windowGuard) {
                return;
            }
            if (!result.ok) {
                if (!silentWhenCurrent) {
                    this->updates.showUpdateError(result.error.empty() ? "Could not check for updates." : result.error);
                }
                return;
            }
            if (result.available) {
                this->updates.showUpdateAvailable(result.summary);
            } else if (!silentWhenCurrent) {
                this->updates.showUpToDate(PROJECT_VERSION);
            }
        }, Qt::QueuedConnection);
    }).detach();
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

void QtAppShell::exportPdf() {
    if (!this->documentController.hasDocument()) {
        return;
    }

    const QString filePath = QFileDialog::getSaveFileName(&this->window, QStringLiteral("Export PDF"),
                                                          dialogInitialDirectory(this->currentSettings.lastExportPath),
                                                          QStringLiteral("PDF Files (*.pdf)"));
    if (filePath.isEmpty()) {
        return;
    }
    rememberDialogPath(this->currentSettings.lastExportPath, filePath);

    auto* renderer = this->window.canvas()->contentRenderer();
    if (!renderer) {
        return;
    }

    QtDocumentExporter exp(renderer);
    std::string errorMsg;
    const auto& pages = this->documentController.snapshotPages();
    if (exp.exportPdf(filePath.toStdString(), pages, &errorMsg)) {
        this->window.statusBar()->showMessage(QStringLiteral("PDF exported successfully"), 3000);
    } else {
        QMessageBox::warning(&this->window, QStringLiteral("Export Failed"),
                             QString::fromStdString(errorMsg));
    }
}

void QtAppShell::exportPng() {
    if (!this->documentController.hasDocument()) {
        return;
    }

    const QString filePath = QFileDialog::getSaveFileName(&this->window, QStringLiteral("Export PNG"),
                                                          dialogInitialDirectory(this->currentSettings.lastExportPath),
                                                          QStringLiteral("PNG Images (*.png)"));
    if (filePath.isEmpty()) {
        return;
    }
    rememberDialogPath(this->currentSettings.lastExportPath, filePath);

    auto* renderer = this->window.canvas()->contentRenderer();
    if (!renderer) {
        return;
    }

    // Export page 0 (current single-page focus)
    const auto& pages = this->documentController.snapshotPages();
    if (pages.empty()) {
        return;
    }

    QtDocumentExporter exp(renderer);
    std::string errorMsg;
    if (exp.exportPng(filePath.toStdString(), pages[0], 2.0, &errorMsg)) {
        this->window.statusBar()->showMessage(QStringLiteral("PNG exported successfully"), 3000);
    } else {
        QMessageBox::warning(&this->window, QStringLiteral("Export Failed"),
                             QString::fromStdString(errorMsg));
    }
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

void QtAppShell::saveDocument() {
    if (!this->documentController.hasDocument()) {
        return;
    }

    // If there's an existing source path, save there; otherwise prompt
    auto existingPath = this->documentController.sourcePath();
    std::filesystem::path savePath;
    if (existingPath) {
        savePath = *existingPath;
    } else {
        const QString filePath = QFileDialog::getSaveFileName(&this->window, QStringLiteral("Save Document"),
                                                              dialogInitialDirectory(this->currentSettings.lastSavePath),
                                                              QStringLiteral("VertexNote Files (*.xopp)"));
        if (filePath.isEmpty()) {
            return;
        }
        rememberDialogPath(this->currentSettings.lastSavePath, filePath);
        savePath = filePath.toStdString();
    }

    std::string errorMsg;
    if (this->documentController.saveDocument(savePath, &errorMsg)) {
        this->session.markDirty(false);
        this->recentFiles.addRecentFile(savePath);
        rebuildRecentDocumentsMenu();
        savePersistentUiState();
        updateWindowTitle();
        this->window.statusBar()->showMessage(QStringLiteral("Document saved"), 3000);
    } else {
        QMessageBox::warning(&this->window, QStringLiteral("Save Failed"), QString::fromStdString(errorMsg));
    }
}

void QtAppShell::printDocument() {
    if (!this->documentController.hasDocument()) {
        return;
    }

    auto* renderer = this->window.canvas()->contentRenderer();
    if (!renderer) {
        return;
    }

    QtDocumentExporter exp(renderer);
    const auto& pages = this->documentController.snapshotPages();
    if (exp.printDocument(pages, &this->window)) {
        this->window.statusBar()->showMessage(QStringLiteral("Document printed"), 3000);
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

void QtAppShell::findText() {
    bool ok = false;
    const QString searchTerm = QInputDialog::getText(&this->window, QStringLiteral("Find Text"),
                                                     QStringLiteral("Search for:"), QLineEdit::Normal, QString(), &ok);
    if (!ok || searchTerm.isEmpty()) {
        return;
    }

    const auto results = this->documentController.findTextInDocument(searchTerm.toStdString());
    if (results.empty()) {
        QMessageBox::information(&this->window, QStringLiteral("Find Text"),
                                 QStringLiteral("No matches found for \"%1\".").arg(searchTerm));
        return;
    }

    // Show summary and scroll to first result
    const auto& first = results.front();
    QString msg = QStringLiteral("Found %1 match(es). First on page %2.")
                          .arg(results.size())
                          .arg(first.pageIndex + 1);
    this->window.statusBar()->showMessage(msg, 5000);

    // Scroll to the page of the first result
    const auto& pages = this->documentController.snapshotPages();
    double y = 0.0;
    constexpr double PAGE_GAP = 20.0;
    for (std::size_t i = 0; i < first.pageIndex && i < pages.size(); ++i) {
        y += pages[i].height + PAGE_GAP;
    }
    this->window.canvas()->setViewportState(this->window.canvas()->sessionViewportState().zoom, 0.0, y);
    this->window.canvas()->update();
}

void QtAppShell::insertImage() {
    if (!this->documentController.hasDocument()) {
        return;
    }

    const QString filePath =
            QFileDialog::getOpenFileName(&this->window, QStringLiteral("Insert Image"),
                                         dialogInitialDirectory(this->currentSettings.lastImagePath),
                                         QStringLiteral("Images (*.png *.jpg *.jpeg *.bmp *.gif *.svg)"));
    if (filePath.isEmpty()) {
        return;
    }
    rememberDialogPath(this->currentSettings.lastImagePath, filePath);

    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        QMessageBox::warning(&this->window, QStringLiteral("Insert Image"), QStringLiteral("Could not read the image file."));
        return;
    }

    const QByteArray imageData = file.readAll();
    file.close();

    // Insert on page 0, layer 0 at a default position
    this->documentController.insertImage(0, 100.0, 100.0, std::string(imageData.constData(), imageData.size()),
                                         200.0, 200.0);
    this->window.canvas()->update();
    markSessionDirty();
    this->window.statusBar()->showMessage(QStringLiteral("Image inserted"), 3000);
}

void QtAppShell::insertMathTex() {
    if (!this->documentController.hasDocument()) {
        return;
    }

    const auto settings = buildQtLatexSettings(this->currentSettings);
    if (!fs::is_regular_file(settings.globalTemplatePath)) {
        QMessageBox::warning(&this->window, QStringLiteral("Math TeX"),
                             QStringLiteral("VertexNote could not find the LaTeX template file."));
        return;
    }

    QDialog dialog(&this->window);
    dialog.setWindowTitle(QStringLiteral("Insert Math TeX"));
    dialog.resize(560, 360);

    auto* layout = new QVBoxLayout(&dialog);
    auto* hint = new QLabel(
            QStringLiteral("Enter LaTeX to render into the current page. VertexNote will compile it with the shared LaTeX pipeline."),
            &dialog);
    hint->setWordWrap(true);
    layout->addWidget(hint);

    auto* editor = new QPlainTextEdit(&dialog);
    editor->setObjectName(QStringLiteral("vertexNoteQtMathTexEditor"));
    editor->setPlainText(QString::fromStdString(settings.defaultText));
    editor->setLineWrapMode(settings.editorWordWrap ? QPlainTextEdit::WidgetWidth : QPlainTextEdit::NoWrap);
    layout->addWidget(editor, 1);

    auto* buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
    layout->addWidget(buttonBox);
    QObject::connect(buttonBox, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    QObject::connect(buttonBox, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);

    if (dialog.exec() != QDialog::Accepted) {
        return;
    }

    const QString formulaText = editor->toPlainText().trimmed();
    if (formulaText.isEmpty()) {
        this->window.statusBar()->showMessage(QStringLiteral("Math TeX insertion canceled: empty formula"), 3000);
        return;
    }

    const auto pageIndex = this->window.canvas()->currentPageIndex();
    const auto& pages = this->documentController.snapshotPages();
    if (pageIndex >= pages.size()) {
        return;
    }

    QApplication::setOverrideCursor(Qt::WaitCursor);
    auto renderResult = renderMathTex(formulaText.toStdString(), settings, this->window.canvas()->toolState().penColor,
                                      pages[pageIndex].width * 0.5, pages[pageIndex].height * 0.5);
    QApplication::restoreOverrideCursor();

    if (const auto* error = std::get_if<std::string>(&renderResult)) {
        QMessageBox::warning(&this->window, QStringLiteral("Math TeX"),
                             QStringLiteral("VertexNote could not render this LaTeX formula.\n\n%1")
                                     .arg(QString::fromStdString(*error)));
        return;
    }

    auto* texImage = std::get_if<std::unique_ptr<TexImage>>(&renderResult);
    if (!texImage || !*texImage) {
        QMessageBox::warning(&this->window, QStringLiteral("Math TeX"),
                             QStringLiteral("VertexNote could not create the rendered TeX image."));
        return;
    }

    ElementPtr element(texImage->release());
    if (!this->documentController.insertElement(pageIndex, std::move(element), "Insert LaTeX")) {
        QMessageBox::warning(&this->window, QStringLiteral("Math TeX"),
                             QStringLiteral("VertexNote could not insert the rendered TeX image into the document."));
        return;
    }

    this->window.canvas()->update();
    this->window.pageSidebar()->refresh();
    this->window.layerPanel()->refresh();
    syncFooterWidgets();
    markSessionDirty();
    this->window.statusBar()->showMessage(QStringLiteral("LaTeX formula inserted"), 3000);
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

// ---------------------------------------------------------------------------
// Phase 7: Clipboard & element operations
// ---------------------------------------------------------------------------

void QtAppShell::deleteSelection() {
    if (!this->documentController.hasDocument()) {
        return;
    }
    if (this->documentController.deleteSelectedElements()) {
        this->window.canvas()->update();
        markSessionDirty();
        this->window.statusBar()->showMessage(QStringLiteral("Selection deleted"), 3000);
    }
}

void QtAppShell::selectAll() {
    if (!this->documentController.hasDocument()) {
        return;
    }
    const auto pageIndex = this->window.canvas()->currentPageIndex();
    this->documentController.selectAllElements(pageIndex);
    this->window.canvas()->update();
    this->window.statusBar()->showMessage(QStringLiteral("All elements selected"), 3000);
}

void QtAppShell::copySelection() {
    auto clones = this->documentController.copySelectedElements();
    if (clones.empty()) {
        this->window.statusBar()->showMessage(QStringLiteral("Nothing to copy"), 3000);
        return;
    }
    this->elementClipboard = std::move(clones);
    this->window.statusBar()->showMessage(
            QStringLiteral("Copied %1 element(s)").arg(this->elementClipboard.size()), 3000);
}

void QtAppShell::cutSelection() {
    auto clones = this->documentController.cutSelectedElements();
    if (clones.empty()) {
        this->window.statusBar()->showMessage(QStringLiteral("Nothing to cut"), 3000);
        return;
    }
    this->elementClipboard = std::move(clones);
    this->window.canvas()->update();
    markSessionDirty();
    this->window.statusBar()->showMessage(
            QStringLiteral("Cut %1 element(s)").arg(this->elementClipboard.size()), 3000);
}

void QtAppShell::pasteClipboard() {
    if (this->elementClipboard.empty()) {
        this->window.statusBar()->showMessage(QStringLiteral("Clipboard is empty"), 3000);
        return;
    }
    if (!this->documentController.hasDocument()) {
        return;
    }

    // Clone clipboard contents so the clipboard survives for repeated paste
    std::vector<ElementPtr> clones;
    clones.reserve(this->elementClipboard.size());
    for (const auto& elem: this->elementClipboard) {
        clones.push_back(elem->clone());
    }

    const auto pageIndex = this->window.canvas()->currentPageIndex();
    if (this->documentController.pasteElements(pageIndex, std::move(clones))) {
        this->window.canvas()->update();
        markSessionDirty();
        this->window.statusBar()->showMessage(
                QStringLiteral("Pasted %1 element(s)").arg(this->elementClipboard.size()), 3000);
    }
}

// ---------------------------------------------------------------------------
// Phase 8: Page navigation
// ---------------------------------------------------------------------------

void QtAppShell::goToPage(std::size_t pageIndex) {
    if (!this->documentController.hasDocument()) {
        return;
    }
    if (pageIndex >= this->documentController.pageCount()) {
        return;
    }
    recordNavPoint();
    this->window.canvas()->scrollToPage(pageIndex);
    this->window.canvas()->update();
    this->window.statusBar()->showMessage(
            QStringLiteral("Page %1 of %2").arg(pageIndex + 1).arg(this->documentController.pageCount()), 3000);
}

void QtAppShell::goToFirstPage() { goToPage(0); }

void QtAppShell::goToLastPage() {
    if (this->documentController.hasDocument() && this->documentController.pageCount() > 0) {
        goToPage(this->documentController.pageCount() - 1);
    }
}

void QtAppShell::goToNextPage() {
    if (!this->documentController.hasDocument()) {
        return;
    }
    const auto current = this->window.canvas()->currentPageIndex();
    if (current + 1 < this->documentController.pageCount()) {
        goToPage(current + 1);
    }
}

void QtAppShell::goToPreviousPage() {
    if (!this->documentController.hasDocument()) {
        return;
    }
    const auto current = this->window.canvas()->currentPageIndex();
    if (current > 0) {
        goToPage(current - 1);
    }
}

void QtAppShell::goToPageDialog() {
    if (!this->documentController.hasDocument()) {
        return;
    }
    bool ok = false;
    const int pageNum = QInputDialog::getInt(&this->window, QStringLiteral("Go to Page"),
                                             QStringLiteral("Page number (1-%1):").arg(this->documentController.pageCount()),
                                             static_cast<int>(this->window.canvas()->currentPageIndex()) + 1, 1,
                                             static_cast<int>(this->documentController.pageCount()), 1, &ok);
    if (ok) {
        goToPage(static_cast<std::size_t>(pageNum - 1));
    }
}

// ---------------------------------------------------------------------------
// Phase 9: Layer operations
// ---------------------------------------------------------------------------

void QtAppShell::copyLayer() {
    if (!this->documentController.hasDocument()) {
        return;
    }
    const auto pageIndex = this->window.canvas()->currentPageIndex();
    const auto layerIndex = this->documentController.selectedLayerIndex(pageIndex);
    this->documentController.copyLayer(pageIndex, layerIndex);
    this->window.canvas()->update();
    this->window.layerPanel()->refresh();
    markSessionDirty();
    this->window.statusBar()->showMessage(QStringLiteral("Layer copied"), 3000);
}

void QtAppShell::mergeLayerDown() {
    if (!this->documentController.hasDocument()) {
        return;
    }
    const auto pageIndex = this->window.canvas()->currentPageIndex();
    const auto layerIndex = this->documentController.selectedLayerIndex(pageIndex);
    if (layerIndex == 0) {
        this->window.statusBar()->showMessage(QStringLiteral("Cannot merge bottom layer"), 3000);
        return;
    }
    this->documentController.mergeLayerDown(pageIndex, layerIndex);
    this->window.canvas()->update();
    this->window.layerPanel()->refresh();
    markSessionDirty();
    this->window.statusBar()->showMessage(QStringLiteral("Layer merged down"), 3000);
}

void QtAppShell::showAllLayers() {
    if (!this->documentController.hasDocument()) {
        return;
    }
    const auto pageIndex = this->window.canvas()->currentPageIndex();
    this->documentController.showAllLayers(pageIndex);
    this->window.canvas()->update();
    this->window.layerPanel()->refresh();
    markSessionDirty();
    this->window.statusBar()->showMessage(QStringLiteral("All layers visible"), 3000);
}

void QtAppShell::hideAllLayers() {
    if (!this->documentController.hasDocument()) {
        return;
    }
    const auto pageIndex = this->window.canvas()->currentPageIndex();
    this->documentController.hideAllLayers(pageIndex);
    this->window.canvas()->update();
    this->window.layerPanel()->refresh();
    markSessionDirty();
    this->window.statusBar()->showMessage(QStringLiteral("All layers hidden"), 3000);
}

void QtAppShell::renameLayerDialog() {
    if (!this->documentController.hasDocument()) {
        return;
    }
    const auto pageIndex = this->window.canvas()->currentPageIndex();
    const auto layerIndex = this->documentController.selectedLayerIndex(pageIndex);
    const auto infos = this->documentController.layerInfos(pageIndex);
    if (layerIndex >= infos.size()) {
        return;
    }

    bool ok = false;
    const QString newName = QInputDialog::getText(
            &this->window, QStringLiteral("Rename Layer"), QStringLiteral("Layer name:"),
            QLineEdit::Normal, QString::fromStdString(infos[layerIndex].name), &ok);
    if (ok && !newName.isEmpty()) {
        this->documentController.renameLayer(pageIndex, layerIndex, newName.toStdString());
        this->window.layerPanel()->refresh();
        markSessionDirty();
        this->window.statusBar()->showMessage(QStringLiteral("Layer renamed"), 3000);
    }
}

// ---------------------------------------------------------------------------
// Phase 10: Page operations
// ---------------------------------------------------------------------------

void QtAppShell::addPageBefore() {
    if (!this->documentController.hasDocument()) {
        return;
    }
    const auto pageIndex = this->window.canvas()->currentPageIndex();
    this->documentController.addPageBefore(pageIndex);
    this->window.canvas()->update();
    this->window.pageSidebar()->refresh();
    updateEditCommandStates();
    markSessionDirty();
    this->window.statusBar()->showMessage(QStringLiteral("Page added before"), 3000);
}

void QtAppShell::movePageUp() {
    if (!this->documentController.hasDocument()) {
        return;
    }
    const auto pageIndex = this->window.canvas()->currentPageIndex();
    if (pageIndex == 0) {
        this->window.statusBar()->showMessage(QStringLiteral("Already at the first page"), 3000);
        return;
    }
    this->documentController.movePageTowards(pageIndex, -1);
    this->window.canvas()->scrollToPage(pageIndex - 1);
    this->window.canvas()->update();
    this->window.pageSidebar()->refresh();
    updateEditCommandStates();
    markSessionDirty();
    this->window.statusBar()->showMessage(QStringLiteral("Page moved up"), 3000);
}

void QtAppShell::movePageDown() {
    if (!this->documentController.hasDocument()) {
        return;
    }
    const auto pageIndex = this->window.canvas()->currentPageIndex();
    if (pageIndex + 1 >= this->documentController.pageCount()) {
        this->window.statusBar()->showMessage(QStringLiteral("Already at the last page"), 3000);
        return;
    }
    this->documentController.movePageTowards(pageIndex, 1);
    this->window.canvas()->scrollToPage(pageIndex + 1);
    this->window.canvas()->update();
    this->window.pageSidebar()->refresh();
    updateEditCommandStates();
    markSessionDirty();
    this->window.statusBar()->showMessage(QStringLiteral("Page moved down"), 3000);
}

// ---------------------------------------------------------------------------
// Phase 11: Z-order operations
// ---------------------------------------------------------------------------

void QtAppShell::bringToFront() {
    if (this->documentController.bringSelectionToFront()) {
        this->window.canvas()->update();
        markSessionDirty();
        this->window.statusBar()->showMessage(QStringLiteral("Brought to front"), 3000);
    }
}

void QtAppShell::sendToBack() {
    if (this->documentController.sendSelectionToBack()) {
        this->window.canvas()->update();
        markSessionDirty();
        this->window.statusBar()->showMessage(QStringLiteral("Sent to back"), 3000);
    }
}

void QtAppShell::bringForward() {
    if (this->documentController.bringSelectionForward()) {
        this->window.canvas()->update();
        markSessionDirty();
        this->window.statusBar()->showMessage(QStringLiteral("Brought forward"), 3000);
    }
}

void QtAppShell::sendBackward() {
    if (this->documentController.sendSelectionBackward()) {
        this->window.canvas()->update();
        markSessionDirty();
        this->window.statusBar()->showMessage(QStringLiteral("Sent backward"), 3000);
    }
}

// ---------------------------------------------------------------------------
// Phase 12: Pen styling & font
// ---------------------------------------------------------------------------

void QtAppShell::setPenLineStyle(const std::string& style) {
    auto& ts = this->window.canvas()->toolState();
    ts.penLineStyle = style;
    this->window.commandHost()->setCommandChecked("pen.line-solid", style == "plain");
    this->window.commandHost()->setCommandChecked("pen.line-dash", style == "dash");
    this->window.commandHost()->setCommandChecked("pen.line-dashdot", style == "dashdot");
    this->window.commandHost()->setCommandChecked("pen.line-dot", style == "dot");
    this->window.statusBar()->showMessage(
            QStringLiteral("Line style: %1").arg(QString::fromStdString(style)), 3000);
}

void QtAppShell::setStrokeFill(int fillOpacity) {
    auto& ts = this->window.canvas()->toolState();
    ts.fillOpacity = fillOpacity;
    ts.fillEnabled = fillOpacity > 0;
    if (this->toolbarFillAction) {
        const QSignalBlocker blocker(this->toolbarFillAction);
        this->toolbarFillAction->setChecked(ts.fillEnabled);
    }
    this->window.statusBar()->showMessage(QStringLiteral("Fill opacity: %1").arg(fillOpacity), 2500);
}

void QtAppShell::setPdfTextMarkerOpacity(int opacity) {
    auto& ts = this->window.canvas()->toolState();
    ts.pdfTextMarkerOpacity = std::clamp(opacity, 0, 255);
    this->window.canvas()->update();
    this->window.statusBar()->showMessage(QStringLiteral("PDF text marker opacity: %1").arg(ts.pdfTextMarkerOpacity),
                                          2500);
}

void QtAppShell::highlightPdfTextSelection() {
    auto& ts = this->window.canvas()->toolState();
    const int inserted = this->documentController.createPdfTextMarkerStrokesForSelection(
            QtPdfTextMarkerKind::Highlight, ts.pdfTextMarkerOpacity, ts.highlighterColor);
    if (inserted <= 0) {
        this->window.statusBar()->showMessage(QStringLiteral("No active PDF text selection to highlight"), 3000);
        return;
    }

    this->documentController.cancelPdfTextSelection();
    this->window.canvas()->update();
    updateEditCommandStates();
    markSessionDirty();
    this->window.statusBar()->showMessage(QStringLiteral("Highlighted selected PDF text"), 3000);
}

void QtAppShell::selectFont() {
    auto& ts = this->window.canvas()->toolState();

    bool ok = false;
    QFont current;
    current.setFamily(QString::fromStdString(ts.fontName));
    current.setPointSizeF(ts.fontSize);

    QFont selected = QFontDialog::getFont(&ok, current, &this->window, QStringLiteral("Select Font"));
    if (ok) {
        ts.fontName = selected.family().toStdString();
        ts.fontSize = selected.pointSizeF();
        this->window.statusBar()->showMessage(
                QStringLiteral("Font: %1 %2pt").arg(selected.family()).arg(selected.pointSizeF()), 3000);
    }
}

// ---------------------------------------------------------------------------
// Phase 13: Navigation history
// ---------------------------------------------------------------------------

void QtAppShell::recordNavPoint() {
    auto* canvas = this->window.canvas();
    const auto state = canvas->sessionViewportState();
    NavPoint point{.pageIndex = canvas->currentPageIndex(),
                   .scrollX = state.scrollX,
                   .scrollY = state.scrollY,
                   .zoom = state.zoom};

    // Trim forward history when recording a new point
    if (this->navHistoryIndex < this->navHistory.size()) {
        this->navHistory.resize(this->navHistoryIndex);
    }

    // Don't record duplicate consecutive positions
    if (!this->navHistory.empty()) {
        const auto& last = this->navHistory.back();
        if (last.pageIndex == point.pageIndex && std::abs(last.scrollX - point.scrollX) < 1.0 &&
            std::abs(last.scrollY - point.scrollY) < 1.0) {
            return;
        }
    }

    this->navHistory.push_back(point);
    this->navHistoryIndex = this->navHistory.size();

    // Limit history size
    if (this->navHistory.size() > 100) {
        this->navHistory.erase(this->navHistory.begin());
        this->navHistoryIndex--;
    }
}

auto QtAppShell::selectedElementsForAudioPlayback() const -> std::vector<const Element*> {
    if (const auto& selection = this->documentController.elementSelection(); selection.has_value()) {
        return selection->elements;
    }
    return {};
}

void QtAppShell::navigateBack() {
    if (this->navHistoryIndex == 0 || this->navHistory.empty()) {
        this->window.statusBar()->showMessage(QStringLiteral("No previous position"), 3000);
        return;
    }

    // Save current position if we're at the end
    if (this->navHistoryIndex == this->navHistory.size()) {
        recordNavPoint();
        this->navHistoryIndex--;  // Step back past the just-recorded point
    }

    this->navHistoryIndex--;
    const auto& point = this->navHistory[this->navHistoryIndex];
    this->window.canvas()->setViewportState(point.zoom, point.scrollX, point.scrollY);
    this->window.canvas()->update();
}

void QtAppShell::navigateForward() {
    if (this->navHistoryIndex + 1 >= this->navHistory.size()) {
        this->window.statusBar()->showMessage(QStringLiteral("No next position"), 3000);
        return;
    }

    this->navHistoryIndex++;
    const auto& point = this->navHistory[this->navHistoryIndex];
    this->window.canvas()->setViewportState(point.zoom, point.scrollX, point.scrollY);
    this->window.canvas()->update();
}

// ---------------------------------------------------------------------------
// Phase 13: Layer navigation
// ---------------------------------------------------------------------------

void QtAppShell::gotoNextLayer() {
    auto pageIdx = this->window.canvas()->currentPageIndex();
    auto current = this->documentController.selectedLayerIndex(pageIdx);
    auto count = this->documentController.layerCount(pageIdx);
    if (current + 1 < count) {
        this->documentController.selectLayer(pageIdx, current + 1);
        this->window.canvas()->update();
        this->window.statusBar()->showMessage(
                QStringLiteral("Layer %1 / %2").arg(current + 2).arg(count), 3000);
    }
}

void QtAppShell::gotoPrevLayer() {
    auto pageIdx = this->window.canvas()->currentPageIndex();
    auto current = this->documentController.selectedLayerIndex(pageIdx);
    if (current > 0) {
        this->documentController.selectLayer(pageIdx, current - 1);
        this->window.canvas()->update();
        auto count = this->documentController.layerCount(pageIdx);
        this->window.statusBar()->showMessage(
                QStringLiteral("Layer %1 / %2").arg(current).arg(count), 3000);
    }
}

void QtAppShell::gotoTopLayer() {
    auto pageIdx = this->window.canvas()->currentPageIndex();
    auto count = this->documentController.layerCount(pageIdx);
    if (count > 0) {
        this->documentController.selectLayer(pageIdx, count - 1);
        this->window.canvas()->update();
        this->window.statusBar()->showMessage(
                QStringLiteral("Top layer (%1 / %2)").arg(count).arg(count), 3000);
    }
}

void QtAppShell::addLayerAbove() {
    auto pageIdx = this->window.canvas()->currentPageIndex();
    this->documentController.addLayer(pageIdx);
    auto count = this->documentController.layerCount(pageIdx);
    // Select the newly added layer (top)
    this->documentController.selectLayer(pageIdx, count - 1);
    this->window.canvas()->update();
    markSessionDirty();
    this->window.statusBar()->showMessage(QStringLiteral("Added layer above"), 3000);
}

void QtAppShell::addLayerBelow() {
    auto pageIdx = this->window.canvas()->currentPageIndex();
    auto current = this->documentController.selectedLayerIndex(pageIdx);
    this->documentController.addLayer(pageIdx);
    // addLayer adds at top; keep selection on the same layer (shifted up by one)
    this->documentController.selectLayer(pageIdx, current);
    this->window.canvas()->update();
    markSessionDirty();
    this->window.statusBar()->showMessage(QStringLiteral("Added layer below"), 3000);
}

// ---------------------------------------------------------------------------
// Phase 14: Annotated page navigation
// ---------------------------------------------------------------------------

void QtAppShell::gotoNextAnnotatedPage() {
    auto current = this->window.canvas()->currentPageIndex();
    auto count = this->documentController.pageCount();
    for (std::size_t i = current + 1; i < count; ++i) {
        if (this->documentController.isPageAnnotated(i)) {
            recordNavPoint();
            this->window.canvas()->scrollToPage(i);
            this->window.canvas()->update();
            this->window.statusBar()->showMessage(
                    QStringLiteral("Annotated page %1").arg(i + 1), 3000);
            return;
        }
    }
    this->window.statusBar()->showMessage(QStringLiteral("No more annotated pages"), 3000);
}

void QtAppShell::gotoPrevAnnotatedPage() {
    auto current = this->window.canvas()->currentPageIndex();
    if (current == 0) {
        this->window.statusBar()->showMessage(QStringLiteral("No previous annotated pages"), 3000);
        return;
    }
    for (std::size_t i = current; i > 0; --i) {
        if (this->documentController.isPageAnnotated(i - 1)) {
            recordNavPoint();
            this->window.canvas()->scrollToPage(i - 1);
            this->window.canvas()->update();
            this->window.statusBar()->showMessage(
                    QStringLiteral("Annotated page %1").arg(i), 3000);
            return;
        }
    }
    this->window.statusBar()->showMessage(QStringLiteral("No previous annotated pages"), 3000);
}

// ---------------------------------------------------------------------------
// Pen/eraser/highlighter size and type
// ---------------------------------------------------------------------------

namespace {
constexpr std::array<double, 5> PEN_SIZES = {0.40, 0.85, 1.41, 3.54, 5.00};
constexpr std::array<double, 5> ERASER_SIZES = {3.00, 5.00, 8.50, 14.00, 20.00};
constexpr std::array<double, 5> HIGHLIGHTER_SIZES = {3.00, 5.00, 8.50, 14.00, 20.00};

const std::array<const char*, 5> PEN_SIZE_IDS = {
        "pen.size-very-fine", "pen.size-fine", "pen.size-medium", "pen.size-thick", "pen.size-very-thick"};
const std::array<const char*, 5> ERASER_SIZE_IDS = {
        "eraser.size-very-fine", "eraser.size-fine", "eraser.size-medium", "eraser.size-thick", "eraser.size-very-thick"};
const std::array<const char*, 5> HIGHLIGHTER_SIZE_IDS = {
        "highlighter.size-very-fine", "highlighter.size-fine", "highlighter.size-medium", "highlighter.size-thick", "highlighter.size-very-thick"};
}  // namespace

void QtAppShell::setPenSize(int sizeIndex) {
    if (sizeIndex < 0 || sizeIndex >= 5) return;
    this->window.canvas()->toolState().penWidth = PEN_SIZES[static_cast<std::size_t>(sizeIndex)];
    for (int i = 0; i < 5; ++i) {
        this->window.commandHost()->setCommandChecked(PEN_SIZE_IDS[static_cast<std::size_t>(i)], i == sizeIndex);
    }
    this->window.toolPalette()->syncFromToolState(this->window.canvas()->toolState());
}

void QtAppShell::setEraserSize(int sizeIndex) {
    if (sizeIndex < 0 || sizeIndex >= 5) return;
    this->window.canvas()->toolState().eraserWidth = ERASER_SIZES[static_cast<std::size_t>(sizeIndex)];
    for (int i = 0; i < 5; ++i) {
        this->window.commandHost()->setCommandChecked(ERASER_SIZE_IDS[static_cast<std::size_t>(i)], i == sizeIndex);
    }
    this->window.toolPalette()->syncFromToolState(this->window.canvas()->toolState());
}

void QtAppShell::setEraserType(QtEraserMode mode) {
    this->window.canvas()->toolState().eraserMode = mode;
    this->window.commandHost()->setCommandChecked("eraser.type-standard", mode == QtEraserMode::Standard);
    this->window.commandHost()->setCommandChecked("eraser.type-whiteout", mode == QtEraserMode::Whiteout);
    this->window.commandHost()->setCommandChecked("eraser.type-delete-stroke", mode == QtEraserMode::DeleteStroke);
}

void QtAppShell::setHighlighterSize(int sizeIndex) {
    if (sizeIndex < 0 || sizeIndex >= 5) return;
    this->window.canvas()->toolState().highlighterWidth = HIGHLIGHTER_SIZES[static_cast<std::size_t>(sizeIndex)];
    for (int i = 0; i < 5; ++i) {
        this->window.commandHost()->setCommandChecked(HIGHLIGHTER_SIZE_IDS[static_cast<std::size_t>(i)], i == sizeIndex);
    }
    this->window.toolPalette()->syncFromToolState(this->window.canvas()->toolState());
}

// ---------------------------------------------------------------------------
// View & UI toggles
// ---------------------------------------------------------------------------

void QtAppShell::togglePairedPages() {
    const bool enabled = !this->window.canvas()->isPairedPagesEnabled();
    if (enabled) {
        this->window.canvas()->setLayoutColumns(2);
    } else {
        this->window.canvas()->setLayoutColumns(1);
    }
    syncLayoutSpanCommandStates();
    savePersistentUiState();
    this->window.statusBar()->showMessage(
            enabled ? QStringLiteral("Paired pages enabled") : QStringLiteral("Paired pages disabled"), 3000);
    syncFooterWidgets();
}

void QtAppShell::toggleToolbarVisibility() {
    const bool visible = !this->window.mainToolBar()->isVisible();
    this->window.mainToolBar()->setVisible(visible);
    this->window.toolsToolBar()->setVisible(visible);
    this->window.footerToolBar()->setVisible(visible);
    applyAuxiliaryToolBarVisibility(visible);
    this->window.commandHost()->setCommandChecked("view.show-toolbar", visible);
    savePersistentUiState();
}

void QtAppShell::toggleMenubarVisibility() {
    auto* menubar = this->window.menuBar();
    const bool visible = !menubar->isVisible();
    menubar->setVisible(visible);
    this->window.commandHost()->setCommandChecked("view.show-menubar", visible);
    savePersistentUiState();
}

void QtAppShell::toggleSidebarVisibility() {
    auto* sidebar = this->window.pageSidebar();
    const bool visible = !sidebar->isVisible();
    applySidebarVisibility(visible);
    this->window.commandHost()->setCommandChecked("view.show-sidebar", visible);
    savePersistentUiState();
}

void QtAppShell::setLayoutVertical(bool vertical) {
    this->window.canvas()->setVerticalLayout(vertical);
    this->window.commandHost()->setCommandChecked("view.layout-horizontal", !vertical);
    this->window.commandHost()->setCommandChecked("view.layout-vertical", vertical);
    savePersistentUiState();
    this->window.statusBar()->showMessage(
            vertical ? QStringLiteral("Vertical layout") : QStringLiteral("Horizontal layout"), 3000);
    syncFooterWidgets();
}

void QtAppShell::setLayoutRtl(bool rtl) {
    this->window.canvas()->setRightToLeftLayout(rtl);
    this->window.commandHost()->setCommandChecked("view.layout-ltr", !rtl);
    this->window.commandHost()->setCommandChecked("view.layout-rtl", rtl);
    savePersistentUiState();
    this->window.statusBar()->showMessage(
            rtl ? QStringLiteral("Right to left") : QStringLiteral("Left to right"), 3000);
    syncFooterWidgets();
}

void QtAppShell::setLayoutBtt(bool btt) {
    this->window.canvas()->setBottomToTopLayout(btt);
    this->window.commandHost()->setCommandChecked("view.layout-ttb", !btt);
    this->window.commandHost()->setCommandChecked("view.layout-btt", btt);
    savePersistentUiState();
    this->window.statusBar()->showMessage(
            btt ? QStringLiteral("Bottom to top") : QStringLiteral("Top to bottom"), 3000);
    syncFooterWidgets();
}

void QtAppShell::setLayoutColumns(int columns) {
    this->window.canvas()->setLayoutColumns(columns);
    syncLayoutSpanCommandStates();
    savePersistentUiState();
    this->window.statusBar()->showMessage(
            QStringLiteral("%1 page %2").arg(columns).arg(columns == 1 ? QStringLiteral("column") : QStringLiteral("columns")),
            3000);
    syncFooterWidgets();
}

void QtAppShell::setLayoutRows(int rows) {
    this->window.canvas()->setLayoutRows(rows);
    syncLayoutSpanCommandStates();
    savePersistentUiState();
    this->window.statusBar()->showMessage(
            QStringLiteral("%1 page %2").arg(rows).arg(rows == 1 ? QStringLiteral("row") : QStringLiteral("rows")),
            3000);
    syncFooterWidgets();
}

void QtAppShell::setPairOffset(int offset) {
    this->window.canvas()->setPairOffset(offset);
    syncLayoutSpanCommandStates();
    savePersistentUiState();
    this->window.statusBar()->showMessage(QStringLiteral("Pair offset %1").arg(offset), 3000);
    syncFooterWidgets();
}

void QtAppShell::syncLayoutSpanCommandStates() {
    const int value = this->window.canvas()->layoutColumnsRows();
    this->window.commandHost()->setCommandChecked("view.paired-pages", value == 2);
    for (int columns = 1; columns <= 8; ++columns) {
        this->window.commandHost()->setCommandChecked("view.columns-" + std::to_string(columns), value == columns);
    }
    for (int rows = 1; rows <= 8; ++rows) {
        this->window.commandHost()->setCommandChecked("view.rows-" + std::to_string(rows), value == -rows);
    }
    const int pairOffset = this->window.canvas()->pairOffset();
    for (int offset = 0; offset <= 1; ++offset) {
        this->window.commandHost()->setCommandChecked("view.pair-offset-" + std::to_string(offset), pairOffset == offset);
    }
}

// ---------------------------------------------------------------------------
// Journal extras
// ---------------------------------------------------------------------------

void QtAppShell::addPageAtEnd() {
    if (!this->documentController.hasDocument()) return;
    const auto pageCount = this->documentController.pageCount();
    this->documentController.addPageAfter(pageCount > 0 ? pageCount - 1 : 0);
    this->window.canvas()->update();
    this->window.pageSidebar()->refresh();
    updateEditCommandStates();
    markSessionDirty();
    this->window.statusBar()->showMessage(QStringLiteral("Page added at end"), 3000);
}

void QtAppShell::appendNewPdfPages() {
    const int inserted = this->documentController.appendNewPdfPages();
    if (inserted < 0) {
        QMessageBox::information(&this->window, QStringLiteral("Append PDF Pages"),
                                 QStringLiteral("No PDF is attached to this document."));
        return;
    }
    if (inserted == 0) {
        this->window.statusBar()->showMessage(QStringLiteral("No new PDF pages to append"), 3000);
        return;
    }
    this->window.canvas()->update();
    this->window.pageSidebar()->refresh();
    syncFooterWidgets();
    updateEditCommandStates();
    markSessionDirty();
    this->window.statusBar()->showMessage(QStringLiteral("Appended %1 PDF page%2")
                                                  .arg(inserted)
                                                  .arg(inserted == 1 ? QString() : QStringLiteral("s")),
                                          3000);
}

void QtAppShell::deleteLayer() {
    if (!this->documentController.hasDocument()) return;
    const auto pageIndex = this->window.canvas()->currentPageIndex();
    const auto layerCount = this->documentController.layerCount(pageIndex);
    if (layerCount <= 1) {
        this->window.statusBar()->showMessage(QStringLiteral("Cannot delete the only layer"), 3000);
        return;
    }
    const auto layerIndex = this->documentController.selectedLayerIndex(pageIndex);
    this->documentController.removeLayer(pageIndex, layerIndex);
    this->window.canvas()->update();
    this->window.layerPanel()->refresh();
    markSessionDirty();
    this->window.statusBar()->showMessage(QStringLiteral("Layer deleted"), 3000);
}

// ---------------------------------------------------------------------------
// Edit extras
// ---------------------------------------------------------------------------

void QtAppShell::moveSelectionLayerUp() {
    if (!this->documentController.moveSelectionToAdjacentLayer(+1)) {
        return;
    }
    this->window.canvas()->update();
    this->window.layerPanel()->refresh();
    this->window.pageSidebar()->refresh();
    updateEditCommandStates();
    markSessionDirty();
    this->window.statusBar()->showMessage(QStringLiteral("Moved selection up one layer"), 3000);
}

void QtAppShell::moveSelectionLayerDown() {
    if (!this->documentController.moveSelectionToAdjacentLayer(-1)) {
        return;
    }
    this->window.canvas()->update();
    this->window.layerPanel()->refresh();
    this->window.pageSidebar()->refresh();
    updateEditCommandStates();
    markSessionDirty();
    this->window.statusBar()->showMessage(QStringLiteral("Moved selection down one layer"), 3000);
}
