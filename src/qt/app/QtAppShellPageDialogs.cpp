/*
 * VertexNote
 *
 * Qt app shell page and paper dialogs.
 */

#include "QtAppShell.h"

#include <array>
#include <cmath>
#include <cstddef>
#include <string_view>

#include <QComboBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QLabel>
#include <QObject>
#include <QSignalBlocker>
#include <QStatusBar>
#include <QString>
#include <QVBoxLayout>

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

}  // namespace

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
