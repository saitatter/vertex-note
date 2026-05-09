/*
 * VertexNote
 *
 * Experimental Qt document/session state for the shell migration.
 */

#pragma once

#include <filesystem>
#include <optional>
#include <string>

struct QtExperimentalViewportState {
    double zoom = 1.0;
    double scrollX = 0.0;
    double scrollY = 0.0;
};

class QtExperimentalDocumentSession {
public:
    void newDocument();
    auto openFrom(const std::filesystem::path& path) -> std::optional<QtExperimentalViewportState>;
    auto saveAs(const std::filesystem::path& path, const QtExperimentalViewportState& viewport) -> bool;
    void markDirty(bool dirty);

    [[nodiscard]] auto isDirty() const -> bool;
    [[nodiscard]] auto currentPath() const -> const std::optional<std::filesystem::path>&;
    [[nodiscard]] auto displayName() const -> std::string;
    [[nodiscard]] auto titleText() const -> std::string;

private:
    [[nodiscard]] static auto parseDouble(std::string_view value) -> std::optional<double>;
    static void trim(std::string& value);

private:
    std::optional<std::filesystem::path> path;
    bool dirty = false;
};
