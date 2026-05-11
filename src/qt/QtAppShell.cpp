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

auto lowerExtension(const std::filesystem::path& path) -> std::string {
    auto ext = path.extension().string();
    std::ranges::transform(ext, ext.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return ext;
}

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

auto isAutosavableDocumentPath(const std::filesystem::path& path) -> bool {
    const auto ext = lowerExtension(path);
    return ext == ".xopp" || ext == ".xoj" || ext == ".xopt";
}

constexpr int QT_SHELL_LAYOUT_VERSION = 5;

auto settingsPointerAction(QSettings& settings, const QString& key, QtPointerButtonAction fallback)
        -> QtPointerButtonAction {
    return static_cast<QtPointerButtonAction>(settings.value(key, static_cast<int>(fallback)).toInt());
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

struct PaperPresetSpec {
    std::string_view id;
    std::string_view label;
    double width = 0.0;
    double height = 0.0;
};

constexpr std::array<PaperPresetSpec, 5> PAPER_PRESETS = {{
        {"custom", "Custom", 0.0, 0.0},
        {"a5", "A5", 420.0, 595.0},
        {"a4", "A4", 595.0, 842.0},
        {"letter", "Letter", 612.0, 792.0},
        {"legal", "Legal", 612.0, 1008.0},
}};

auto matchingPaperPreset(double width, double height) -> int {
    for (std::size_t i = 1; i < PAPER_PRESETS.size(); ++i) {
        const auto& preset = PAPER_PRESETS[i];
        const bool portrait = std::abs(width - preset.width) < 0.5 && std::abs(height - preset.height) < 0.5;
        const bool landscape = std::abs(width - preset.height) < 0.5 && std::abs(height - preset.width) < 0.5;
        if (portrait || landscape) {
            return static_cast<int>(i);
        }
    }
    return 0;
}

auto isLandscapeSize(double width, double height) -> bool { return width > height; }

auto defaultLatexTemplatePath() -> fs::path {
    return fs::path(PROJECT_SOURCE_DIR) / "resources" / "default_template.tex";
}

void applyQtPreferredLocale(const std::string& preferredLocale) {
    qputenv("LANGUAGE", QByteArray(preferredLocale.c_str(), static_cast<qsizetype>(preferredLocale.size())));
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
    settings.setValue(QStringLiteral("view/showToolbar"), showToolbar);
    settings.setValue(QStringLiteral("view/showMenubar"), showMenubar);
    settings.setValue(QStringLiteral("view/showSidebar"), showSidebar);
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

void QtAppShell::showBackgroundDialog() {
    // Use page 0 for now (single-page focus)
    const std::size_t pageIndex = 0;
    if (!this->documentController.hasDocument() || pageIndex >= this->documentController.pageCount()) {
        return;
    }

    const auto& pages = this->documentController.snapshotPages();
    if (pageIndex >= pages.size()) {
        return;
    }

    const auto& bg = pages[pageIndex].background;
    QtBackgroundDialog dialog(bg.backgroundColor, bg.backgroundFormat, &this->window);
    if (dialog.exec() != QDialog::Accepted) {
        return;
    }

    this->documentController.setPageBackgroundColor(pageIndex, dialog.selectedColor());
    this->documentController.setPageBackgroundType(pageIndex, dialog.selectedFormat());
    this->window.canvas()->update();
    markSessionDirty();
    this->window.statusBar()->showMessage(QStringLiteral("Page background updated"), 3000);
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

void QtAppShell::showSettingsDialog() {
    QtSettingsDialog dialog(this->currentSettings, this->availableToolbarProfiles,
                            this->audioController.inputDeviceOptions(),
                            this->audioController.outputDeviceOptions(), &this->window);
    if (dialog.exec() != QDialog::Accepted) {
        return;
    }

    const auto previousToolbarProfileId = this->currentSettings.toolbarProfileId;
    const auto previousIconTheme = this->currentSettings.iconTheme;
    const auto previousThemeVariant = this->currentSettings.themeVariant;
    const auto previousLocale = this->currentSettings.preferredLocale;
    this->currentSettings = dialog.settings();
    if (this->currentSettings.audioFolder.empty()) {
        this->currentSettings.audioFolder = Util::getDataSubfolder("audio").string();
    }

    // Apply relevant settings immediately
    applyRuntimeSettings();
    if (this->currentSettings.preferredLocale != previousLocale) {
        applyQtPreferredLocale(this->currentSettings.preferredLocale);
    }
    this->audioController.applySettings(this->currentSettings);
    this->window.commandHost()->setCommandChecked("view.toggle-geometry-snap", this->currentSettings.geometrySnapDefault);
    this->window.commandHost()->setCommandChecked("view.toggle-grid-snap", this->currentSettings.gridSnapDefault);
    this->window.commandHost()->setCommandChecked("view.toggle-rotation-snap", this->currentSettings.rotationSnapDefault);
    this->window.commandHost()->setCommandChecked("view.toggle-touch-drawing", this->currentSettings.touchDrawingDefault);

    this->window.toolPalette()->syncFromToolState(this->window.canvas()->toolState());
    if (this->currentSettings.toolbarProfileId != previousToolbarProfileId ||
        this->currentSettings.iconTheme != previousIconTheme ||
        this->currentSettings.themeVariant != previousThemeVariant) {
        rebuildToolbar();
        applySidebarVisibility(this->window.commandHost()->actionForCommand("view.show-sidebar")
                                       ? this->window.commandHost()->actionForCommand("view.show-sidebar")->isChecked()
                                       : this->persistedShowSidebar);
    }
    configureAutosave();
    savePersistentUiState();
    updateWindowTitle();
    updateAudioCommandStates();
    this->window.statusBar()->showMessage(QStringLiteral("Settings applied"), 3000);
}

void QtAppShell::showToolbarCustomizeDialog() {
    QtToolbarProfile baseProfile = customToolbarProfileFromSettings().value_or(this->activeToolbarProfile.value_or(QtToolbarProfile{}));
    if (baseProfile.id.empty()) {
        baseProfile = QtToolbarLayoutEngine::loadProfile(toolbarProfilePath(), QT_GTK_PARITY_PROFILE_ID).value_or(QtToolbarProfile{});
    }

    QDialog dialog(&this->window);
    dialog.setWindowTitle(QStringLiteral("Customize Toolbars"));
    dialog.setMinimumSize(720, 460);
    auto* layout = new QVBoxLayout(&dialog);
    auto* hint = new QLabel(QStringLiteral("Edit comma-separated toolbar tokens. Unknown tokens are rejected."), &dialog);
    layout->addWidget(hint);
    auto* editor = new QPlainTextEdit(&dialog);
    QStringList lines;
    for (const auto key: QT_TOOLBAR_KEYS) {
        const auto* items = baseProfile.itemsFor(key);
        lines.push_back(QStringLiteral("%1=%2")
                                .arg(QString::fromUtf8(key.data(), static_cast<int>(key.size())),
                                     items ? joinToolbarTokens(*items) : QString()));
    }
    editor->setPlainText(lines.join(QStringLiteral("\n")));
    layout->addWidget(editor, 1);
    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
    layout->addWidget(buttons);
    QObject::connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    QObject::connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);

    if (dialog.exec() != QDialog::Accepted) {
        return;
    }

    const std::unordered_set<std::string> knownTokens = {
            "SAVE", "NEW", "OPEN", "SAVEPDF", "PRINT", "CUT", "COPY", "PASTE", "SEARCH", "DELETE", "UNDO",
            "REDO", "GOTO_FIRST", "GOTO_BACK", "NAVIGATE_BACK", "NAVIGATE_FORWARD", "GOTO_NEXT_ANNOTATED_PAGE",
            "GOTO_NEXT", "GOTO_LAST", "INSERT_NEW_PAGE", "DELETE_CURRENT_PAGE", "FULLSCREEN", "AUDIO_RECORDING",
            "AUDIO_SEEK_BACKWARDS", "AUDIO_PAUSE_PLAYBACK", "AUDIO_SEEK_FORWARDS", "AUDIO_STOP_PLAYBACK",
            "SELECT_FONT", "PEN", "PLAIN", "DASHED", "DASH-DOTTED", "DASH-/ DOTTED", "DOTTED", "ERASER",
            "HIGHLIGHTER", "HILIGHTER", "LASER_POINTER", "IMAGE", "TEXT", "MATH_TEX", "DRAW", "DRAW_STROKE",
            "DRAW_VERTEX", "ROTATION_SNAPPING", "GRID_SNAPPING", "VERTEXNOTE_GEOMETRY_SNAPPING",
            "VERTEXNOTE_GRID_SNAPPING", "TOGGLE_TOUCH_DRAWING", "SELECT", "VERTICAL_SPACE", "HAND", "SETSQUARE",
            "COMPASS", "DEFAULT_TOOL", "MANAGE_TOOLBAR", "CUSTOMIZE_TOOLBAR", "GOTO_PAGE", "PDF_TOOL",
            "SELECT_PDF_TEXT_LINEAR", "SELECT_PDF_TEXT_RECT", "SHAPE_RECOGNIZER", "DRAW_RECTANGLE", "DRAW_ELLIPSE",
            "DRAW_ARROW", "DRAW_DOUBLE_ARROW", "DRAW_COORDINATE_SYSTEM", "RULER", "DRAW_SPLINE", "SELECT_REGION",
            "SELECT_RECTANGLE", "SELECT_MULTILAYER_REGION", "SELECT_MULTILAYER_RECTANGLE", "SELECT_OBJECT",
            "PLAY_OBJECT", "GOTO_PREVIOUS_LAYER", "GOTO_NEXT_LAYER", "GOTO_TOP_LAYER", "FILL_OPACITY",
            "GOTO_TOP_LAYER", "GOTO_PREVIOUS_LAYER", "GOTO_NEXT_LAYER", "LAYER", "PAGE_SPIN", "PAIRED_PAGES",
            "PRESENTATION_MODE", "ZOOM_100", "ZOOM_FIT", "ZOOM_OUT", "ZOOM_SLIDER", "ZOOM_IN", "TOOL_FILL",
            "VERY_FINE", "FINE", "MEDIUM", "THICK", "VERY_THICK", "COLOR_SELECT", "CONSTRAINT_COINCIDENT",
            "CONSTRAINT_HORIZONTAL", "CONSTRAINT_VERTICAL", "CONSTRAINT_FIXED_LENGTH", "CONSTRAINT_EDIT_FIXED_LENGTH",
            "CONSTRAINT_PARALLEL", "CONSTRAINT_PERPENDICULAR", "CONSTRAINT_DELETE", "SPACER", "SEPARATOR"};
    const std::unordered_set<std::string> knownToolbarKeys(QT_TOOLBAR_KEYS.begin(), QT_TOOLBAR_KEYS.end());

    QtToolbarProfile customProfile;
    customProfile.id = std::string(QT_CUSTOM_PROFILE_ID);
    customProfile.displayName = std::string(QT_CUSTOM_PROFILE_ID);
    const auto editedLines = editor->toPlainText().split(QLatin1Char('\n'), Qt::SkipEmptyParts);
    for (const auto& rawLine: editedLines) {
        const auto line = rawLine.trimmed();
        if (line.isEmpty() || line.startsWith(QLatin1Char('#'))) {
            continue;
        }
        const qsizetype equalsIndex = line.indexOf(QLatin1Char('='));
        if (equalsIndex <= 0) {
            QMessageBox::warning(&dialog, QStringLiteral("Customize Toolbars"),
                                 QStringLiteral("Each line must be key=tokens."));
            return;
        }
        const auto key = line.left(equalsIndex).trimmed().toLower().toStdString();
        if (!knownToolbarKeys.contains(key)) {
            QMessageBox::warning(&dialog, QStringLiteral("Customize Toolbars"),
                                 QStringLiteral("Unknown toolbar key: %1").arg(QString::fromStdString(key)));
            return;
        }
        auto tokens = splitToolbarTokens(line.mid(equalsIndex + 1));
        for (const auto& token: tokens) {
            const bool colorToken = token.rfind("COLOR(", 0) == 0 && token.ends_with(')');
            if (!colorToken && !knownTokens.contains(token)) {
                QMessageBox::warning(&dialog, QStringLiteral("Customize Toolbars"),
                                     QStringLiteral("Unknown toolbar token: %1").arg(QString::fromStdString(token)));
                return;
            }
        }
        customProfile.toolbars[key] = std::move(tokens);
    }

    saveCustomToolbarProfileToSettings(customProfile);
    this->currentSettings.toolbarProfileId = std::string(QT_CUSTOM_PROFILE_ID);
    this->activeToolbarProfile = std::move(customProfile);
    rebuildToolbar();
    savePersistentUiState();
    this->window.statusBar()->showMessage(QStringLiteral("Custom Qt toolbar profile saved"), 3000);
}

void QtAppShell::showPluginManagerDialog() {
    QDialog dialog(&this->window);
    dialog.setWindowTitle(QStringLiteral("Plugin Manager"));
    auto* layout = new QVBoxLayout(&dialog);
    const auto statuses = this->luaPlugins.statuses();

    auto* table = new QTableWidget(static_cast<int>(statuses.size()), 5, &dialog);
    table->setHorizontalHeaderLabels(
            {QStringLiteral("Enabled"), QStringLiteral("Plugin"), QStringLiteral("Actions"),
             QStringLiteral("Status"), QStringLiteral("Description")});
    table->setSelectionBehavior(QAbstractItemView::SelectRows);
    table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table->verticalHeader()->setVisible(false);
    table->setMinimumSize(760, 360);
    table->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    table->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    table->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    table->horizontalHeader()->setSectionResizeMode(3, QHeaderView::ResizeToContents);
    table->horizontalHeader()->setSectionResizeMode(4, QHeaderView::Stretch);

    for (int row = 0; row < static_cast<int>(statuses.size()); ++row) {
        const auto& status = statuses[static_cast<std::size_t>(row)];
        auto* enabledItem = new QTableWidgetItem();
        enabledItem->setFlags(Qt::ItemIsUserCheckable | Qt::ItemIsEnabled | Qt::ItemIsSelectable);
        enabledItem->setCheckState(status.enabled ? Qt::Checked : Qt::Unchecked);
        enabledItem->setData(Qt::UserRole, QString::fromStdString(status.name));
        table->setItem(row, 0, enabledItem);

        auto* nameItem = new QTableWidgetItem(QString::fromStdString(status.name));
        nameItem->setToolTip(QString::fromStdWString(status.path.wstring()));
        table->setItem(row, 1, nameItem);
        table->setItem(row, 2, new QTableWidgetItem(QString::number(status.registeredActions)));

        QString statusText;
        if (!status.valid) {
            statusText = QStringLiteral("Invalid");
        } else if (!status.enabled) {
            statusText = QStringLiteral("Disabled");
        } else if (!status.error.empty()) {
            statusText = QStringLiteral("Error");
        } else {
            statusText = QStringLiteral("Loaded");
        }
        auto* statusItem = new QTableWidgetItem(statusText);
        if (!status.error.empty()) {
            statusItem->setToolTip(QString::fromStdString(status.error));
        }
        table->setItem(row, 3, statusItem);

        QString description = QString::fromStdString(status.description);
        if (!status.description.empty()) {
            description += QStringLiteral("\n");
        }
        if (!status.version.empty() || !status.author.empty()) {
            description += QStringLiteral("%1 %2")
                                   .arg(QString::fromStdString(status.version), QString::fromStdString(status.author))
                                   .trimmed();
        }
        if (!status.error.empty()) {
            if (!description.isEmpty()) {
                description += QStringLiteral("\n");
            }
            description += QStringLiteral("Error: %1").arg(QString::fromStdString(status.error));
        }
        table->setItem(row, 4, new QTableWidgetItem(description));
    }

    layout->addWidget(table);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Save | QDialogButtonBox::Cancel, &dialog);
    QObject::connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    QObject::connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    layout->addWidget(buttons);
    if (dialog.exec() != QDialog::Accepted) {
        return;
    }

    std::vector<std::pair<std::string, bool>> states;
    states.reserve(static_cast<std::size_t>(table->rowCount()));
    for (int row = 0; row < table->rowCount(); ++row) {
        const auto* item = table->item(row, 0);
        if (!item) {
            continue;
        }
        states.emplace_back(item->data(Qt::UserRole).toString().toStdString(), item->checkState() == Qt::Checked);
    }
    this->luaPlugins.saveEnabledStates(states);
    this->window.statusBar()->showMessage(QStringLiteral("Plugin settings saved"), 3000);
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

void QtAppShell::paperFormatDialog() {
    const auto pageIndex = this->window.canvas()->currentPageIndex();
    if (!this->documentController.hasDocument() || pageIndex >= this->documentController.snapshotPages().size()) {
        return;
    }
    if (!this->documentController.canResizePage(pageIndex)) {
        this->window.statusBar()->showMessage(QStringLiteral("Paper format is fixed for PDF-backed pages"), 3000);
        return;
    }

    const auto& snapshot = this->documentController.snapshotPages()[pageIndex];
    QDialog dialog(&this->window);
    dialog.setWindowTitle(QStringLiteral("Paper Format"));
    auto* rootLayout = new QVBoxLayout(&dialog);
    auto* formLayout = new QFormLayout();

    auto* presetCombo = new QComboBox(&dialog);
    for (const auto& preset: PAPER_PRESETS) {
        presetCombo->addItem(QString::fromUtf8(preset.label.data(), static_cast<int>(preset.label.size())));
    }

    auto* orientationCombo = new QComboBox(&dialog);
    orientationCombo->addItems({QStringLiteral("Portrait"), QStringLiteral("Landscape")});

    auto* widthSpin = new QDoubleSpinBox(&dialog);
    widthSpin->setRange(50.0, 4000.0);
    widthSpin->setDecimals(1);
    widthSpin->setSuffix(QStringLiteral(" pt"));
    widthSpin->setValue(snapshot.width);

    auto* heightSpin = new QDoubleSpinBox(&dialog);
    heightSpin->setRange(50.0, 4000.0);
    heightSpin->setDecimals(1);
    heightSpin->setSuffix(QStringLiteral(" pt"));
    heightSpin->setValue(snapshot.height);

    auto* helpLabel = new QLabel(QStringLiteral("Built-in presets use document points, matching the shared core page model."),
                                 &dialog);
    helpLabel->setWordWrap(true);
    helpLabel->setObjectName(QStringLiteral("paperFormatHelp"));

    formLayout->addRow(QStringLiteral("Preset"), presetCombo);
    formLayout->addRow(QStringLiteral("Orientation"), orientationCombo);
    formLayout->addRow(QStringLiteral("Width"), widthSpin);
    formLayout->addRow(QStringLiteral("Height"), heightSpin);
    rootLayout->addLayout(formLayout);
    rootLayout->addWidget(helpLabel);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
    rootLayout->addWidget(buttons);
    QObject::connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    QObject::connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);

    bool syncingPreset = false;
    auto syncPresetFromSize = [&]() {
        const QSignalBlocker presetBlocker(presetCombo);
        const QSignalBlocker orientationBlocker(orientationCombo);
        presetCombo->setCurrentIndex(matchingPaperPreset(widthSpin->value(), heightSpin->value()));
        orientationCombo->setCurrentIndex(isLandscapeSize(widthSpin->value(), heightSpin->value()) ? 1 : 0);
    };
    auto applyPreset = [&](int presetIndex) {
        if (syncingPreset || presetIndex <= 0 || presetIndex >= static_cast<int>(PAPER_PRESETS.size())) {
            return;
        }
        syncingPreset = true;
        const auto& preset = PAPER_PRESETS[static_cast<std::size_t>(presetIndex)];
        const bool landscape = orientationCombo->currentIndex() == 1;
        widthSpin->setValue(landscape ? preset.height : preset.width);
        heightSpin->setValue(landscape ? preset.width : preset.height);
        syncingPreset = false;
    };

    QObject::connect(presetCombo, qOverload<int>(&QComboBox::currentIndexChanged), &dialog, applyPreset);
    QObject::connect(orientationCombo, qOverload<int>(&QComboBox::currentIndexChanged), &dialog, [&](int) {
        applyPreset(presetCombo->currentIndex());
    });
    QObject::connect(widthSpin, qOverload<double>(&QDoubleSpinBox::valueChanged), &dialog, [&](double) {
        if (!syncingPreset) {
            syncPresetFromSize();
        }
    });
    QObject::connect(heightSpin, qOverload<double>(&QDoubleSpinBox::valueChanged), &dialog, [&](double) {
        if (!syncingPreset) {
            syncPresetFromSize();
        }
    });

    syncPresetFromSize();

    if (dialog.exec() != QDialog::Accepted) {
        return;
    }

    if (!this->documentController.resizePage(pageIndex, widthSpin->value(), heightSpin->value())) {
        return;
    }

    this->window.canvas()->update();
    this->window.pageSidebar()->refresh();
    this->window.layerPanel()->refresh();
    updateEditCommandStates();
    markSessionDirty();
    this->window.statusBar()->showMessage(QStringLiteral("Page size updated"), 3000);
}

void QtAppShell::configurePageTemplateDialog() {
    QDialog dialog(&this->window);
    dialog.setWindowTitle(QStringLiteral("Configure Page Template"));
    auto* rootLayout = new QVBoxLayout(&dialog);
    auto* formLayout = new QFormLayout();

    auto* presetCombo = new QComboBox(&dialog);
    for (const auto& preset: PAPER_PRESETS) {
        presetCombo->addItem(QString::fromUtf8(preset.label.data(), static_cast<int>(preset.label.size())));
    }
    auto* widthSpin = new QDoubleSpinBox(&dialog);
    widthSpin->setRange(50.0, 4000.0);
    widthSpin->setDecimals(1);
    widthSpin->setSuffix(QStringLiteral(" pt"));
    widthSpin->setValue(this->currentSettings.defaultPageWidth);
    auto* heightSpin = new QDoubleSpinBox(&dialog);
    heightSpin->setRange(50.0, 4000.0);
    heightSpin->setDecimals(1);
    heightSpin->setSuffix(QStringLiteral(" pt"));
    heightSpin->setValue(this->currentSettings.defaultPageHeight);
    presetCombo->setCurrentIndex(matchingPaperPreset(widthSpin->value(), heightSpin->value()));
    QObject::connect(presetCombo, qOverload<int>(&QComboBox::currentIndexChanged), &dialog,
                     [widthSpin, heightSpin](int index) {
                         if (index <= 0 || index >= static_cast<int>(PAPER_PRESETS.size())) {
                             return;
                         }
                         const auto& preset = PAPER_PRESETS[static_cast<std::size_t>(index)];
                         widthSpin->setValue(preset.width);
                         heightSpin->setValue(preset.height);
                     });

    formLayout->addRow(QStringLiteral("Preset"), presetCombo);
    formLayout->addRow(QStringLiteral("Default width"), widthSpin);
    formLayout->addRow(QStringLiteral("Default height"), heightSpin);
    rootLayout->addLayout(formLayout);
    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
    rootLayout->addWidget(buttons);
    QObject::connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    QObject::connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);

    if (dialog.exec() != QDialog::Accepted) {
        return;
    }
    this->currentSettings.defaultPageWidth = widthSpin->value();
    this->currentSettings.defaultPageHeight = heightSpin->value();
    savePersistentUiState();
    this->window.statusBar()->showMessage(QStringLiteral("Page template updated"), 3000);
}

auto QtAppShell::dialogInitialDirectory(const std::string& storedPath) const -> QString {
    if (storedPath.empty()) {
        return QString();
    }
    const QFileInfo info(QString::fromStdString(storedPath));
    if (info.exists() && info.isDir()) {
        return info.absoluteFilePath();
    }
    if (info.exists()) {
        return info.absolutePath();
    }
    return QString();
}

void QtAppShell::rememberDialogPath(std::string& storedPath, const QString& filePath) {
    const QFileInfo info(filePath);
    const QString directory = info.isDir() ? info.absoluteFilePath() : info.absolutePath();
    if (!directory.isEmpty()) {
        storedPath = directory.toStdString();
    }
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
