/*
 * VertexNote
 *
 * Experimental Qt dialog service.
 */

#include "QtExperimentalDialogService.h"

#include <QFileDialog>
#include <QMessageBox>
#include <QString>
#include <QStringList>

QtExperimentalDialogService::QtExperimentalDialogService(QWidget* parent): parent(parent) {}

auto QtExperimentalDialogService::openDocument(const std::vector<vn::ui::common::FileDialogFilter>& filters)
        -> std::optional<std::filesystem::path> {
    const auto path =
            QFileDialog::getOpenFileName(this->parent, QStringLiteral("Open Document"), QString(), joinFilters(filters));
    if (path.isEmpty()) {
        return std::nullopt;
    }
    return std::filesystem::path(path.toStdWString());
}

auto QtExperimentalDialogService::saveDocument(const std::filesystem::path& suggestedPath,
                                               const std::vector<vn::ui::common::FileDialogFilter>& filters)
        -> std::optional<std::filesystem::path> {
    const auto path = QFileDialog::getSaveFileName(this->parent, QStringLiteral("Save Document"),
                                                   QString::fromStdWString(suggestedPath.wstring()), joinFilters(filters));
    if (path.isEmpty()) {
        return std::nullopt;
    }
    return std::filesystem::path(path.toStdWString());
}

auto QtExperimentalDialogService::confirm(std::string_view title, std::string_view message) -> bool {
    return QMessageBox::question(this->parent, QString::fromUtf8(title.data(), static_cast<int>(title.size())),
                                 QString::fromUtf8(message.data(), static_cast<int>(message.size()))) == QMessageBox::Yes;
}

void QtExperimentalDialogService::showError(std::string_view title, std::string_view message) {
    QMessageBox::critical(this->parent, QString::fromUtf8(title.data(), static_cast<int>(title.size())),
                          QString::fromUtf8(message.data(), static_cast<int>(message.size())));
}

void QtExperimentalDialogService::showInfo(std::string_view title, std::string_view message) {
    QMessageBox::information(this->parent, QString::fromUtf8(title.data(), static_cast<int>(title.size())),
                             QString::fromUtf8(message.data(), static_cast<int>(message.size())));
}

auto QtExperimentalDialogService::joinFilters(const std::vector<vn::ui::common::FileDialogFilter>& filters) const
        -> QString {
    QStringList items;
    for (const auto& filter: filters) {
        QStringList patterns;
        for (const auto& pattern: filter.patterns) {
            patterns << QString::fromStdString(pattern);
        }
        items << QString::fromStdString(filter.label) + " (" + patterns.join(' ') + ")";
    }
    return items.join(";;");
}
