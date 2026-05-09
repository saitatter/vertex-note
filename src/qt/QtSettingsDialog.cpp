/*
 * VertexNote
 *
 * Qt settings/preferences dialog implementation.
 */

#include "QtSettingsDialog.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QLabel>
#include <QSpinBox>
#include <QTabWidget>
#include <QVBoxLayout>

QtSettingsDialog::QtSettingsDialog(const QtSettings& current, QWidget* parent): QDialog(parent) {
    setWindowTitle(QStringLiteral("Preferences"));
    setMinimumWidth(380);

    auto* mainLayout = new QVBoxLayout(this);
    auto* tabs = new QTabWidget(this);

    // --- Tools tab ---
    auto* toolsPage = new QWidget(this);
    auto* toolsLayout = new QFormLayout(toolsPage);

    this->penWidthSpin = new QDoubleSpinBox(toolsPage);
    this->penWidthSpin->setRange(0.1, 50.0);
    this->penWidthSpin->setSingleStep(0.5);
    this->penWidthSpin->setDecimals(2);
    this->penWidthSpin->setValue(current.defaultPenWidth);
    toolsLayout->addRow(QStringLiteral("Default pen width:"), this->penWidthSpin);

    this->highlighterWidthSpin = new QDoubleSpinBox(toolsPage);
    this->highlighterWidthSpin->setRange(0.1, 50.0);
    this->highlighterWidthSpin->setSingleStep(0.5);
    this->highlighterWidthSpin->setDecimals(2);
    this->highlighterWidthSpin->setValue(current.defaultHighlighterWidth);
    toolsLayout->addRow(QStringLiteral("Default highlighter width:"), this->highlighterWidthSpin);

    this->eraserWidthSpin = new QDoubleSpinBox(toolsPage);
    this->eraserWidthSpin->setRange(0.1, 50.0);
    this->eraserWidthSpin->setSingleStep(0.5);
    this->eraserWidthSpin->setDecimals(2);
    this->eraserWidthSpin->setValue(current.defaultEraserWidth);
    toolsLayout->addRow(QStringLiteral("Default eraser width:"), this->eraserWidthSpin);

    this->pressureCheck = new QCheckBox(toolsPage);
    this->pressureCheck->setChecked(current.defaultPressureSensitive);
    toolsLayout->addRow(QStringLiteral("Pressure sensitive by default:"), this->pressureCheck);

    this->eraserModeCombo = new QComboBox(toolsPage);
    this->eraserModeCombo->addItem(QStringLiteral("Whole Stroke"), static_cast<int>(QtEraserMode::WholeStroke));
    this->eraserModeCombo->addItem(QStringLiteral("Segment"), static_cast<int>(QtEraserMode::Segment));
    this->eraserModeCombo->setCurrentIndex(current.defaultEraserMode == QtEraserMode::Segment ? 1 : 0);
    toolsLayout->addRow(QStringLiteral("Default eraser mode:"), this->eraserModeCombo);

    toolsPage->setLayout(toolsLayout);
    tabs->addTab(toolsPage, QStringLiteral("Tools"));

    // --- Page tab ---
    auto* pagePage = new QWidget(this);
    auto* pageLayout = new QFormLayout(pagePage);

    this->pageWidthSpin = new QDoubleSpinBox(pagePage);
    this->pageWidthSpin->setRange(50.0, 5000.0);
    this->pageWidthSpin->setSingleStep(10.0);
    this->pageWidthSpin->setDecimals(1);
    this->pageWidthSpin->setValue(current.defaultPageWidth);
    this->pageWidthSpin->setSuffix(QStringLiteral(" pt"));
    pageLayout->addRow(QStringLiteral("Default page width:"), this->pageWidthSpin);

    this->pageHeightSpin = new QDoubleSpinBox(pagePage);
    this->pageHeightSpin->setRange(50.0, 5000.0);
    this->pageHeightSpin->setSingleStep(10.0);
    this->pageHeightSpin->setDecimals(1);
    this->pageHeightSpin->setValue(current.defaultPageHeight);
    this->pageHeightSpin->setSuffix(QStringLiteral(" pt"));
    pageLayout->addRow(QStringLiteral("Default page height:"), this->pageHeightSpin);

    pagePage->setLayout(pageLayout);
    tabs->addTab(pagePage, QStringLiteral("Page"));

    // --- General tab ---
    auto* generalPage = new QWidget(this);
    auto* generalLayout = new QFormLayout(generalPage);

    this->undoLimitSpin = new QSpinBox(generalPage);
    this->undoLimitSpin->setRange(10, 500);
    this->undoLimitSpin->setValue(current.undoHistoryLimit);
    generalLayout->addRow(QStringLiteral("Undo history limit:"), this->undoLimitSpin);

    this->geoSnapCheck = new QCheckBox(generalPage);
    this->geoSnapCheck->setChecked(current.geometrySnapDefault);
    generalLayout->addRow(QStringLiteral("Geometry snap enabled:"), this->geoSnapCheck);

    this->gridSnapCheck = new QCheckBox(generalPage);
    this->gridSnapCheck->setChecked(current.gridSnapDefault);
    generalLayout->addRow(QStringLiteral("Grid snap enabled:"), this->gridSnapCheck);

    generalPage->setLayout(generalLayout);
    tabs->addTab(generalPage, QStringLiteral("General"));

    mainLayout->addWidget(tabs);

    // Buttons
    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    mainLayout->addWidget(buttons);

    setLayout(mainLayout);
}

auto QtSettingsDialog::settings() const -> QtSettings {
    return {
            .defaultPenWidth = this->penWidthSpin->value(),
            .defaultHighlighterWidth = this->highlighterWidthSpin->value(),
            .defaultEraserWidth = this->eraserWidthSpin->value(),
            .defaultPressureSensitive = this->pressureCheck->isChecked(),
            .defaultEraserMode = this->eraserModeCombo->currentIndex() == 1 ? QtEraserMode::Segment
                                                                            : QtEraserMode::WholeStroke,
            .defaultPageWidth = this->pageWidthSpin->value(),
            .defaultPageHeight = this->pageHeightSpin->value(),
            .undoHistoryLimit = this->undoLimitSpin->value(),
            .geometrySnapDefault = this->geoSnapCheck->isChecked(),
            .gridSnapDefault = this->gridSnapCheck->isChecked(),
    };
}
