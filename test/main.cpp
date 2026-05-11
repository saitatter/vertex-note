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
#include <clocale>
#include <iostream>
#include <locale>
#include <stdexcept>

#include <glib.h>
#include <gtest/gtest.h>

namespace {

void initTestLocalisation() {
    setlocale(LC_NUMERIC, "C");
    try {
        std::locale::global(std::locale(""));
    } catch (const std::runtime_error& e) {
        g_warning("VertexNote tests: System default locale could not be set: %s", e.what());
    }
    std::cout.imbue(std::locale());
}

}  // namespace

class TestEnvironment: public ::testing::Environment {
public:
    virtual void SetUp() {
        std::cout << "Setting up localisation for tests" << std::endl;
        initTestLocalisation();
    }
};

int main(int argc, char* argv[]) {
    ::testing::InitGoogleTest(&argc, argv);
    // gtest takes ownership of the TestEnvironment ptr - we don't delete it.
    ::testing::AddGlobalTestEnvironment(new TestEnvironment);
    return RUN_ALL_TESTS();
}
