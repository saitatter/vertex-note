/*
 * VertexNote
 *
 * Qt app shell window-state signal wiring.
 */

#include "QtAppShell.h"

#include <QComboBox>
#include <QDockWidget>
#include <QDoubleSpinBox>
#include <QFontComboBox>
#include <QSlider>
#include <QSpinBox>
#include <QStatusBar>
#include <QToolBar>

#include "QtPageSidebar.h"
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
                     [this]() {
                         updateEditCommandStates();
                         updateStatusBarLabels();
                     });
    QObject::connect(this->window.canvas(), &QtCanvas::toolStateChanged, &this->window,
                     [this]() {
                         updateToolCommandStates();
                         this->window.toolPalette()->syncFromToolState(this->window.canvas()->toolState());
                         syncToolbarWidgets();
                         updateStatusBarLabels();
                     });
    QObject::connect(this->window.geometryPanel(), &QtGeometryPanel::modelPositionEdited, &this->window,
                     [this](double x, double y, double z) {
                         if (this->window.canvas()->setSelectedGeometryModelCenter(x, y, z)) {
                             updateEditCommandStates();
                             updateStatusBarLabels();
                         }
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
    QObject::connect(this->window.geometryPanel(), &QDockWidget::visibilityChanged, &this->window,
                     [this](bool visible) {
                         if (this->window.geometryPanel()->property("vertexProgrammaticVisibilityChange").toBool()) {
                             return;
                         }
                         this->window.commandHost()->setCommandChecked("view.show-geometry-panel", visible);
                         savePersistentUiState();
                     });

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
