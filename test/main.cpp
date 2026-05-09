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

#include <config-test.h>
#include <gtest/gtest.h>

#include "control/VertexNoteMain.h"

class TestEnvironment: public ::testing::Environment {
public:
    virtual void SetUp() {
        std::cout << "Setting up localisation for tests" << std::endl;
        VertexNoteMain::initLocalisation();
    }
};

int main(int argc, char* argv[]) {
    ::testing::InitGoogleTest(&argc, argv);
    // gtest takes ownership of the TestEnvironment ptr - we don't delete it.
    ::testing::AddGlobalTestEnvironment(new TestEnvironment);
    return RUN_ALL_TESTS();
}
