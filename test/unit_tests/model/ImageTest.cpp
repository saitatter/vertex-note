/*
 * VertexNote
 *
 * This file is part of the Xournal UnitTests
 *
 * @author VertexNote Team
 * https://github.com/vertex-note/vertex-note
 *
 * @license GNU GPLv2 or later
 */

#include <fstream>

#include <config-test.h>
#include <gtest/gtest.h>

#include "model/Image.h"

#include "filesystem.h"


TEST(Image, testGetImageApplyOrientation) {
    auto image = Image();

    // Image width is 500px and height 130px - but has exif data saying image should be
    // rotated 90 deg CW to have correct orientation.
    std::ifstream imageFile{fs::path(GET_TESTFILE(u8"images/r90.jpg")), std::ios::binary};
    auto imageData = std::string(std::istreambuf_iterator<char>(imageFile), {});

    auto rotatedImageSize = std::make_pair(130, 500);

    image.setImage(imageData);

    // Test Image object has no size before the image has be rendered
    EXPECT_EQ(image.getImageSize(), Image::NOSIZE);

    // renderBuffer decodes metadata and applies EXIF orientation.
    EXPECT_FALSE(image.renderBuffer().has_value());

    // Test image now have the correct size - which is the image has been rotated.
    EXPECT_EQ(image.getImageSize(), rotatedImageSize);
}
