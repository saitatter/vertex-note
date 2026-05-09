/*
 * VertexNote
 *
 * Experimental Qt dialog service.
 */

#pragma once

#include "ui/common/IDialogService.h"

class QWidget;
class QString;

class QtExperimentalDialogService: public vn::ui::common::IDialogService {
public:
    explicit QtExperimentalDialogService(QWidget* parent);

public:
    [[nodiscard]] auto openDocument(const std::vector<vn::ui::common::FileDialogFilter>& filters)
            -> std::optional<std::filesystem::path> override;
    [[nodiscard]] auto saveDocument(const std::filesystem::path& suggestedPath,
                                    const std::vector<vn::ui::common::FileDialogFilter>& filters)
            -> std::optional<std::filesystem::path> override;
    [[nodiscard]] auto confirm(std::string_view title, std::string_view message) -> bool override;
    void showError(std::string_view title, std::string_view message) override;
    void showInfo(std::string_view title, std::string_view message) override;

private:
    [[nodiscard]] auto joinFilters(const std::vector<vn::ui::common::FileDialogFilter>& filters) const -> QString;

private:
    QWidget* parent = nullptr;
};
