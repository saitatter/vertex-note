#pragma once

#include <optional>
#include <string_view>

#include "ReleaseInfo.h"

namespace vn::update {

auto parseGithubRelease(std::string_view payload) -> std::optional<ReleaseInfo>;

}  // namespace vn::update
