#include <gtest/gtest.h>

#include "vertexnote/update/ReleaseAssetSelector.h"

using vn::update::ReleaseAsset;
using vn::update::ReleaseInfo;
using vn::update::ReleasePlatform;
using vn::update::selectBestAsset;

TEST(VertexNoteReleaseAssetSelector, selectsWindowsInstaller) {
    ReleaseInfo release;
    release.assets = {
            ReleaseAsset{"vertex-note-source.zip", "source"},
            ReleaseAsset{"vertex-note-linux-x86_64.AppImage", "linux"},
            ReleaseAsset{"vertex-note-windows-x86_64.exe", "windows"},
    };

    const auto asset = selectBestAsset(release, ReleasePlatform::Windows);
    ASSERT_TRUE(asset.has_value());
    EXPECT_EQ(asset->downloadUrl, "windows");
}

TEST(VertexNoteReleaseAssetSelector, prefersLinuxAppImage) {
    ReleaseInfo release;
    release.assets = {
            ReleaseAsset{"vertex-note-linux-x86_64.zip", "zip"},
            ReleaseAsset{"vertex-note-linux-x86_64.AppImage", "appimage"},
    };

    const auto asset = selectBestAsset(release, ReleasePlatform::Linux);
    ASSERT_TRUE(asset.has_value());
    EXPECT_EQ(asset->downloadUrl, "appimage");
}

TEST(VertexNoteReleaseAssetSelector, ignoresSourceOnlyAssets) {
    ReleaseInfo release;
    release.assets = {
            ReleaseAsset{"vertex-note-source.zip", "source"},
    };

    EXPECT_FALSE(selectBestAsset(release, ReleasePlatform::Windows).has_value());
}
