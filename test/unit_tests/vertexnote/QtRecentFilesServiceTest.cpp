#include <gtest/gtest.h>

#include "qt/QtRecentFilesService.h"

TEST(VertexNoteQtRecentFilesService, deduplicatesAndCapsRecentFiles) {
    QtRecentFilesService service;

    service.addRecentFile("c:/docs/alpha.xopp");
    service.addRecentFile("c:/docs/beta.xopp");
    service.addRecentFile("c:/docs/alpha.xopp");

    const auto initial = service.recentFiles();
    ASSERT_EQ(2U, initial.size());
    EXPECT_EQ(std::filesystem::path("c:/docs/alpha.xopp"), initial[0]);
    EXPECT_EQ(std::filesystem::path("c:/docs/beta.xopp"), initial[1]);

    for (int index = 0; index < 20; ++index) {
        service.addRecentFile(std::filesystem::path("c:/docs/file-" + std::to_string(index) + ".xopp"));
    }

    const auto capped = service.recentFiles();
    ASSERT_EQ(10U, capped.size());
    EXPECT_EQ(std::filesystem::path("c:/docs/file-19.xopp"), capped.front());
    EXPECT_EQ(std::filesystem::path("c:/docs/file-10.xopp"), capped.back());
}
