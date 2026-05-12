/*
 * VertexNote
 *
 * Qt settings/preferences dialog implementation.
 */

#include "QtSettingsDialog.h"

#include <QColor>
#include <QDialogButtonBox>
#include <QTabWidget>
#include <QVBoxLayout>

namespace {

auto colorToQColor(Color color) -> QColor { return QColor(color.red, color.green, color.blue, color.alpha); }

}  // namespace

QtSettingsDialog::QtSettingsDialog(const QtSettings& current, const std::vector<QtToolbarProfileOption>& toolbarProfiles,
                                   const std::vector<QtAudioDeviceOption>& audioInputDevices,
                                   const std::vector<QtAudioDeviceOption>& audioOutputDevices,
                                   QWidget* parent):
        QDialog(parent) {
    setWindowTitle(QStringLiteral("Preferences"));
    setMinimumWidth(380);
    this->lastOpenPath = current.lastOpenPath;
    this->lastSavePath = current.lastSavePath;
    this->lastImagePath = current.lastImagePath;
    this->lastPdfPath = current.lastPdfPath;
    this->lastExportPath = current.lastExportPath;
    this->cursorHighlightColor = colorToQColor(current.cursorHighlightColor);
    this->cursorHighlightBorderColor = colorToQColor(current.cursorHighlightBorderColor);

    auto* mainLayout = new QVBoxLayout(this);
    auto* tabs = new QTabWidget(this);

    addToolsTab(tabs, current);
    addPageTab(tabs, current);
    addGeneralTab(tabs, current);
    addAppearanceTab(tabs, current);
    addToolbarTab(tabs, current, toolbarProfiles);
    addPdfTab(tabs, current);
    addLatexTab(tabs, current);
    addAudioTab(tabs, current, audioInputDevices, audioOutputDevices);
    addDevicesTab(tabs, current);
    mainLayout->addWidget(tabs);

    // Buttons
    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    mainLayout->addWidget(buttons);

    setLayout(mainLayout);
}
