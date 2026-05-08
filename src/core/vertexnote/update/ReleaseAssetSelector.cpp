#include "ReleaseAssetSelector.h"

#include <algorithm>
#include <cctype>
#include <string>

namespace vn::update {
namespace {

auto lower(std::string_view value) -> std::string {
    std::string result(value);
    std::transform(result.begin(), result.end(), result.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return result;
}

auto contains(std::string_view haystack, std::string_view needle) -> bool { return haystack.find(needle) != std::string_view::npos; }

auto scoreAsset(std::string_view name, ReleasePlatform platform) -> int {
    int score = 0;

    switch (platform) {
        case ReleasePlatform::Windows:
            if (contains(name, "windows") || contains(name, "win64") || contains(name, "mingw")) {
                score += 40;
            }
            if (contains(name, ".exe") || contains(name, ".msi")) {
                score += 30;
            } else if (contains(name, ".zip")) {
                score += 10;
            }
            break;
        case ReleasePlatform::Linux:
            if (contains(name, "linux") || contains(name, "ubuntu") || contains(name, "debian")) {
                score += 40;
            }
            if (contains(name, "appimage")) {
                score += 35;
            } else if (contains(name, ".deb") || contains(name, ".tar.gz") || contains(name, ".zip")) {
                score += 15;
            }
            break;
        case ReleasePlatform::Macos:
            if (contains(name, "macos") || contains(name, "darwin") || contains(name, "osx")) {
                score += 40;
            }
            if (contains(name, ".dmg") || contains(name, ".pkg")) {
                score += 30;
            } else if (contains(name, ".zip")) {
                score += 15;
            }
            break;
        case ReleasePlatform::Unknown:
            score += 1;
            break;
    }

    if (contains(name, "x86_64") || contains(name, "amd64") || contains(name, "x64")) {
        score += 5;
    }
    if (contains(name, "source") || contains(name, "src")) {
        score -= 50;
    }
    return score;
}

}  // namespace

auto currentReleasePlatform() -> ReleasePlatform {
#if defined(_WIN32)
    return ReleasePlatform::Windows;
#elif defined(__APPLE__)
    return ReleasePlatform::Macos;
#elif defined(__linux__)
    return ReleasePlatform::Linux;
#else
    return ReleasePlatform::Unknown;
#endif
}

auto selectBestAsset(const ReleaseInfo& release, ReleasePlatform platform) -> std::optional<ReleaseAsset> {
    const ReleaseAsset* best = nullptr;
    int bestScore = 0;

    for (const auto& asset: release.assets) {
        const auto normalized = lower(asset.name);
        const int score = scoreAsset(normalized, platform);
        if (score > bestScore) {
            best = &asset;
            bestScore = score;
        }
    }

    if (!best) {
        return std::nullopt;
    }
    return *best;
}

auto platformName(ReleasePlatform platform) -> std::string_view {
    switch (platform) {
        case ReleasePlatform::Windows:
            return "Windows";
        case ReleasePlatform::Linux:
            return "Linux";
        case ReleasePlatform::Macos:
            return "macOS";
        case ReleasePlatform::Unknown:
            return "this platform";
    }
    return "this platform";
}

}  // namespace vn::update
