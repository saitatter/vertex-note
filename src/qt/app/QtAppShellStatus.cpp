/*
 * VertexNote
 *
 * Qt app shell status and footer synchronization.
 */

#include "QtAppShell.h"

#include <algorithm>
#include <cmath>

#include <QComboBox>
#include <QLabel>
#include <QSignalBlocker>
#include <QSlider>
#include <QSpinBox>
#include <QString>
#include <QVariant>

void QtAppShell::syncFooterWidgets() {
    const auto pageCount = this->documentController.pageCount();
    const auto currentPage = this->window.canvas()->currentPageIndex();

    if (auto* pageSpin = this->window.footerPageSpin()) {
        const QSignalBlocker blocker(pageSpin);
        pageSpin->setMinimum(1);
        pageSpin->setMaximum(static_cast<int>(std::max<std::size_t>(pageCount, 1U)));
        pageSpin->setSuffix(QStringLiteral(" / %1").arg(std::max<std::size_t>(pageCount, 1U)));
        pageSpin->setValue(static_cast<int>(std::min(currentPage + 1, std::max<std::size_t>(pageCount, 1U))));
    }

    if (auto* layerCombo = this->window.footerLayerCombo()) {
        const QSignalBlocker blocker(layerCombo);
        layerCombo->clear();
        if (pageCount > 0) {
            const auto infos = this->documentController.layerInfos(currentPage);
            int currentIndex = -1;
            for (int comboIndex = 0; comboIndex < static_cast<int>(infos.size()); ++comboIndex) {
                const auto& info = infos[static_cast<std::size_t>(comboIndex)];
                layerCombo->addItem(QString::fromStdString(info.name),
                                    QVariant::fromValue(static_cast<qulonglong>(info.index)));
                if (info.selected) {
                    currentIndex = comboIndex;
                }
            }
            if (currentIndex >= 0) {
                layerCombo->setCurrentIndex(currentIndex);
            }
        }
    }

    if (auto* zoomSlider = this->window.footerZoomSlider()) {
        const QSignalBlocker blocker(zoomSlider);
        const auto zoomPercent =
                static_cast<int>(std::round(this->window.canvas()->sessionViewportState().zoom * 100.0));
        zoomSlider->setValue(std::clamp(zoomPercent, zoomSlider->minimum(), zoomSlider->maximum()));
    }
}

void QtAppShell::updateWindowTitle() {
    std::string title = "VertexNote - ";
    if (this->currentSettings.showFilePathInTitlebar && this->documentController.sourcePath()) {
        title += this->documentController.sourcePath()->string();
    } else {
        title += this->documentController.titleText();
    }
    if (this->currentSettings.showPageNumberInTitlebar && this->documentController.pageCount() > 0U) {
        title += " - Page " + std::to_string(this->window.canvas()->currentPageIndex() + 1U) + "/" +
                 std::to_string(this->documentController.pageCount());
    }
    if (this->session.isDirty()) {
        title += " *";
    }
    setMainWindowTitle(title);
}

void QtAppShell::updateStatusBarLabels() {
    const auto pageIdx = this->window.canvas()->currentPageIndex();
    const auto pageCount = this->documentController.pageCount();
    this->window.pageStatusLabel()->setText(
            QStringLiteral("Page %1 of %2").arg(pageIdx + 1).arg(pageCount > 0 ? pageCount : 1));

    if (pageCount > 0) {
        const auto layerIdx = this->documentController.selectedLayerIndex(pageIdx);
        const auto layerCount = this->documentController.layerCount(pageIdx);
        this->window.layerStatusLabel()->setText(QStringLiteral("Layer %1 / %2").arg(layerIdx + 1).arg(layerCount));
    }

    const auto zoom = this->window.canvas()->sessionViewportState().zoom;
    this->window.zoomStatusLabel()->setText(QStringLiteral("%1%").arg(zoom * 100.0, 0, 'f', 0));
}
