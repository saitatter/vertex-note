/*
 * VertexNote
 *
 * Qt app shell toolbar and plugin dialogs.
 */

#include "QtAppShell.h"

#include <cstddef>
#include <string>
#include <unordered_set>
#include <utility>
#include <vector>

#include <QAbstractItemView>
#include <QDialog>
#include <QDialogButtonBox>
#include <QHeaderView>
#include <QLabel>
#include <QMessageBox>
#include <QObject>
#include <QPlainTextEdit>
#include <QStatusBar>
#include <QString>
#include <QStringList>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QVBoxLayout>

#include "QtToolbarLayoutEngine.h"
#include "QtToolbarProfileStore.h"

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
            "GROUP_FILE", "GROUP_EDIT", "GROUP_NAV", "GROUP_PAGE", "GROUP_TOOLS", "GROUP_INSERT",
            "GROUP_DRAW", "GROUP_GEOMETRY", "GROUP_SNAP", "GROUP_STYLE", "GROUP_COLOR",
            "SAVE", "NEW", "OPEN", "SAVEPDF", "PRINT", "CUT", "COPY", "PASTE", "SEARCH", "DELETE", "UNDO",
            "REDO", "GOTO_FIRST", "GOTO_BACK", "NAVIGATE_BACK", "NAVIGATE_FORWARD", "GOTO_NEXT_ANNOTATED_PAGE",
            "GOTO_NEXT", "GOTO_LAST", "INSERT_NEW_PAGE", "DELETE_CURRENT_PAGE", "FULLSCREEN", "AUDIO_RECORDING",
            "AUDIO_SEEK_BACKWARDS", "AUDIO_PAUSE_PLAYBACK", "AUDIO_SEEK_FORWARDS", "AUDIO_STOP_PLAYBACK",
            "SELECT_FONT", "PEN", "PLAIN", "DASHED", "DASH-DOTTED", "DASH-/ DOTTED", "DOTTED", "ERASER",
            "HIGHLIGHTER", "HILIGHTER", "LASER_POINTER", "IMAGE", "TEXT", "MATH_TEX", "DRAW", "DRAW_STROKE",
            "DRAW_VERTEX", "GEOMETRY_SELECT_VERTEX", "GEOMETRY_SELECT_EDGE", "GEOMETRY_SELECT_FACE",
            "GEOMETRY_SELECT_OBJECT",
            "GEOMETRY_TRANSFORM", "VERTEX_TRANSFORM", "ROTATION_SNAPPING", "GRID_SNAPPING", "VERTEXNOTE_GEOMETRY_SNAPPING",
            "VERTEXNOTE_GRID_SNAPPING", "TOGGLE_TOUCH_DRAWING", "SELECT", "VERTICAL_SPACE", "HAND", "SETSQUARE",
            "COMPASS", "DEFAULT_TOOL", "MANAGE_TOOLBAR", "CUSTOMIZE_TOOLBAR", "GOTO_PAGE", "PDF_TOOL",
            "SELECT_PDF_TEXT_LINEAR", "SELECT_PDF_TEXT_RECT", "SHAPE_RECOGNIZER", "DRAW_LINE", "DRAW_RECTANGLE",
            "DRAW_ELLIPSE", "DRAW_ARROW", "DRAW_DOUBLE_ARROW", "DRAW_COORDINATE_SYSTEM", "RULER", "DRAW_SPLINE",
            "DRAW_EDGE", "DRAW_POLYLINE", "SELECT_REGION",
            "SELECT_RECTANGLE", "SELECT_MULTILAYER_REGION", "SELECT_MULTILAYER_RECTANGLE", "SELECT_OBJECT",
            "PLAY_OBJECT", "GOTO_PREVIOUS_LAYER", "GOTO_NEXT_LAYER", "GOTO_TOP_LAYER", "FILL_OPACITY",
            "GOTO_TOP_LAYER", "GOTO_PREVIOUS_LAYER", "GOTO_NEXT_LAYER", "LAYER", "PAGE_SPIN", "PAIRED_PAGES",
            "PRESENTATION_MODE", "ZOOM_100", "ZOOM_FIT", "ZOOM_OUT", "ZOOM_SLIDER", "ZOOM_IN", "TOOL_FILL",
            "VERY_FINE", "FINE", "MEDIUM", "THICK", "VERY_THICK", "COLOR_SELECT", "CONSTRAINT_COINCIDENT",
            "CONSTRAINT_HORIZONTAL", "CONSTRAINT_VERTICAL", "CONSTRAINT_FIXED_LENGTH", "CONSTRAINT_EDIT_FIXED_LENGTH",
            "CONSTRAINT_PARALLEL", "CONSTRAINT_PERPENDICULAR", "CONSTRAINT_DELETE", "GEOMETRY_TRANSLATE",
            "GEOMETRY_ROTATE", "GEOMETRY_SCALE", "GEOMETRY_3D_VERTEX", "GEOMETRY_3D_BOX", "GEOMETRY_VIEW_WIREFRAME",
            "GEOMETRY_VIEW_VERTICES", "GEOMETRY_VIEW_LINKED", "GEOMETRY_VIEW_FACES", "SPACER", "SEPARATOR"};
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
