/*
 * VertexNote
 *
 * Qt app shell bootstrap.
 */

#pragma once

#include <filesystem>
#include <initializer_list>
#include <string>
#include <string_view>
#include <vector>

#include <QByteArray>

#include "QtAudioController.h"
#include "QtClipboardService.h"
#include "QtColorPalette.h"
#include "QtDialogService.h"
#include "QtDocumentController.h"
#include "QtDocumentExporter.h"
#include "QtDocumentSession.h"
#include "QtMainWindow.h"
#include "QtLuaPluginRuntime.h"
#include "QtPluginUiBridge.h"
#include "QtRecentFilesService.h"
#include "QtSettingsDialog.h"
#include "QtToolState.h"
#include "QtToolbarLayoutEngine.h"
#include "QtToolbarProfileStore.h"
#include "QtUpdatePresentationService.h"
#include "ui/common/IAppShell.h"

class QDoubleSpinBox;
class QFontComboBox;
class QSpinBox;
class QAction;
class QToolBar;
class QToolButton;
class QTimer;
class QWidget;

class QtAppShell: public vn::ui::common::IAppShell {
public:
    QtAppShell();
    ~QtAppShell() override;

public:
    [[nodiscard]] auto commandHost() -> vn::ui::common::ICommandHost* override;
    [[nodiscard]] auto canvasHost() -> vn::ui::common::ICanvasHost* override;
    [[nodiscard]] auto clipboardService() -> vn::ui::common::IClipboardService* override;
    [[nodiscard]] auto dialogService() -> vn::ui::common::IDialogService* override;
    [[nodiscard]] auto recentFilesService() -> vn::ui::common::IRecentFilesService* override;
    [[nodiscard]] auto updatePresentationService() -> vn::ui::common::IUpdatePresentationService* override;
    [[nodiscard]] auto pluginUiBridge() -> vn::ui::common::IPluginUiBridge* override;
    [[nodiscard]] auto nativeMainWindowHandle() const -> void* override;

    void showMainWindow() override;
    void requestQuit() override;
    void setMainWindowTitle(std::string_view title) override;

private:
    void registerBootstrapCommands();
    void registerFileCommands();
    void registerEditCommands();
    void registerViewCommands();
    void registerNavigationCommands();
    void registerJournalCommands();
    void registerToolCommands();
    void registerHelpCommands();
    void wireWindowState();
    void rebuildToolbar();
    void resetToolbarWidgetState();
    void loadActiveToolbarProfile();
    void configureToolbarChrome();
    [[nodiscard]] auto toolbarItemsFor(std::string_view key, std::initializer_list<std::string_view> fallback) const
            -> std::vector<std::string>;
    void addToolbarToken(QToolBar* toolbar, std::string_view rawToken);
    void addToolbarCommand(QToolBar* toolbar, std::string_view commandId);
    void addGenericSizeToolbarAction(QToolBar* toolbar, const char* text, const char* iconFile, int sizeIndex);
    void addFillToolbarAction(QToolBar* toolbar);
    void addStretchToolbarSpacer(QToolBar* toolbar);
    [[nodiscard]] auto ensureSelectionToolButton() -> QToolButton*;
    [[nodiscard]] auto createStrokeDrawingToolButton() -> QWidget*;
    [[nodiscard]] auto createVertexDrawingToolButton() -> QWidget*;
    [[nodiscard]] auto ensureGeometryTransformButton() -> QToolButton*;
    [[nodiscard]] auto ensureLaserToolButton() -> QToolButton*;
    [[nodiscard]] auto ensurePdfToolButton() -> QToolButton*;
    void ensureFontToolbarWidgets();
    [[nodiscard]] auto ensureFillOpacityWidget() -> QSpinBox*;
    [[nodiscard]] auto makeToolbarColorButton(int colorIndex) -> QToolButton*;
    [[nodiscard]] auto ensureToolbarColorSelectButton() -> QToolButton*;
    [[nodiscard]] auto currentToolbarStrokeColor() const -> Color;
    [[nodiscard]] auto toolbarColorAt(int colorIndex) const -> Color;
    void applyToolbarColor(Color color);
    void rebuildRecentDocumentsMenu();
    void loadPersistentUiState();
    void savePersistentUiState() const;
    void syncFloatingToolBarsVisibility(bool showToolbars);
    void applyAuxiliaryToolBarVisibility(bool showToolbars);
    void applySidebarVisibility(bool visible);
    void syncToolbarWidgets();
    void syncFooterWidgets();
    void updateWindowTitle();
    void updateStatusBarLabels();
    void newSession();
    void openSession();
    void annotatePdf();
    auto openPath(const std::filesystem::path& path, bool fromRecentDocuments) -> bool;
    void saveSessionAs();
    void markSessionDirty();
    void configureAutosave();
    void autosaveNow();
    void checkForUpdates(bool silentWhenCurrent = false);
    void applyRuntimeSettings();
    void applyAppearanceSettings();
    void applySidebarSettings();
    void reloadColorPalette();
    void updateEditCommandStates();
    void setGeometrySnapEnabled(bool enabled);
    void setGridSnapEnabled(bool enabled);
    void selectTool(QtToolType tool);
    void toggleDrawingTool(QtToolType tool);
    void updateToolCommandStates();
    void updateAudioCommandStates();
    void showBackgroundDialog();
    void exportPdf();
    void exportPng();
    void toggleFullscreen();
    void togglePresentationMode();
    void saveDocument();
    void printDocument();
    void addPage();
    void deletePage();
    void duplicatePage();
    void findText();
    void insertImage();
    void insertMathTex();
    void showSettingsDialog();
    void showToolbarCustomizeDialog();
    void showPluginManagerDialog();
    void applyConstraint(vn::geom::ConstraintKind kind);
    void deleteConstraints();
    void editFixedLengthConstraint();
    void translateSelectedVertices();
    void rotateSelectedGeometry();
    void toggleAudioRecording();
    void toggleAudioPausePlayback();
    void stopAudioPlayback();
    void seekAudioBackwards();
    void seekAudioForwards();

