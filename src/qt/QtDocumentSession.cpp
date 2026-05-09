/*
 * VertexNote
 *
 * Qt document/session state for the shell migration.
 */

#include "QtDocumentSession.h"

#include <charconv>
#include <fstream>
#include <sstream>
#include <string_view>

namespace {

constexpr std::string_view SESSION_HEADER = "vertexnote_qt_session_v1";
constexpr std::string_view LEGACY_SESSION_HEADER = "vertexnote_qt_experimental_session_v1";

}

void QtDocumentSession::newDocument() {
    this->path.reset();
    this->dirty = false;
}

auto QtDocumentSession::openFrom(const std::filesystem::path& path) -> std::optional<QtSessionState> {
    std::ifstream stream(path);
    if (!stream.is_open()) {
        return std::nullopt;
    }

    std::string line;
    if (!std::getline(stream, line)) {
        return std::nullopt;
    }
    trim(line);
    if (line != SESSION_HEADER && line != LEGACY_SESSION_HEADER) {
        return std::nullopt;
    }

    QtSessionState sessionState;
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
                sessionState.viewport.zoom = *parsed;
            }
        } else if (key == "scroll_x") {
            if (auto parsed = parseDouble(value)) {
                sessionState.viewport.scrollX = *parsed;
            }
        } else if (key == "scroll_y") {
            if (auto parsed = parseDouble(value)) {
                sessionState.viewport.scrollY = *parsed;
            }
        } else if (key == "document_path" && !value.empty()) {
            sessionState.linkedDocumentPath = std::filesystem::path(value);
        } else if (key == "document_path_relative" && !value.empty()) {
            sessionState.linkedDocumentPath = path.parent_path() / std::filesystem::path(value);
        }
    }

    this->path = path;
    this->dirty = false;
    return sessionState;
}

auto QtDocumentSession::saveAs(const std::filesystem::path& path, const QtSessionState& sessionState)
        -> bool {
    std::ofstream stream(path, std::ios::trunc);
    if (!stream.is_open()) {
        return false;
    }

    stream << SESSION_HEADER << '\n';
    stream << "zoom=" << sessionState.viewport.zoom << '\n';
    stream << "scroll_x=" << sessionState.viewport.scrollX << '\n';
    stream << "scroll_y=" << sessionState.viewport.scrollY << '\n';
    if (sessionState.linkedDocumentPath) {
        std::error_code error;
        const auto relative = std::filesystem::relative(*sessionState.linkedDocumentPath, path.parent_path(), error);
        if (!error) {
            stream << "document_path_relative=" << relative.string() << '\n';
        } else {
            stream << "document_path=" << sessionState.linkedDocumentPath->string() << '\n';
        }
    }

    if (!stream.good()) {
        return false;
    }

    this->path = path;
    this->dirty = false;
    return true;
}

void QtDocumentSession::markDirty(bool dirty) { this->dirty = dirty; }

auto QtDocumentSession::isDirty() const -> bool { return this->dirty; }

auto QtDocumentSession::currentPath() const -> const std::optional<std::filesystem::path>& { return this->path; }

auto QtDocumentSession::displayName() const -> std::string {
    if (!this->path) {
        return "Untitled Qt Session";
    }
    return this->path->filename().string();
}

auto QtDocumentSession::titleText() const -> std::string {
    return this->dirty ? displayName() + " *" : displayName();
}

auto QtDocumentSession::parseDouble(std::string_view value) -> std::optional<double> {
    double parsed{};
    const auto* begin = value.data();
    const auto* end = value.data() + value.size();
    const auto [ptr, ec] = std::from_chars(begin, end, parsed);
    if (ec != std::errc{} || ptr != end) {
        return std::nullopt;
    }
    return parsed;
}

void QtDocumentSession::trim(std::string& value) {
    while (!value.empty() && (value.front() == ' ' || value.front() == '\t' || value.front() == '\r')) {
        value.erase(value.begin());
    }
    while (!value.empty() && (value.back() == ' ' || value.back() == '\t' || value.back() == '\r')) {
        value.pop_back();
    }
}
