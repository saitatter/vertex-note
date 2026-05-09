/*
 * VertexNote
 *
 * Qt app shell bootstrap.
 */

#pragma once

#include "QtClipboardService.h"
#include "QtDialogService.h"
#include "QtDocumentController.h"
#include "QtDocumentExporter.h"
#include "QtDocumentSession.h"
#include "QtMainWindow.h"
#include "QtPluginUiBridge.h"
#include "QtRecentFilesService.h"
#include "QtSettingsDialog.h"
#include "QtToolState.h"
#include "QtUpdatePresentationService.h"
#include "ui/common/IAppShell.h"

class QtAppShell: public vn::ui::common::IAppShell {
public:
    QtAppShell();

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
    void wireWindowState();
    void rebuildToolbar();
    void updateWindowTitle();
    void updateStatusBarLabels();
    void newSession();
    void openSession();
    void saveSessionAs();
    void markSessionDirty();
    void updateEditCommandStates();
    void setGeometrySnapEnabled(bool enabled);
    void setGridSnapEnabled(bool enabled);
    void selectTool(QtToolType tool);
    void updateToolCommandStates();
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
    void showSettingsDialog();
    void applyConstraint(vn::geom::ConstraintKind kind);
    void deleteConstraints();
    void editFixedLengthConstraint();

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

    // Journal extras
    void addPageAtEnd();
    void deleteLayer();
    void paperFormatDialog();

    // Edit extras
    void moveSelectionLayerUp();
    void moveSelectionLayerDown();

private:
    QtMainWindow window;
    QtClipboardService clipboard;
    QtDialogService dialogs;
    QtRecentFilesService recentFiles;
    QtUpdatePresentationService updates;
    QtPluginUiBridge plugins;
    QtDocumentController documentController;
    QtDocumentSession session;
    std::unique_ptr<QtDocumentExporter> exporter;
    bool suppressDirtyTracking = false;
    bool presentationMode = false;
    QtSettings currentSettings;
    std::vector<ElementPtr> elementClipboard;

    // Navigation history
    struct NavPoint {
        std::size_t pageIndex = 0U;
        double scrollX = 0.0;
        double scrollY = 0.0;
        double zoom = 1.0;
    };
    std::vector<NavPoint> navHistory;
    std::size_t navHistoryIndex = 0U;
    void recordNavPoint();
};
