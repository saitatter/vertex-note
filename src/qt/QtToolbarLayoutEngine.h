/*
 * VertexNote
 *
 * Toolbar profile parser for the Qt shell.
 */

#pragma once

#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include "filesystem.h"

struct QtToolbarProfile {
    std::string id;
    std::string displayName;
    std::unordered_map<std::string, std::vector<std::string>> toolbars;

    [[nodiscard]] auto itemsFor(std::string_view toolbarKey) const -> const std::vector<std::string>*;
};

class QtToolbarLayoutEngine {
public:
    [[nodiscard]] static auto loadProfile(const fs::path& configPath, std::string_view profileId)
            -> std::optional<QtToolbarProfile>;
    [[nodiscard]] static auto loadProfiles(const fs::path& configPath) -> std::vector<QtToolbarProfile>;
};
