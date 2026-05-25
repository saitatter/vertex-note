/*
 * VertexNote
 *
 * Qt toolbar profile storage and compatibility helpers.
 */

#pragma once

#include <array>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include <QString>

#include "QtToolbarLayoutEngine.h"
#include "filesystem.h"

struct QtToolbarProfileOption {
    std::string id;
    std::string displayName;
};

inline constexpr std::string_view QT_GTK_PARITY_PROFILE_ID = "Portrait";
inline constexpr std::string_view QT_GEOMETRY_PROFILE_ID = "Geometry";
inline constexpr std::string_view QT_3D_PROFILE_ID = "3D Modeling";
inline constexpr std::string_view QT_CUSTOM_PROFILE_ID = "Qt Custom";
inline constexpr std::array<std::string_view, 9> QT_TOOLBAR_KEYS = {{
        "toolbartop1", "toolbartop2", "toolbarbottom1", "toolbarleft1", "toolbarleft2",
        "toolbarright1", "toolbarfloat1", "toolbarfloat2", "toolbarfloat3",
}};

[[nodiscard]] auto isGtkParityProfileId(std::string_view profileId) -> bool;
[[nodiscard]] auto toolbarProfilePath() -> fs::path;
[[nodiscard]] auto profileUsesFloatingToolBars(const std::optional<QtToolbarProfile>& profile) -> bool;
[[nodiscard]] auto joinToolbarTokens(const std::vector<std::string>& tokens) -> QString;
[[nodiscard]] auto splitToolbarTokens(const QString& text) -> std::vector<std::string>;
[[nodiscard]] auto customToolbarProfileFromSettings() -> std::optional<QtToolbarProfile>;
void saveCustomToolbarProfileToSettings(const QtToolbarProfile& profile);
[[nodiscard]] auto availableToolbarProfileOptions() -> std::vector<QtToolbarProfileOption>;
