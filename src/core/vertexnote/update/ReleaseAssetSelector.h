#pragma once

#include <optional>
#include <string_view>

#include "ReleaseInfo.h"

namespace vn::update {

enum class ReleasePlatform { Windows, Linux, Macos, Unknown };

auto currentReleasePlatform() -> ReleasePlatform;
auto selectBestAsset(const ReleaseInfo& release, ReleasePlatform platform) -> std::optional<ReleaseAsset>;
auto platformName(ReleasePlatform platform) -> std::string_view;

}  // namespace vn::update
