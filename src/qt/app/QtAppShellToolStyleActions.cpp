/*
 * VertexNote
 *
 * Qt app shell tool style actions.
 */

#include "QtAppShell.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <string>

#include <QFont>
#include <QFontDialog>
#include <QSignalBlocker>
#include <QStatusBar>
#include <QString>

void QtAppShell::setPenLineStyle(const std::string& style) {
    auto& ts = this->window.canvas()->toolState();
    ts.penLineStyle = style;
    this->window.commandHost()->setCommandChecked("pen.line-solid", style == "plain");
    this->window.commandHost()->setCommandChecked("pen.line-dash", style == "dash");
    this->window.commandHost()->setCommandChecked("pen.line-dashdot", style == "dashdot");
    this->window.commandHost()->setCommandChecked("pen.line-dot", style == "dot");
    this->window.statusBar()->showMessage(QStringLiteral("Line style: %1").arg(QString::fromStdString(style)), 3000);
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

namespace {
constexpr std::array<double, 5> PEN_SIZES = {0.40, 0.85, 1.41, 3.54, 5.00};
constexpr std::array<double, 5> ERASER_SIZES = {3.00, 5.00, 8.50, 14.00, 20.00};
constexpr std::array<double, 5> HIGHLIGHTER_SIZES = {3.00, 5.00, 8.50, 14.00, 20.00};

const std::array<const char*, 5> PEN_SIZE_IDS = {
        "pen.size-very-fine", "pen.size-fine", "pen.size-medium", "pen.size-thick", "pen.size-very-thick"};
const std::array<const char*, 5> ERASER_SIZE_IDS = {
        "eraser.size-very-fine", "eraser.size-fine", "eraser.size-medium", "eraser.size-thick", "eraser.size-very-thick"};
const std::array<const char*, 5> HIGHLIGHTER_SIZE_IDS = {
        "highlighter.size-very-fine", "highlighter.size-fine", "highlighter.size-medium", "highlighter.size-thick",
        "highlighter.size-very-thick"};
}  // namespace

void QtAppShell::setPenSize(int sizeIndex) {
    if (sizeIndex < 0 || sizeIndex >= 5) {
        return;
    }
    this->window.canvas()->toolState().penWidth = PEN_SIZES[static_cast<std::size_t>(sizeIndex)];
    for (int i = 0; i < 5; ++i) {
        this->window.commandHost()->setCommandChecked(PEN_SIZE_IDS[static_cast<std::size_t>(i)], i == sizeIndex);
    }
    this->window.toolPalette()->syncFromToolState(this->window.canvas()->toolState());
}

void QtAppShell::setEraserSize(int sizeIndex) {
    if (sizeIndex < 0 || sizeIndex >= 5) {
        return;
    }
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
    if (sizeIndex < 0 || sizeIndex >= 5) {
        return;
    }
    this->window.canvas()->toolState().highlighterWidth = HIGHLIGHTER_SIZES[static_cast<std::size_t>(sizeIndex)];
    for (int i = 0; i < 5; ++i) {
        this->window.commandHost()->setCommandChecked(HIGHLIGHTER_SIZE_IDS[static_cast<std::size_t>(i)], i == sizeIndex);
    }
    this->window.toolPalette()->syncFromToolState(this->window.canvas()->toolState());
}
