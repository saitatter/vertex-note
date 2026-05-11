/*
 * VertexNote
 *
 * Qt toolbar profile storage and compatibility helpers.
 */

#include "QtToolbarProfileStore.h"

#include <string>
#include <utility>

#include <QSettings>
#include <QStringList>

#include "config-paths.h"

auto isGtkParityProfileId(std::string_view profileId) -> bool { return profileId == QT_GTK_PARITY_PROFILE_ID; }

auto toolbarProfilePath() -> fs::path {
    return fs::path(PROJECT_SOURCE_DIR) / "resources-templates" / "toolbar.ini.in";
}

auto profileUsesFloatingToolBars(const std::optional<QtToolbarProfile>& profile) -> bool {
    if (!profile) {
        return false;
    }

    for (int index = 1; index <= 4; ++index) {
        const auto key = "toolbarfloat" + std::to_string(index);
        if (const auto* items = profile->itemsFor(key); items && !items->empty()) {
            return true;
        }
    }

    return false;
}

auto joinToolbarTokens(const std::vector<std::string>& tokens) -> QString {
    QStringList parts;
    for (const auto& token: tokens) {
        parts.push_back(QString::fromStdString(token));
    }
    return parts.join(QStringLiteral(","));
}

auto splitToolbarTokens(const QString& text) -> std::vector<std::string> {
    std::vector<std::string> tokens;
    const auto parts = text.split(QLatin1Char(','), Qt::SkipEmptyParts);
    tokens.reserve(static_cast<std::size_t>(parts.size()));
    for (const auto& part: parts) {
        const auto token = part.trimmed();
        if (!token.isEmpty()) {
            const auto value = token.toStdString();
            tokens.push_back(value == "DRAW_LEGACY" ? std::string("DRAW_STROKE") : value);
        }
    }
    return tokens;
}

auto customToolbarProfileFromSettings() -> std::optional<QtToolbarProfile> {
    QSettings settings(QStringLiteral("VertexNote"), QStringLiteral("VertexNoteQtShell"));
    QtToolbarProfile profile;
    profile.id = std::string(QT_CUSTOM_PROFILE_ID);
    profile.displayName = std::string(QT_CUSTOM_PROFILE_ID);
    bool hasAnyToolbar = false;
    for (const auto key: QT_TOOLBAR_KEYS) {
        const auto value = settings.value(QStringLiteral("toolbar/custom/%1").arg(QString::fromUtf8(key.data(), static_cast<int>(key.size()))))
                                   .toString();
        if (value.trimmed().isEmpty()) {
            continue;
        }
        profile.toolbars[std::string(key)] = splitToolbarTokens(value);
        hasAnyToolbar = true;
    }
    return hasAnyToolbar ? std::optional<QtToolbarProfile>(std::move(profile)) : std::nullopt;
}

void saveCustomToolbarProfileToSettings(const QtToolbarProfile& profile) {
    QSettings settings(QStringLiteral("VertexNote"), QStringLiteral("VertexNoteQtShell"));
    for (const auto key: QT_TOOLBAR_KEYS) {
        const auto* items = profile.itemsFor(key);
        settings.setValue(QStringLiteral("toolbar/custom/%1").arg(QString::fromUtf8(key.data(), static_cast<int>(key.size()))),
                          items ? joinToolbarTokens(*items) : QString());
    }
    settings.sync();
}

auto availableToolbarProfileOptions() -> std::vector<QtToolbarProfileOption> {
    std::vector<QtToolbarProfileOption> options;
    for (const auto& profile: QtToolbarLayoutEngine::loadProfiles(toolbarProfilePath())) {
        options.push_back({.id = profile.id, .displayName = profile.displayName.empty() ? profile.id : profile.displayName});
    }
    options.push_back({.id = std::string(QT_CUSTOM_PROFILE_ID), .displayName = std::string(QT_CUSTOM_PROFILE_ID)});
    return options;
}
