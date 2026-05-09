/*
 * VertexNote unit tests
 */

#include <compare>

#include <gtest/gtest.h>

#include "vertexnote/update/GithubReleaseParser.h"
#include "vertexnote/update/VersionComparator.h"

using vn::update::compareVersions;
using vn::update::isUpdateAvailable;
using vn::update::parseGithubRelease;

TEST(VertexNoteReleaseUpdate, comparesSemanticReleaseTags) {
    EXPECT_EQ(compareVersions("0.2.0+dev", "vertex-note-v0.2.0"), std::strong_ordering::equal);
    EXPECT_TRUE(isUpdateAvailable("0.2.0+dev", "vertex-note-v0.3.0"));
    EXPECT_FALSE(isUpdateAvailable("0.3.0+dev", "vertex-note-v0.2.1"));
    EXPECT_TRUE(isUpdateAvailable("vertex-note-v0.9.9", "vertex-note-v1.0.0"));
}

TEST(VertexNoteReleaseUpdate, parsesGithubLatestReleasePayload) {
    constexpr auto payload = R"json({
      "tag_name": "vertex-note-v0.3.0",
      "name": "VertexNote 0.3.0",
      "html_url": "https://github.com/saitatter/vertex-note/releases/tag/vertex-note-v0.3.0",
      "body": "## Added\n- Snap controls\n- Update dialog",
      "draft": false,
      "prerelease": false,
      "assets": [
        {
          "name": "vertex-note-windows.zip",
          "browser_download_url": "https://github.com/saitatter/vertex-note/releases/download/vertex-note-v0.3.0/vertex-note-windows.zip"
        }
      ]
    })json";

    const auto release = parseGithubRelease(payload);
    ASSERT_TRUE(release);
    EXPECT_EQ(release->tagName, "vertex-note-v0.3.0");
    EXPECT_EQ(release->name, "VertexNote 0.3.0");
    EXPECT_FALSE(release->draft);
    EXPECT_FALSE(release->prerelease);
    ASSERT_EQ(release->assets.size(), 1U);
    EXPECT_EQ(release->assets.front().name, "vertex-note-windows.zip");
}

TEST(VertexNoteReleaseUpdate, rejectsPayloadsWithoutReleaseIdentity) {
    EXPECT_FALSE(parseGithubRelease(R"json({"name":"missing tag"})json"));
    EXPECT_FALSE(parseGithubRelease(R"json({"tag_name":"vertex-note-v0.3.0"})json"));
}

TEST(VertexNoteReleaseUpdate, parsesUnicodeEscapesAndIgnoresNestedFieldNames) {
    constexpr auto payload = R"json({
      "metadata": {
        "tag_name": "wrong-tag",
        "html_url": "https://example.invalid/nested"
      },
      "tag_name": "vertex-note-v0.4.0",
      "html_url": "https://github.com/saitatter/vertex-note/releases/tag/vertex-note-v0.4.0",
      "body": "Emoji: \ud83d\ude80 and accent: \u00e9 and literal \"tag_name\": keep this text",
      "draft": false,
      "prerelease": false,
      "assets": []
    })json";

    const auto release = parseGithubRelease(payload);
    ASSERT_TRUE(release);
    EXPECT_EQ(release->tagName, "vertex-note-v0.4.0");
    EXPECT_EQ(release->body,
              std::string("Emoji: ") + "\xF0\x9F\x9A\x80" + " and accent: " + "\xC3\xA9" +
                      " and literal \"tag_name\": keep this text");
}
