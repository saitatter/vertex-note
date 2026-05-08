#pragma once

#include <string>
#include <vector>

namespace vn::update {

struct ReleaseAsset {
    std::string name;
    std::string downloadUrl;
};

struct ReleaseInfo {
    std::string tagName;
    std::string name;
    std::string htmlUrl;
    std::string body;
    bool draft = false;
    bool prerelease = false;
    std::vector<ReleaseAsset> assets;
};

}  // namespace vn::update
