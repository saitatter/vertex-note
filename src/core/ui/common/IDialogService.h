/*
 * VertexNote
 *
 * Platform-neutral dialog contract.
 */

#pragma once

#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "UiTypes.h"

namespace vn::ui::common {

class IDialogService {
public:
    virtual ~IDialogService() = default;

    [[nodiscard]] virtual auto openDocument(const std::vector<FileDialogFilter>& filters)
            -> std::optional<std::filesystem::path> = 0;
    [[nodiscard]] virtual auto saveDocument(const std::filesystem::path& suggestedPath,
                                            const std::vector<FileDialogFilter>& filters)
            -> std::optional<std::filesystem::path> = 0;
    [[nodiscard]] virtual auto confirm(std::string_view title, std::string_view message) -> bool = 0;
    virtual void showError(std::string_view title, std::string_view message) = 0;
    virtual void showInfo(std::string_view title, std::string_view message) = 0;
};

}  // namespace vn::ui::common
