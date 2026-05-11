/*
 * VertexNote
 *
 * Qt app shell dialogs.
 */

#include "QtAppShell.h"

#include <array>
#include <cmath>
#include <string>
#include <string_view>
#include <unordered_set>
#include <utility>
#include <vector>

#include <QAbstractItemView>
#include <QByteArray>
#include <QComboBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QFileInfo>
#include <QFormLayout>
#include <QHeaderView>
#include <QLabel>
#include <QMessageBox>
#include <QObject>
#include <QPlainTextEdit>
#include <QSignalBlocker>
#include <QStatusBar>
#include <QString>
#include <QStringList>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QVBoxLayout>

#include "QtBackgroundDialog.h"
#include "QtSettingsDialog.h"
#include "QtToolbarLayoutEngine.h"
#include "QtToolbarProfileStore.h"
#include "util/PathUtil.h"

namespace {

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

void applyQtPreferredLocale(const std::string& preferredLocale) {
    qputenv("LANGUAGE", QByteArray(preferredLocale.c_str(), static_cast<qsizetype>(preferredLocale.size())));
}

}  // namespace
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
