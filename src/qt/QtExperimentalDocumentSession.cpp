/*
 * VertexNote
 *
 * Experimental Qt document/session state for the shell migration.
 */

#include "QtExperimentalDocumentSession.h"

#include <charconv>
#include <fstream>
#include <sstream>
#include <string_view>

namespace {

constexpr std::string_view SESSION_HEADER = "vertexnote_qt_experimental_session_v1";

}

void QtExperimentalDocumentSession::newDocument() {
    this->path.reset();
    this->dirty = false;
}

auto QtExperimentalDocumentSession::openFrom(const std::filesystem::path& path) -> std::optional<QtExperimentalViewportState> {
    std::ifstream stream(path);
    if (!stream.is_open()) {
        return std::nullopt;
    }

    std::string line;
    if (!std::getline(stream, line)) {
        return std::nullopt;
    }
    trim(line);
    if (line != SESSION_HEADER) {
        return std::nullopt;
    }

    QtExperimentalViewportState viewport;
    while (std::getline(stream, line)) {
        trim(line);
        if (line.empty()) {
            continue;
        }

        const auto split = line.find('=');
        if (split == std::string::npos) {
            continue;
        }

        auto key = line.substr(0, split);
        auto value = line.substr(split + 1);
        trim(key);
        trim(value);

        if (key == "zoom") {
            if (auto parsed = parseDouble(value)) {
                viewport.zoom = *parsed;
            }
        } else if (key == "scroll_x") {
            if (auto parsed = parseDouble(value)) {
                viewport.scrollX = *parsed;
            }
        } else if (key == "scroll_y") {
            if (auto parsed = parseDouble(value)) {
                viewport.scrollY = *parsed;
            }
        }
    }

    this->path = path;
    this->dirty = false;
    return viewport;
}

auto QtExperimentalDocumentSession::saveAs(const std::filesystem::path& path, const QtExperimentalViewportState& viewport)
        -> bool {
    std::ofstream stream(path, std::ios::trunc);
    if (!stream.is_open()) {
        return false;
    }

    stream << SESSION_HEADER << '\n';
    stream << "zoom=" << viewport.zoom << '\n';
    stream << "scroll_x=" << viewport.scrollX << '\n';
    stream << "scroll_y=" << viewport.scrollY << '\n';

    if (!stream.good()) {
        return false;
    }

    this->path = path;
    this->dirty = false;
    return true;
}

void QtExperimentalDocumentSession::markDirty(bool dirty) { this->dirty = dirty; }

auto QtExperimentalDocumentSession::isDirty() const -> bool { return this->dirty; }

auto QtExperimentalDocumentSession::currentPath() const -> const std::optional<std::filesystem::path>& { return this->path; }

auto QtExperimentalDocumentSession::displayName() const -> std::string {
    if (!this->path) {
        return "Untitled Qt Session";
    }
    return this->path->filename().string();
}

auto QtExperimentalDocumentSession::titleText() const -> std::string {
    return this->dirty ? displayName() + " *" : displayName();
}

auto QtExperimentalDocumentSession::parseDouble(std::string_view value) -> std::optional<double> {
    double parsed{};
    const auto* begin = value.data();
    const auto* end = value.data() + value.size();
    const auto [ptr, ec] = std::from_chars(begin, end, parsed);
    if (ec != std::errc{} || ptr != end) {
        return std::nullopt;
    }
    return parsed;
}

void QtExperimentalDocumentSession::trim(std::string& value) {
    while (!value.empty() && (value.front() == ' ' || value.front() == '\t' || value.front() == '\r')) {
        value.erase(value.begin());
    }
    while (!value.empty() && (value.back() == ' ' || value.back() == '\t' || value.back() == '\r')) {
        value.pop_back();
    }
}
