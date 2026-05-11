/*
 * VertexNote
 *
 * Qt app shell toolbar assembly.
 */

#include "QtAppShell.h"

#include <algorithm>
#include <string>
#include <string_view>
#include <unordered_set>
#include <vector>

#include <QAction>
#include <QColor>
#include <QColorDialog>
#include <QDoubleSpinBox>
#include <QFontComboBox>
#include <QMenu>
#include <QObject>
#include <QSize>
#include <QSizePolicy>
#include <QSpinBox>
#include <QString>
#include <QToolBar>
#include <QToolButton>
#include <QWidget>

#include "QtColorPalette.h"
#include "QtIconResources.h"
#include "QtToolFamilies.h"
#include "QtToolbarLayoutEngine.h"
#include "QtToolbarProfileStore.h"

namespace {

auto qColorFromColor(Color color) -> QColor { return QColor(color.red, color.green, color.blue, color.alpha); }

}  // namespace
void QtAppShell::rebuildToolbar() {
    auto* documentToolBar = this->window.mainToolBar();
    auto* toolsToolBar = this->window.toolsToolBar();
    auto* footerToolBar = this->window.footerToolBar();
    auto* leftPrimaryToolBar = this->window.leftPrimaryToolBar();
    auto* leftSecondaryToolBar = this->window.leftSecondaryToolBar();
    auto* rightPrimaryToolBar = this->window.rightPrimaryToolBar();
    documentToolBar->clear();
    toolsToolBar->clear();
    footerToolBar->clear();
    leftPrimaryToolBar->clear();
    leftSecondaryToolBar->clear();
    rightPrimaryToolBar->clear();
    for (auto* floatingToolBar: this->window.floatingToolBars()) {
        floatingToolBar->clear();
    }
    this->selectionToolButton = nullptr;
    this->strokeDrawingToolButtons.clear();
    this->vertexDrawingToolButtons.clear();
    this->laserToolButton = nullptr;
    this->pdfToolButton = nullptr;
    this->fontFamilyCombo = nullptr;
    this->fontSizeSpinner = nullptr;
    this->toolbarFillAction = nullptr;
    this->toolbarColorSelectButton = nullptr;
    this->toolbarColorButtons.clear();
    const auto wantedToolbarProfile =
            this->currentSettings.toolbarProfileId.empty() ? std::string(QT_GTK_PARITY_PROFILE_ID)
                                                           : this->currentSettings.toolbarProfileId;
    this->activeToolbarProfile = wantedToolbarProfile == QT_CUSTOM_PROFILE_ID
                                         ? customToolbarProfileFromSettings()
                                         : QtToolbarLayoutEngine::loadProfile(toolbarProfilePath(), wantedToolbarProfile);
    if (!this->activeToolbarProfile) {
        this->activeToolbarProfile = QtToolbarLayoutEngine::loadProfile(toolbarProfilePath(), QT_GTK_PARITY_PROFILE_ID);
    }

    documentToolBar->setToolButtonStyle(Qt::ToolButtonIconOnly);
    toolsToolBar->setToolButtonStyle(Qt::ToolButtonIconOnly);
    footerToolBar->setToolButtonStyle(Qt::ToolButtonIconOnly);
    leftPrimaryToolBar->setToolButtonStyle(Qt::ToolButtonIconOnly);
    leftSecondaryToolBar->setToolButtonStyle(Qt::ToolButtonIconOnly);
    footerToolBar->setIconSize(QSize(20, 20));
    leftPrimaryToolBar->setIconSize(QSize(22, 22));
    leftSecondaryToolBar->setIconSize(QSize(22, 22));
    rightPrimaryToolBar->setToolButtonStyle(Qt::ToolButtonIconOnly);
    rightPrimaryToolBar->setIconSize(QSize(22, 22));
    for (auto* floatingToolBar: this->window.floatingToolBars()) {
        floatingToolBar->setToolButtonStyle(Qt::ToolButtonIconOnly);
        floatingToolBar->setIconSize(QSize(22, 22));
        floatingToolBar->hide();
    }

    applyQtCommandIcons(this->window);

    const auto addCommand = [&](QToolBar* toolbar, std::string_view id) {
        if (auto* action = this->window.commandHost()->actionForCommand(id)) {
            toolbar->addAction(action);
        }
    };
    const auto addGenericSizeAction = [&](QToolBar* toolbar, const char* text, const char* iconFile, int sizeIndex) {
        auto* action = new QAction(QString::fromUtf8(text), toolbar);
        action->setToolTip(QString::fromUtf8(text));
        action->setIcon(bundledQtIcon(iconFile));
        QObject::connect(action, &QAction::triggered, toolbar, [this, sizeIndex]() {
            switch (this->window.canvas()->activeTool()) {
                case QtToolType::Eraser:
                    setEraserSize(sizeIndex);
                    break;
                case QtToolType::Highlighter:
                    setHighlighterSize(sizeIndex);
                    break;
                default:
                    setPenSize(sizeIndex);
                    break;
            }
        });
        toolbar->addAction(action);
    };
    const auto addFillAction = [&](QToolBar* toolbar) {
        auto* action = new QAction(QStringLiteral("Fill"), toolbar);
        action->setToolTip(QStringLiteral("Toggle fill"));
        action->setCheckable(true);
        action->setIcon(bundledQtIcon("xopp-fill.svg"));
        QObject::connect(action, &QAction::triggered, toolbar, [this, action]() {
            auto& ts = this->window.canvas()->toolState();
            if (this->window.canvas()->activeTool() == QtToolType::Highlighter) {
                ts.highlighterFillEnabled = !ts.highlighterFillEnabled;
                action->setChecked(ts.highlighterFillEnabled);
            } else {
                ts.fillEnabled = !ts.fillEnabled;
                action->setChecked(ts.fillEnabled);
            }
        });
        action->setChecked(this->window.canvas()->toolState().fillEnabled);
        toolbar->addAction(action);
        this->toolbarFillAction = action;
    };
    const auto ensureFillOpacityWidget = [&]() -> QSpinBox* {
        if (!this->fillOpacitySpinner) {
            this->fillOpacitySpinner = new QSpinBox(&this->window);
            this->fillOpacitySpinner->setObjectName(QStringLiteral("vertexNoteQtFillOpacitySpinner"));
            this->fillOpacitySpinner->setRange(0, 255);
            this->fillOpacitySpinner->setSingleStep(8);
            this->fillOpacitySpinner->setPrefix(QStringLiteral("A "));
            this->fillOpacitySpinner->setToolTip(QStringLiteral("Fill opacity"));
            this->fillOpacitySpinner->setFixedWidth(78);
            QObject::connect(this->fillOpacitySpinner, &QSpinBox::valueChanged, &this->window,
                             [this](int value) { setStrokeFill(value); });
        }
        return this->fillOpacitySpinner;
    };

    const auto currentStrokeColor = [&]() -> Color {
        const auto& toolState = this->window.canvas()->toolState();
        return toolState.activeTool == QtToolType::Highlighter ||
                       toolState.activeTool == QtToolType::LaserPointerHighlighter
               ? toolState.highlighterColor
               : toolState.penColor;
    };
    const auto applyToolbarColor = [&](Color color) {
        auto& toolState = this->window.canvas()->toolState();
        if (toolState.activeTool == QtToolType::Highlighter ||
            toolState.activeTool == QtToolType::LaserPointerHighlighter) {
            toolState.highlighterColor = color;
        } else {
            toolState.penColor = color;
        }
        this->window.toolPalette()->syncFromToolState(toolState);
        syncToolbarWidgets();
    };
    const auto toolbarColorAt = [this](int colorIndex) -> Color {
        if (this->activeColorPalette.empty()) {
            const auto palette = qtDefaultColorPalette();
            return palette[static_cast<std::size_t>(colorIndex) % palette.size()].color;
        }
        return this->activeColorPalette[static_cast<std::size_t>(colorIndex) % this->activeColorPalette.size()].color;
    };
    const auto ensureSelectionButton = [&]() -> QToolButton* {
        if (!this->selectionToolButton) {
            this->selectionToolButton = new QToolButton(&this->window);
            this->selectionToolButton->setObjectName(QStringLiteral("vertexNoteQtFamilyToolButton"));
            this->selectionToolButton->setPopupMode(QToolButton::MenuButtonPopup);
            this->selectionToolButton->setIcon(bundledQtIcon("xopp-combo-selection.svg"));
            auto* selectionMenu = new QMenu(this->selectionToolButton);
            for (const auto& spec: selectionToolSpecs()) {
                if (auto* action = this->window.commandHost()->actionForCommand(spec.commandId)) {
                    selectionMenu->addAction(action);
                }
            }
            this->selectionToolButton->setMenu(selectionMenu);
        }
        return this->selectionToolButton;
    };
    const auto ensureStrokeDrawingButton = [&]() -> QToolButton* {
        auto* button = new QToolButton(&this->window);
        button->setObjectName(QStringLiteral("vertexNoteQtFamilyToolButton"));
        button->setPopupMode(QToolButton::MenuButtonPopup);
        button->setIcon(bundledQtIcon("xopp-combo-drawing-type.svg"));
        button->setToolTip(QStringLiteral("Stroke drawing tools"));
        auto* drawingMenu = new QMenu(button);
        for (const auto& spec: strokeDrawingToolSpecs()) {
            if (auto* action = this->window.commandHost()->actionForCommand(spec.commandId)) {
                drawingMenu->addAction(action);
            }
        }
        if (auto* action = this->window.commandHost()->actionForCommand("tool.draw-line")) {
            button->setDefaultAction(action);
        }
        button->setMenu(drawingMenu);
        button->setToolTip(QStringLiteral("Stroke drawing tools"));
        this->strokeDrawingToolButtons.push_back(button);
        return button;
    };
    const auto ensureVertexDrawingButton = [&]() -> QToolButton* {
        auto* button = new QToolButton(&this->window);
        button->setObjectName(QStringLiteral("vertexNoteQtFamilyToolButton"));
        button->setPopupMode(QToolButton::MenuButtonPopup);
        button->setIcon(bundledQtIcon("xopp-draw-coordinate-system.svg"));
        button->setToolTip(QStringLiteral("Vertex drawing tools"));
        auto* drawingMenu = new QMenu(button);
        for (const auto& spec: vertexDrawingToolSpecs()) {
            if (auto* action = this->window.commandHost()->actionForCommand(spec.commandId)) {
                drawingMenu->addAction(action);
            }
        }
        if (auto* action = this->window.commandHost()->actionForCommand("tool.draw-circle")) {
            button->setDefaultAction(action);
        }
        button->setMenu(drawingMenu);
        button->setToolTip(QStringLiteral("Vertex drawing tools"));
        this->vertexDrawingToolButtons.push_back(button);
        return button;
    };
    const auto ensureLaserButton = [&]() -> QToolButton* {
        if (!this->laserToolButton) {
            this->laserToolButton = new QToolButton(&this->window);
            this->laserToolButton->setObjectName(QStringLiteral("vertexNoteQtFamilyToolButton"));
            this->laserToolButton->setPopupMode(QToolButton::MenuButtonPopup);
            this->laserToolButton->setIcon(bundledQtIcon("xopp-laser-pointer.svg"));
            auto* laserMenu = new QMenu(this->laserToolButton);
            for (const auto& spec: laserToolSpecs()) {
                if (auto* action = this->window.commandHost()->actionForCommand(spec.commandId)) {
                    laserMenu->addAction(action);
                }
            }
            this->laserToolButton->setMenu(laserMenu);
        }
        return this->laserToolButton;
    };
    const auto ensurePdfButton = [&]() -> QToolButton* {
        if (!this->pdfToolButton) {
            this->pdfToolButton = new QToolButton(&this->window);
            this->pdfToolButton->setObjectName(QStringLiteral("vertexNoteQtFamilyToolButton"));
            this->pdfToolButton->setPopupMode(QToolButton::MenuButtonPopup);
            this->pdfToolButton->setIcon(bundledQtIcon("xopp-select-pdf-text-ht.svg"));
            auto* pdfMenu = new QMenu(this->pdfToolButton);
            for (const auto& spec: pdfToolSpecs()) {
                if (auto* action = this->window.commandHost()->actionForCommand(spec.commandId)) {
                    pdfMenu->addAction(action);
                }
            }
            this->pdfToolButton->setMenu(pdfMenu);
        }
        return this->pdfToolButton;
    };
    const auto ensureFontWidgets = [&]() {
        if (!this->fontFamilyCombo) {
            this->fontFamilyCombo = new QFontComboBox(&this->window);
            this->fontFamilyCombo->setObjectName(QStringLiteral("vertexNoteQtFontFamilyCombo"));
            this->fontFamilyCombo->setMaximumWidth(140);
            QObject::connect(this->fontFamilyCombo, &QFontComboBox::currentFontChanged, &this->window,
                             [this](const QFont& font) {
                                 this->window.canvas()->toolState().fontName = font.family().toStdString();
                             });
        }
        if (!this->fontSizeSpinner) {
            this->fontSizeSpinner = new QDoubleSpinBox(&this->window);
            this->fontSizeSpinner->setObjectName(QStringLiteral("vertexNoteQtFontSizeSpinner"));
            this->fontSizeSpinner->setRange(6.0, 96.0);
            this->fontSizeSpinner->setDecimals(0);
            this->fontSizeSpinner->setSingleStep(1.0);
            this->fontSizeSpinner->setFixedWidth(56);
            QObject::connect(this->fontSizeSpinner, &QDoubleSpinBox::valueChanged, &this->window,
                             [this](double size) { this->window.canvas()->toolState().fontSize = size; });
        }
    };
    const auto makeColorButton = [&](int colorIndex) -> QToolButton* {
        auto* button = new QToolButton(&this->window);
        button->setAutoRaise(true);
        button->setFixedSize(20, 20);
        button->setProperty("toolbarColorIndex", colorIndex);
        button->setToolTip(QStringLiteral("Quick colour"));
        QObject::connect(button, &QToolButton::clicked, &this->window,
                         [applyToolbarColor, toolbarColorAt, colorIndex]() { applyToolbarColor(toolbarColorAt(colorIndex)); });
        this->toolbarColorButtons.push_back(button);
        return button;
    };
    const auto ensureColorSelectButton = [&]() -> QToolButton* {
        if (!this->toolbarColorSelectButton) {
            this->toolbarColorSelectButton = new QToolButton(&this->window);
            this->toolbarColorSelectButton->setAutoRaise(true);
            this->toolbarColorSelectButton->setFixedSize(21, 21);
            this->toolbarColorSelectButton->setToolTip(QStringLiteral("Choose colour"));
            QObject::connect(this->toolbarColorSelectButton, &QToolButton::clicked, &this->window, [this, currentStrokeColor, applyToolbarColor]() {
                const QColor initial = qColorFromColor(currentStrokeColor());
                const QColor chosen = QColorDialog::getColor(initial, &this->window, QStringLiteral("Stroke Colour"),
                                                             QColorDialog::ShowAlphaChannel);
                if (chosen.isValid()) {
                    applyToolbarColor(Color{static_cast<uint8_t>(chosen.red()), static_cast<uint8_t>(chosen.green()),
                                           static_cast<uint8_t>(chosen.blue()), static_cast<uint8_t>(chosen.alpha())});
                }
            });
        }
        return this->toolbarColorSelectButton;
    };
    const auto addStretchSpacer = [&](QToolBar* toolbar) {
        auto* spacer = new QWidget(toolbar);
        spacer->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
        toolbar->addWidget(spacer);
    };
    const auto addToolbarToken = [&](QToolBar* toolbar, std::string_view rawToken) {
        const std::string token(rawToken);
        if (token == "SEPARATOR") {
            toolbar->addSeparator();
            return;
        }
        if (token == "SPACER") {
            addStretchSpacer(toolbar);
            return;
        }
        if (token == "SAVE") { addCommand(toolbar, "file.save"); return; }
        if (token == "NEW") { addCommand(toolbar, "app.new"); return; }
        if (token == "OPEN") { addCommand(toolbar, "app.open"); return; }
        if (token == "SAVEPDF") { addCommand(toolbar, "export.pdf"); return; }
        if (token == "PRINT") { addCommand(toolbar, "file.print"); return; }
        if (token == "CUT") { addCommand(toolbar, "edit.cut"); return; }
        if (token == "COPY") { addCommand(toolbar, "edit.copy"); return; }
        if (token == "PASTE") { addCommand(toolbar, "edit.paste"); return; }
        if (token == "SEARCH") { addCommand(toolbar, "edit.find"); return; }
        if (token == "DELETE") { addCommand(toolbar, "edit.delete"); return; }
        if (token == "UNDO") { addCommand(toolbar, "edit.undo-geometry"); return; }
        if (token == "REDO") { addCommand(toolbar, "edit.redo-geometry"); return; }
        if (token == "GOTO_FIRST") { addCommand(toolbar, "nav.first-page"); return; }
        if (token == "GOTO_BACK") { addCommand(toolbar, "nav.prev-page"); return; }
        if (token == "NAVIGATE_BACK") { addCommand(toolbar, "nav.back"); return; }
        if (token == "NAVIGATE_FORWARD") { addCommand(toolbar, "nav.forward"); return; }
        if (token == "GOTO_NEXT_ANNOTATED_PAGE") { addCommand(toolbar, "nav.next-annotated"); return; }
        if (token == "GOTO_NEXT") { addCommand(toolbar, "nav.next-page"); return; }
        if (token == "GOTO_LAST") { addCommand(toolbar, "nav.last-page"); return; }
        if (token == "INSERT_NEW_PAGE") { addCommand(toolbar, "page.add"); return; }
        if (token == "DELETE_CURRENT_PAGE") { addCommand(toolbar, "page.delete"); return; }
        if (token == "FULLSCREEN") { addCommand(toolbar, "view.fullscreen"); return; }
        if (token == "AUDIO_RECORDING") { addCommand(toolbar, "audio.record"); return; }
        if (token == "AUDIO_SEEK_BACKWARDS") { addCommand(toolbar, "audio.seek-backwards"); return; }
        if (token == "AUDIO_PAUSE_PLAYBACK") { addCommand(toolbar, "audio.pause-playback"); return; }
        if (token == "AUDIO_SEEK_FORWARDS") { addCommand(toolbar, "audio.seek-forwards"); return; }
        if (token == "AUDIO_STOP_PLAYBACK") { addCommand(toolbar, "audio.stop-playback"); return; }
        if (token == "SELECT_FONT") {
            ensureFontWidgets();
            toolbar->addWidget(this->fontFamilyCombo);
            toolbar->addWidget(this->fontSizeSpinner);
            return;
        }
        if (token == "PEN") { addCommand(toolbar, "tool.pen"); return; }
        if (token == "PLAIN") { addCommand(toolbar, "pen.line-solid"); return; }
        if (token == "DASHED") { addCommand(toolbar, "pen.line-dash"); return; }
        if (token == "DASH-/ DOTTED" || token == "DASH-DOTTED") { addCommand(toolbar, "pen.line-dashdot"); return; }
        if (token == "DOTTED") { addCommand(toolbar, "pen.line-dot"); return; }
        if (token == "ERASER") { addCommand(toolbar, "tool.eraser"); return; }
        if (token == "HIGHLIGHTER" || token == "HILIGHTER") { addCommand(toolbar, "tool.highlighter"); return; }
        if (token == "LASER_POINTER") {
            toolbar->addWidget(ensureLaserButton());
            return;
        }
        if (token == "IMAGE") { addCommand(toolbar, "edit.insert-image"); return; }
        if (token == "TEXT") { addCommand(toolbar, "tool.text"); return; }
        if (token == "MATH_TEX") {
            addCommand(toolbar, "tool.math-tex");
            return;
        }
        if (token == "DRAW") {
            toolbar->addWidget(ensureStrokeDrawingButton());
            toolbar->addWidget(ensureVertexDrawingButton());
            return;
        }
        if (token == "DRAW_STROKE") { toolbar->addWidget(ensureStrokeDrawingButton()); return; }
        if (token == "DRAW_VERTEX") { toolbar->addWidget(ensureVertexDrawingButton()); return; }
        if (token == "ROTATION_SNAPPING") {
            addCommand(toolbar, "view.toggle-rotation-snap");
            return;
        }
        if (token == "GRID_SNAPPING") {
            addCommand(toolbar, "view.toggle-grid-snap");
            return;
        }
        if (token == "VERTEXNOTE_GEOMETRY_SNAPPING") { addCommand(toolbar, "view.toggle-geometry-snap"); return; }
        if (token == "VERTEXNOTE_GRID_SNAPPING") { addCommand(toolbar, "view.toggle-grid-snap"); return; }
        if (token == "SHOW_SIDEBAR") { addCommand(toolbar, "view.show-sidebar"); return; }
        if (token == "TOGGLE_TOUCH_DRAWING") {
            addCommand(toolbar, "view.toggle-touch-drawing");
            return;
        }
        if (token == "SELECT") { toolbar->addWidget(ensureSelectionButton()); return; }
        if (token == "VERTICAL_SPACE") { addCommand(toolbar, "tool.vertical-space"); return; }
        if (token == "HAND") { addCommand(toolbar, "tool.hand"); return; }
        if (token == "SETSQUARE") {
            addCommand(toolbar, "tool.setsquare");
            return;
        }
        if (token == "COMPASS") {
            addCommand(toolbar, "tool.compass");
            return;
        }
        if (token == "DEFAULT_TOOL") {
            addCommand(toolbar, "tool.default-preset");
            return;
        }
        if (token == "MANAGE_TOOLBAR") {
            addCommand(toolbar, "app.settings");
            return;
        }
        if (token == "CUSTOMIZE_TOOLBAR") {
            addCommand(toolbar, "view.customize-toolbar");
            return;
        }
        if (token == "GOTO_PAGE") { addCommand(toolbar, "nav.goto-page"); return; }
        if (token == "SELECT_PDF_TEXT_LINEAR") {
            toolbar->addWidget(ensurePdfButton());
            return;
        }
        if (token == "PDF_TOOL") {
            toolbar->addWidget(ensurePdfButton());
            return;
        }
        if (token == "SELECT_PDF_TEXT_RECT") {
            toolbar->addWidget(ensurePdfButton());
            return;
        }
        if (token == "SHAPE_RECOGNIZER") {
            addCommand(toolbar, "tool.draw-shape-recognizer");
            return;
        }
        if (token == "DRAW_RECTANGLE") { addCommand(toolbar, "tool.draw-rectangle"); return; }
        if (token == "DRAW_ELLIPSE") { addCommand(toolbar, "tool.draw-ellipse"); return; }
        if (token == "DRAW_ARROW") { addCommand(toolbar, "tool.draw-arrow"); return; }
        if (token == "DRAW_DOUBLE_ARROW") { addCommand(toolbar, "tool.draw-double-arrow"); return; }
        if (token == "DRAW_COORDINATE_SYSTEM") { addCommand(toolbar, "tool.draw-coordinate-system"); return; }
        if (token == "RULER") {
            addCommand(toolbar, "tool.draw-line");
            return;
        }
        if (token == "DRAW_SPLINE") { addCommand(toolbar, "tool.draw-spline"); return; }
        if (token == "SELECT_REGION") { addCommand(toolbar, "tool.select-region"); return; }
        if (token == "SELECT_RECTANGLE") { addCommand(toolbar, "tool.select"); return; }
        if (token == "SELECT_MULTILAYER_REGION") { addCommand(toolbar, "tool.select-multilayer-region"); return; }
        if (token == "SELECT_MULTILAYER_RECTANGLE") { addCommand(toolbar, "tool.select-multilayer-rect"); return; }
        if (token == "SELECT_OBJECT") { addCommand(toolbar, "tool.select-object"); return; }
        if (token == "PLAY_OBJECT") { addCommand(toolbar, "audio.play-object"); return; }
        if (token == "GOTO_PREVIOUS_LAYER") { addCommand(toolbar, "layer.goto-prev"); return; }
        if (token == "GOTO_NEXT_LAYER") { addCommand(toolbar, "layer.goto-next"); return; }
        if (token == "GOTO_TOP_LAYER") { addCommand(toolbar, "layer.goto-top"); return; }
        if (token == "FILL_OPACITY" || token == "PEN_FILL_OPACITY") {
            toolbar->addWidget(createStaticIconWidget(toolbar, "xopp-fill-opacity.svg", "Fill opacity"));
            toolbar->addWidget(ensureFillOpacityWidget());
            return;
        }
        if (token == "CONSTRAINT_COINCIDENT") { addCommand(toolbar, "constraint.coincident"); return; }
        if (token == "CONSTRAINT_HORIZONTAL") { addCommand(toolbar, "constraint.horizontal"); return; }
        if (token == "CONSTRAINT_VERTICAL") { addCommand(toolbar, "constraint.vertical"); return; }
        if (token == "CONSTRAINT_FIXED_LENGTH") { addCommand(toolbar, "constraint.fixed-length"); return; }
        if (token == "CONSTRAINT_EDIT_FIXED_LENGTH") { addCommand(toolbar, "constraint.edit-length"); return; }
        if (token == "CONSTRAINT_PARALLEL") { addCommand(toolbar, "constraint.parallel"); return; }
        if (token == "CONSTRAINT_PERPENDICULAR") { addCommand(toolbar, "constraint.perpendicular"); return; }
        if (token == "CONSTRAINT_DELETE") { addCommand(toolbar, "constraint.delete"); return; }
        if (token == "VERY_FINE") { addGenericSizeAction(toolbar, "Very Fine", "xopp-thickness-finer.svg", 0); return; }
        if (token == "FINE") { addGenericSizeAction(toolbar, "Fine", "xopp-thickness-fine.svg", 1); return; }
        if (token == "MEDIUM") { addGenericSizeAction(toolbar, "Medium", "xopp-thickness-medium.svg", 2); return; }
        if (token == "THICK") { addGenericSizeAction(toolbar, "Thick", "xopp-thickness-thick.svg", 3); return; }
        if (token == "VERY_THICK") { addGenericSizeAction(toolbar, "Very Thick", "xopp-thickness-thicker.svg", 4); return; }
        if (token == "TOOL_FILL") { addFillAction(toolbar); return; }
        if (token == "PAGE_SPIN") {
            toolbar->addWidget(createStaticIconWidget(toolbar, "xopp-page-spinner.svg", "Page number"));
            toolbar->addWidget(this->window.footerPageSpin());
            return;
        }
        if (token == "LAYER") {
            toolbar->addWidget(createStaticIconWidget(toolbar, "xopp-combo-layer.svg", "Layer selector"));
            toolbar->addWidget(this->window.footerLayerCombo());
            return;
        }
        if (token == "PAIRED_PAGES") { addCommand(toolbar, "view.paired-pages"); return; }
        if (token == "PRESENTATION_MODE") { addCommand(toolbar, "view.presentation"); return; }
        if (token == "ZOOM_100") { addCommand(toolbar, "view.zoom-100"); return; }
        if (token == "ZOOM_FIT") { addCommand(toolbar, "view.fit-page"); return; }
        if (token == "ZOOM_OUT") { addCommand(toolbar, "view.zoom-out"); return; }
        if (token == "ZOOM_SLIDER") {
            toolbar->addWidget(createStaticIconWidget(toolbar, "xopp-zoom-slider.svg", "Zoom"));
            toolbar->addWidget(this->window.footerZoomSlider());
            return;
        }
        if (token == "ZOOM_IN") { addCommand(toolbar, "view.zoom-in"); return; }
        if (token == "COLOR_SELECT") {
            toolbar->addWidget(ensureColorSelectButton());
            return;
        }
        if (token.rfind("COLOR(", 0) == 0 && token.back() == ')') {
            const auto number = token.substr(6, token.size() - 7);
            const auto index = std::stoi(number);
            if (index >= 0) {
                toolbar->addWidget(makeColorButton(index));
            }
            return;
        }
    };

    const auto toolbarItems = [&](std::string_view key,
                                  std::initializer_list<std::string_view> fallback) -> std::vector<std::string> {
        if (this->activeToolbarProfile) {
            if (const auto* items = this->activeToolbarProfile->itemsFor(key)) {
                return *items;
            }

            return {};
        }

        std::vector<std::string> items;
        items.reserve(fallback.size());
        for (const auto token: fallback) {
            items.emplace_back(token);
        }
        return items;
    };

    auto top1Tokens = toolbarItems("toolbarTop1",
                                   {"SAVE", "NEW", "OPEN", "SEPARATOR", "SAVEPDF", "PRINT", "SEPARATOR",
                                    "CUT", "COPY", "PASTE", "SEPARATOR", "UNDO", "REDO", "SEPARATOR",
                                    "GOTO_FIRST", "GOTO_BACK", "GOTO_NEXT_ANNOTATED_PAGE", "GOTO_NEXT",
                                    "GOTO_LAST", "INSERT_NEW_PAGE", "DELETE_CURRENT_PAGE", "SEPARATOR",
                                    "FULLSCREEN", "SEPARATOR", "AUDIO_RECORDING", "AUDIO_SEEK_BACKWARDS",
                                    "AUDIO_PAUSE_PLAYBACK", "AUDIO_SEEK_FORWARDS", "AUDIO_STOP_PLAYBACK",
                                    "SEPARATOR", "SELECT_FONT"});
    auto top2Tokens = toolbarItems("toolbarTop2",
                                   {"PEN", "ERASER", "HILIGHTER", "LASER_POINTER", "IMAGE", "TEXT",
                                    "MATH_TEX", "DRAW_STROKE", "DRAW_VERTEX", "SEPARATOR", "ROTATION_SNAPPING",
                                    "GRID_SNAPPING", "VERTEXNOTE_GEOMETRY_SNAPPING", "VERTEXNOTE_GRID_SNAPPING",
                                    "TOGGLE_TOUCH_DRAWING", "SEPARATOR", "SELECT", "VERTICAL_SPACE", "HAND",
                                    "SEPARATOR", "DEFAULT_TOOL", "SEPARATOR", "VERY_FINE", "FINE", "MEDIUM",
                                    "THICK", "VERY_THICK", "SEPARATOR", "TOOL_FILL", "SEPARATOR", "COLOR(0)",
                                    "COLOR(1)", "COLOR(2)", "COLOR(3)", "COLOR(4)", "COLOR(5)", "COLOR(6)",
                                    "COLOR(7)", "COLOR(8)", "COLOR(9)", "COLOR(10)", "COLOR_SELECT"});
    auto bottomTokens = toolbarItems("toolbarBottom1",
                                     {"PAGE_SPIN", "SEPARATOR", "LAYER", "SPACER", "PAIRED_PAGES",
                                      "PRESENTATION_MODE", "ZOOM_100", "ZOOM_FIT", "ZOOM_OUT", "ZOOM_SLIDER",
                                      "ZOOM_IN"});
    auto left1Tokens = toolbarItems("toolbarLeft1",
                                    {"SAVEPDF", "PRINT", "SEARCH", "DELETE", "SETSQUARE", "COMPASS",
                                     "MANAGE_TOOLBAR", "CUSTOMIZE_TOOLBAR", "GOTO_PAGE",
                                     "SELECT_PDF_TEXT_LINEAR", "PDF_TOOL", "SELECT_PDF_TEXT_RECT",
                                     "SHAPE_RECOGNIZER", "DRAW_RECTANGLE", "DRAW_ELLIPSE", "DRAW_ARROW",
                                     "DRAW_DOUBLE_ARROW", "DRAW_COORDINATE_SYSTEM", "RULER", "DRAW_SPLINE"});
    auto left2Tokens = toolbarItems("toolbarLeft2",
                                    {"SELECT_REGION", "SELECT_RECTANGLE", "SELECT_OBJECT", "PLAY_OBJECT",
                                     "GOTO_PREVIOUS_LAYER", "GOTO_NEXT_LAYER", "GOTO_TOP_LAYER", "FILL_OPACITY"});
    auto rightToolbarTokens = toolbarItems("toolbarRight1", {});

    std::vector<std::vector<std::string>*> toolbarTokenGroups = {
            &top1Tokens, &top2Tokens, &bottomTokens, &left1Tokens, &left2Tokens, &rightToolbarTokens};
    std::vector<std::vector<std::string>> floatingToolbarTokens;
    floatingToolbarTokens.reserve(this->window.floatingToolBars().size());
    for (int floatingIndex = 0; floatingIndex < static_cast<int>(this->window.floatingToolBars().size()); ++floatingIndex) {
        const auto key = "toolbarfloat" + std::to_string(floatingIndex + 1);
        floatingToolbarTokens.push_back(toolbarItems(key, {}));
        toolbarTokenGroups.push_back(&floatingToolbarTokens.back());
    }

    std::unordered_set<std::string> presentTokens;
    for (const auto* group: toolbarTokenGroups) {
        for (const auto& token: *group) {
            presentTokens.insert(token);
        }
    }

    const bool hasSelectionFamily = presentTokens.contains("SELECT");
    const bool hasDrawingFamily =
            presentTokens.contains("DRAW") || presentTokens.contains("DRAW_STROKE") || presentTokens.contains("DRAW_VERTEX");
    const bool hasPdfFamily = presentTokens.contains("PDF_TOOL");

    const auto pruneRedundantFamilyTokens = [&](std::vector<std::string>& tokens) {
        if (!this->activeToolbarProfile) {
            return;
        }

        auto redundant = [&](const std::string& token) {
            if (hasSelectionFamily &&
                (token == "SELECT_REGION" || token == "SELECT_RECTANGLE" || token == "SELECT_MULTILAYER_REGION" ||
                 token == "SELECT_MULTILAYER_RECTANGLE" || token == "SELECT_OBJECT")) {
                return true;
            }

            if (hasDrawingFamily &&
                (token == "DRAW_RECTANGLE" || token == "DRAW_ELLIPSE" || token == "DRAW_ARROW" ||
                 token == "DRAW_DOUBLE_ARROW" || token == "DRAW_COORDINATE_SYSTEM" || token == "DRAW_SPLINE" ||
                 token == "SHAPE_RECOGNIZER")) {
                return true;
            }

            if (hasPdfFamily && (token == "SELECT_PDF_TEXT_LINEAR" || token == "SELECT_PDF_TEXT_RECT")) {
                return true;
            }

            return false;
        };

        tokens.erase(std::remove_if(tokens.begin(), tokens.end(), redundant), tokens.end());
    };

    for (auto* group: toolbarTokenGroups) {
        *group = QtToolbarLayoutEngine::expandTokenAliases(*group);
        *group = QtToolbarLayoutEngine::normalizeQtDrawingFamilies(*group);
        pruneRedundantFamilyTokens(*group);
    }

    for (const auto& token: top1Tokens) {
        addToolbarToken(documentToolBar, token);
    }

    for (const auto& token: top2Tokens) {
        addToolbarToken(toolsToolBar, token);
    }

    for (const auto& token: bottomTokens) {
        addToolbarToken(footerToolBar, token);
    }

    for (const auto& token: left1Tokens) {
        addToolbarToken(leftPrimaryToolBar, token);
    }

    for (const auto& token: left2Tokens) {
        addToolbarToken(leftSecondaryToolBar, token);
    }

    for (const auto& token: rightToolbarTokens) {
        addToolbarToken(rightPrimaryToolBar, token);
    }

    for (int floatingIndex = 0; floatingIndex < static_cast<int>(this->window.floatingToolBars().size()); ++floatingIndex) {
        auto* floatingToolBar = this->window.floatingToolBars()[static_cast<std::size_t>(floatingIndex)];
        const auto& tokens = floatingToolbarTokens[static_cast<std::size_t>(floatingIndex)];
        for (const auto& token: tokens) {
            addToolbarToken(floatingToolBar, token);
        }

        const bool hasTokens = !tokens.empty();
        if (hasTokens) {
            floatingToolBar->setWindowTitle(QStringLiteral("Floating Toolbar %1").arg(floatingIndex + 1));
        }
    }

    const bool showToolbar = !this->presentationMode &&
                             this->window.commandHost()->actionForCommand("view.show-toolbar") &&
                             this->window.commandHost()->actionForCommand("view.show-toolbar")->isChecked();
    rightPrimaryToolBar->setVisible(!rightToolbarTokens.empty() && showToolbar);
    syncFloatingToolBarsVisibility(showToolbar);
    this->window.cascadeFloatingToolBars();

    syncToolbarWidgets();
    syncFooterWidgets();
}
