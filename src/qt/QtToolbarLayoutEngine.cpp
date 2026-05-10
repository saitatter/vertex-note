/*
 * VertexNote
 *
 * Toolbar profile parser for the Qt shell.
 */

#include "QtToolbarLayoutEngine.h"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <sstream>

namespace {

auto trim(std::string_view value) -> std::string {
    std::size_t begin = 0;
    while (begin < value.size() && std::isspace(static_cast<unsigned char>(value[begin])) != 0) {
        ++begin;
    }

    std::size_t end = value.size();
    while (end > begin && std::isspace(static_cast<unsigned char>(value[end - 1])) != 0) {
        --end;
    }

    return std::string(value.substr(begin, end - begin));
}

auto lower(std::string_view value) -> std::string {
    std::string result(value);
    std::transform(result.begin(), result.end(), result.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return result;
}

auto splitToolbarItems(std::string_view value) -> std::vector<std::string> {
    std::vector<std::string> items;
    std::string current;
    std::istringstream stream{std::string(value)};
    while (std::getline(stream, current, ',')) {
        auto item = trim(current);
        if (!item.empty()) {
            items.push_back(std::move(item));
        }
    }
    return items;
}

}  // namespace

auto QtToolbarProfile::itemsFor(std::string_view toolbarKey) const -> const std::vector<std::string>* {
    const auto it = this->toolbars.find(lower(toolbarKey));
    return it == this->toolbars.end() ? nullptr : &it->second;
}

auto QtToolbarLayoutEngine::loadProfile(const fs::path& configPath, std::string_view profileId)
        -> std::optional<QtToolbarProfile> {
    for (auto& profile: loadProfiles(configPath)) {
        if (lower(profile.id) == lower(profileId)) {
            return profile;
        }
    }
    return std::nullopt;
}

auto QtToolbarLayoutEngine::loadProfiles(const fs::path& configPath) -> std::vector<QtToolbarProfile> {
    std::ifstream input(configPath);
    if (!input.is_open()) {
        return {};
    }

    std::vector<QtToolbarProfile> profiles;
    std::optional<QtToolbarProfile> currentProfile;

    std::string line;
    while (std::getline(input, line)) {
        const auto trimmed = trim(line);
        if (trimmed.empty() || trimmed.front() == '#') {
            continue;
        }

        if (trimmed.front() == '[' && trimmed.back() == ']') {
            if (currentProfile && !currentProfile->id.empty()) {
                profiles.push_back(std::move(*currentProfile));
            }
            currentProfile = QtToolbarProfile{};
            currentProfile->id = trim(std::string_view(trimmed).substr(1, trimmed.size() - 2));
            continue;
        }

        if (!currentProfile) {
            continue;
        }

        const auto equalsPos = trimmed.find('=');
        if (equalsPos == std::string::npos) {
            continue;
        }

        const auto key = lower(trim(std::string_view(trimmed).substr(0, equalsPos)));
        const auto value = trim(std::string_view(trimmed).substr(equalsPos + 1));
        if (key == "name") {
            currentProfile->displayName = value;
            continue;
        }

        if (key.rfind("toolbar", 0) == 0) {
            currentProfile->toolbars[key] = splitToolbarItems(value);
        }
    }

    if (currentProfile && !currentProfile->id.empty()) {
        profiles.push_back(std::move(*currentProfile));
    }

    return profiles;
}
