/*
 * VertexNote
 *
 * Platform-neutral recent files contract.
 */

#pragma once

#include <filesystem>
#include <vector>

namespace vn::ui::common {

class IRecentFilesService {
public:
    virtual ~IRecentFilesService() = default;

    virtual void setRecentFiles(const std::vector<std::filesystem::path>& paths) = 0;
    virtual void addRecentFile(const std::filesystem::path& path) = 0;
    [[nodiscard]] virtual auto recentFiles() const -> std::vector<std::filesystem::path> = 0;
};

}  // namespace vn::ui::common
