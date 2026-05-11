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
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QFileDialog>
#include <QSpinBox>
#include <QTabWidget>
#include <QVBoxLayout>

QtSettingsDialog::QtSettingsDialog(const QtSettings& current, const std::vector<QtToolbarProfileOption>& toolbarProfiles,
                                   QWidget* parent):
        QDialog(parent) {
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
    this->eraserModeCombo->addItem(QStringLiteral("Whole Stroke"), static_cast<int>(QtEraserMode::Standard));
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

    this->rotationSnapCheck = new QCheckBox(generalPage);
    this->rotationSnapCheck->setChecked(current.rotationSnapDefault);
    generalLayout->addRow(QStringLiteral("Rotation snap enabled:"), this->rotationSnapCheck);

    this->touchDrawingCheck = new QCheckBox(generalPage);
    this->touchDrawingCheck->setChecked(current.touchDrawingDefault);
    generalLayout->addRow(QStringLiteral("Touch drawing enabled:"), this->touchDrawingCheck);

    this->strokeRecognizerMinSizeSpin = new QDoubleSpinBox(generalPage);
    this->strokeRecognizerMinSizeSpin->setRange(5.0, 500.0);
    this->strokeRecognizerMinSizeSpin->setSingleStep(5.0);
    this->strokeRecognizerMinSizeSpin->setDecimals(1);
    this->strokeRecognizerMinSizeSpin->setValue(current.strokeRecognizerMinSize);
    this->strokeRecognizerMinSizeSpin->setSuffix(QStringLiteral(" pt"));
    generalLayout->addRow(QStringLiteral("Shape recognizer min size:"), this->strokeRecognizerMinSizeSpin);

    this->laserPointerFadeOutSpin = new QSpinBox(generalPage);
    this->laserPointerFadeOutSpin->setRange(100, 10000);
    this->laserPointerFadeOutSpin->setSingleStep(100);
    this->laserPointerFadeOutSpin->setValue(current.laserPointerFadeOutMs);
    this->laserPointerFadeOutSpin->setSuffix(QStringLiteral(" ms"));
    generalLayout->addRow(QStringLiteral("Laser pointer fade-out:"), this->laserPointerFadeOutSpin);

    generalPage->setLayout(generalLayout);
    tabs->addTab(generalPage, QStringLiteral("General"));

    // --- Toolbar tab ---
    auto* toolbarPage = new QWidget(this);
    auto* toolbarLayout = new QFormLayout(toolbarPage);

    this->toolbarProfileCombo = new QComboBox(toolbarPage);
    int currentProfileIndex = -1;
    for (int profileIndex = 0; profileIndex < static_cast<int>(toolbarProfiles.size()); ++profileIndex) {
        const auto& profile = toolbarProfiles[static_cast<std::size_t>(profileIndex)];
        const auto label = profile.displayName.empty() ? profile.id : profile.displayName;
        this->toolbarProfileCombo->addItem(QString::fromStdString(label), QString::fromStdString(profile.id));
        if (profile.id == current.toolbarProfileId) {
            currentProfileIndex = profileIndex;
        }
    }
    if (currentProfileIndex >= 0) {
        this->toolbarProfileCombo->setCurrentIndex(currentProfileIndex);
    }
    toolbarLayout->addRow(QStringLiteral("Toolbar profile:"), this->toolbarProfileCombo);

    toolbarPage->setLayout(toolbarLayout);
    tabs->addTab(toolbarPage, QStringLiteral("Toolbar"));

    // --- PDF tab ---
    auto* pdfPage = new QWidget(this);
    auto* pdfLayout = new QFormLayout(pdfPage);
    this->autoloadPdfXojCheck = new QCheckBox(pdfPage);
    this->autoloadPdfXojCheck->setChecked(current.autoloadPdfXoj);
    pdfLayout->addRow(QStringLiteral("Autoload PDF .xoj:"), this->autoloadPdfXojCheck);
    this->defaultPdfExportNameEdit = new QLineEdit(QString::fromStdString(current.defaultPdfExportName), pdfPage);
    pdfLayout->addRow(QStringLiteral("Default PDF export name:"), this->defaultPdfExportNameEdit);
    pdfPage->setLayout(pdfLayout);
    tabs->addTab(pdfPage, QStringLiteral("PDF"));

    // --- LaTeX tab ---
    auto* latexPage = new QWidget(this);
    auto* latexLayout = new QFormLayout(latexPage);
    auto* latexTemplateRow = new QWidget(latexPage);
    auto* latexTemplateRowLayout = new QHBoxLayout(latexTemplateRow);
    latexTemplateRowLayout->setContentsMargins(0, 0, 0, 0);
    this->latexTemplatePathEdit = new QLineEdit(QString::fromStdString(current.latexTemplatePath), latexTemplateRow);
    auto* browseLatexTemplateButton = new QPushButton(QStringLiteral("Browse..."), latexTemplateRow);
    latexTemplateRowLayout->addWidget(this->latexTemplatePathEdit, 1);
    latexTemplateRowLayout->addWidget(browseLatexTemplateButton, 0);
    QObject::connect(browseLatexTemplateButton, &QPushButton::clicked, this, [this]() {
        const QString selectedPath =
                QFileDialog::getOpenFileName(this, QStringLiteral("Select LaTeX Template"),
                                             this->latexTemplatePathEdit->text().trimmed(),
                                             QStringLiteral("TeX Files (*.tex);;All Files (*)"));
        if (!selectedPath.isEmpty()) {
            this->latexTemplatePathEdit->setText(selectedPath);
        }
    });
    latexLayout->addRow(QStringLiteral("Template path:"), latexTemplateRow);
    latexPage->setLayout(latexLayout);
    tabs->addTab(latexPage, QStringLiteral("LaTeX"));

    // --- Audio tab ---
    auto* audioPage = new QWidget(this);
    auto* audioLayout = new QFormLayout(audioPage);

    auto* audioFolderRow = new QWidget(audioPage);
    auto* audioFolderRowLayout = new QHBoxLayout(audioFolderRow);
    audioFolderRowLayout->setContentsMargins(0, 0, 0, 0);
    this->audioFolderEdit = new QLineEdit(QString::fromStdString(current.audioFolder), audioFolderRow);
    auto* browseAudioFolderButton = new QPushButton(QStringLiteral("Browse..."), audioFolderRow);
    audioFolderRowLayout->addWidget(this->audioFolderEdit, 1);
    audioFolderRowLayout->addWidget(browseAudioFolderButton, 0);
    QObject::connect(browseAudioFolderButton, &QPushButton::clicked, this, [this]() {
        const QString currentPath = this->audioFolderEdit->text().trimmed();
        const QString selectedPath =
                QFileDialog::getExistingDirectory(this, QStringLiteral("Select Audio Folder"), currentPath);
        if (!selectedPath.isEmpty()) {
            this->audioFolderEdit->setText(selectedPath);
        }
    });
    audioLayout->addRow(QStringLiteral("Audio folder:"), audioFolderRow);

    this->audioSampleRateSpin = new QDoubleSpinBox(audioPage);
    this->audioSampleRateSpin->setRange(8000.0, 192000.0);
    this->audioSampleRateSpin->setDecimals(0);
    this->audioSampleRateSpin->setSingleStep(1000.0);
    this->audioSampleRateSpin->setValue(current.audioSampleRate);
    this->audioSampleRateSpin->setSuffix(QStringLiteral(" Hz"));
    audioLayout->addRow(QStringLiteral("Sample rate:"), this->audioSampleRateSpin);

    this->audioGainSpin = new QDoubleSpinBox(audioPage);
    this->audioGainSpin->setRange(0.1, 8.0);
    this->audioGainSpin->setDecimals(2);
    this->audioGainSpin->setSingleStep(0.1);
    this->audioGainSpin->setValue(current.audioGain);
    audioLayout->addRow(QStringLiteral("Playback gain:"), this->audioGainSpin);

    this->defaultSeekTimeSpin = new QSpinBox(audioPage);
    this->defaultSeekTimeSpin->setRange(1, 120);
    this->defaultSeekTimeSpin->setSingleStep(1);
    this->defaultSeekTimeSpin->setValue(current.defaultSeekTimeSeconds);
    this->defaultSeekTimeSpin->setSuffix(QStringLiteral(" s"));
    audioLayout->addRow(QStringLiteral("Seek step:"), this->defaultSeekTimeSpin);

    audioPage->setLayout(audioLayout);
    tabs->addTab(audioPage, QStringLiteral("Audio"));

    // --- Devices tab ---
    auto* devicesPage = new QWidget(this);
    auto* devicesLayout = new QVBoxLayout(devicesPage);
    auto* devicesUnavailableLabel =
            new QLabel(QStringLiteral("Device, stylus, and button configuration is not available in the Qt shell yet."),
                       devicesPage);
    devicesUnavailableLabel->setWordWrap(true);
    devicesLayout->addWidget(devicesUnavailableLabel);
    devicesLayout->addStretch(1);
    devicesPage->setLayout(devicesLayout);
    tabs->addTab(devicesPage, QStringLiteral("Devices"));

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
                                                                            : QtEraserMode::Standard,
            .defaultPageWidth = this->pageWidthSpin->value(),
            .defaultPageHeight = this->pageHeightSpin->value(),
            .undoHistoryLimit = this->undoLimitSpin->value(),
            .geometrySnapDefault = this->geoSnapCheck->isChecked(),
            .gridSnapDefault = this->gridSnapCheck->isChecked(),
            .rotationSnapDefault = this->rotationSnapCheck->isChecked(),
            .touchDrawingDefault = this->touchDrawingCheck->isChecked(),
            .strokeRecognizerMinSize = this->strokeRecognizerMinSizeSpin->value(),
            .laserPointerFadeOutMs = this->laserPointerFadeOutSpin->value(),
            .autoloadPdfXoj = this->autoloadPdfXojCheck->isChecked(),
            .defaultPdfExportName = this->defaultPdfExportNameEdit->text().trimmed().toStdString(),
            .latexTemplatePath = this->latexTemplatePathEdit->text().trimmed().toStdString(),
            .audioFolder = this->audioFolderEdit->text().trimmed().toStdString(),
            .audioSampleRate = this->audioSampleRateSpin->value(),
            .audioGain = this->audioGainSpin->value(),
            .defaultSeekTimeSeconds = this->defaultSeekTimeSpin->value(),
            .toolbarProfileId = this->toolbarProfileCombo->currentData().toString().toStdString(),
    };
}