    // Phase 7: Clipboard & element operations
    void deleteSelection();
    void selectAll();
    void copySelection();
    void cutSelection();
    void pasteClipboard();

    // Phase 8: Page navigation
    void goToPage(std::size_t pageIndex);
    void goToFirstPage();
    void goToLastPage();
    void goToNextPage();
    void goToPreviousPage();
    void goToPageDialog();

    // Phase 9: Layer operations
    void copyLayer();
    void mergeLayerDown();
    void showAllLayers();
    void hideAllLayers();
    void renameLayerDialog();

    // Phase 10: Page operations
    void addPageBefore();
    void movePageUp();
    void movePageDown();

    // Phase 11: Z-order & zoom
    void bringToFront();
    void sendToBack();
    void bringForward();
    void sendBackward();

    // Phase 12: Pen styling
    void setPenLineStyle(const std::string& style);
    void setStrokeFill(int fillOpacity);
    void setPdfTextMarkerOpacity(int opacity);
    void highlightPdfTextSelection();
    void selectFont();
    void setPenSize(int sizeIndex);
    void setEraserSize(int sizeIndex);
    void setEraserType(QtEraserMode mode);
    void setHighlighterSize(int sizeIndex);

    // Phase 13: Navigation history & layer navigation
    void navigateBack();
    void navigateForward();
    void gotoNextLayer();
    void gotoPrevLayer();
    void gotoTopLayer();
    void addLayerAbove();
    void addLayerBelow();

    // Phase 14: Annotated page navigation
    void gotoNextAnnotatedPage();
    void gotoPrevAnnotatedPage();

    // View & UI toggles
    void togglePairedPages();
    void toggleToolbarVisibility();
    void toggleMenubarVisibility();
    void toggleSidebarVisibility();
    void setLayoutVertical(bool vertical);
    void setLayoutRtl(bool rtl);
    void setLayoutBtt(bool btt);
    void setLayoutColumns(int columns);
    void setLayoutRows(int rows);
    void setPairOffset(int offset);
    void syncLayoutSpanCommandStates();

    // Journal extras
    void addPageAtEnd();
    void appendNewPdfPages();
    void deleteLayer();
    void paperFormatDialog();
    void configurePageTemplateDialog();
    [[nodiscard]] auto dialogInitialDirectory(const std::string& storedPath) const -> QString;
    void rememberDialogPath(std::string& storedPath, const QString& filePath);

    // Edit extras
    void moveSelectionLayerUp();
    void moveSelectionLayerDown();

private:
    QtMainWindow window;
    QtAudioController audioController;
    QtClipboardService clipboard;
    QtDialogService dialogs;
    QtRecentFilesService recentFiles;
    QtUpdatePresentationService updates;
    QtPluginUiBridge plugins;
    QtLuaPluginRuntime luaPlugins;
    QtDocumentController documentController;
    QtDocumentSession session;
    std::unique_ptr<QtDocumentExporter> exporter;
    QTimer* autosaveTimer = nullptr;
    bool suppressDirtyTracking = false;
    bool presentationMode = false;
    QtSettings currentSettings;
    std::vector<QtPaletteColor> activeColorPalette;
    std::vector<ElementPtr> elementClipboard;
    QToolButton* selectionToolButton = nullptr;
    std::vector<QToolButton*> strokeDrawingToolButtons;
    std::vector<QToolButton*> vertexDrawingToolButtons;
    QToolButton* geometryTransformButton = nullptr;
    QToolButton* laserToolButton = nullptr;
    QToolButton* pdfToolButton = nullptr;
    QFontComboBox* fontFamilyCombo = nullptr;
    QDoubleSpinBox* fontSizeSpinner = nullptr;
    QSpinBox* fillOpacitySpinner = nullptr;
    QAction* toolbarFillAction = nullptr;
    QToolButton* toolbarColorSelectButton = nullptr;
    std::vector<QToolButton*> toolbarColorButtons;
    std::optional<QtToolbarProfile> activeToolbarProfile;
    std::vector<QtToolbarProfileOption> availableToolbarProfiles;

    // Navigation history
    struct NavPoint {
        std::size_t pageIndex = 0U;
        double scrollX = 0.0;
        double scrollY = 0.0;
        double zoom = 1.0;
    };
    std::vector<NavPoint> navHistory;
    std::size_t navHistoryIndex = 0U;
    QByteArray persistedWindowGeometry;
    QByteArray persistedWindowState;
    std::vector<QByteArray> persistedFloatingToolBarGeometries;
    std::vector<bool> persistedFloatingToolBarUserHidden;
    bool persistedShowToolbar = true;
    bool persistedShowMenubar = true;
    bool persistedShowSidebar = true;
    bool persistedPairedPages = false;
    int persistedPairOffset = 0;
    int persistedLayoutColumnsRows = 1;
    bool persistedVerticalLayout = true;
    bool persistedLayoutRtl = false;
    bool persistedLayoutBtt = false;
    int pluginSidebarPageNo = 1;
    void recordNavPoint();
    [[nodiscard]] auto selectedElementsForAudioPlayback() const -> std::vector<const Element*>;
};
