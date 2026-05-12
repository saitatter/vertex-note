/*
 * VertexNote
 *
 * Experimental Qt recent files service.
 */

#include "QtRecentFilesService.h"

#include <algorithm>

void QtRecentFilesService::setRecentFiles(const std::vector<std::filesystem::path>& paths) { this->files = paths; }

void QtRecentFilesService::addRecentFile(const std::filesystem::path& path) {
    this->files.erase(std::remove(this->files.begin(), this->files.end(), path), this->files.end());
    this->files.insert(this->files.begin(), path);
    constexpr std::size_t maxRecent = 10U;
    if (this->files.size() > maxRecent) {
        this->files.resize(maxRecent);
    }
}

auto QtRecentFilesService::recentFiles() const -> std::vector<std::filesystem::path> { return this->files; }
