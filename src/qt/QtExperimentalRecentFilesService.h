/*
 * VertexNote
 *
 * Experimental Qt recent files service.
 */

#pragma once

#include "ui/common/IRecentFilesService.h"

class QtExperimentalRecentFilesService: public vn::ui::common::IRecentFilesService {
public:
    void setRecentFiles(const std::vector<std::filesystem::path>& paths) override;
    void addRecentFile(const std::filesystem::path& path) override;
    [[nodiscard]] auto recentFiles() const -> std::vector<std::filesystem::path> override;

private:
    std::vector<std::filesystem::path> files;
};
