/*
 * VertexNote
 *
 * Fixed input benchmark test of the file loading process
 *
 * @author VertexNote Team
 * https://github.com/vertex-note/vertex-note
 *
 * @license GNU GPLv2 or later
 */

#include <iostream>
#include <chrono>
#include <memory>

#include <config-test.h>
#include <gtest/gtest.h>

#include "control/xojfile/LoadHandler.h"
#include "control/xojfile/SaveHandler.h"
#include "model/Document.h"
#include "model/PageRef.h"
#include "model/NotePage.h"
#include "util/PathUtil.h"

#include "filesystem.h"


static void benchLoadFile(const fs::path& filename, int iterations) {
    const auto start = std::chrono::steady_clock::now();
    for (int i = 0; i < iterations; ++i) {
        LoadHandler{}.loadDocument(filename);
    }
    const auto stop = std::chrono::steady_clock::now();
    const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(stop - start);
    std::cout << "Loaded " << filename << ' ' << iterations << " times in " << elapsed.count() << "ms.\n";
}

TEST(FileLoadBenchmark, benchmarkHandwrittenText) {
    benchLoadFile(GET_TESTFILE(u8"benchmark/handwritten-text.xopp"), 25);
}

TEST(FileLoadBenchmark, benchmarkTypedText) { benchLoadFile(GET_TESTFILE(u8"benchmark/typed-text.xopp"), 5'000); }

TEST(FileLoadBenchmark, benchmarkLatex) { benchLoadFile(GET_TESTFILE(u8"benchmark/latex.xopp"), 50); }

static auto createTemporaryFile(void (*buildDoc)(Document&), const fs::path& filename) -> fs::path {
    // Build file
    DocumentHandler dh;
    Document doc{&dh};
    buildDoc(doc);

    // Save it to a temporary path
    SaveHandler sh;
    auto tmp_path = Util::getTmpDirSubfolder() / filename;
    sh.prepareSave(&doc, tmp_path);
    sh.saveTo(tmp_path);

    return tmp_path;
}

TEST(FileLoadBenchmark, benchmarkEmpty) {
    // Create empty file (containing only one obligatory page)
    const auto tmp_path = createTemporaryFile(
            [](Document& doc) -> void {
                const PageRef page = std::make_shared<NotePage>(50, 50);
                doc.addPage(page);
            },
            u8"empty.xopp");

    // Benchmark loading time
    benchLoadFile(tmp_path, 100'000);

    // Clean up test file
    fs::remove(tmp_path);
}

TEST(FileLoadBenchmark, benchmarkManyPages) {
    // Create a 500-page file
    const auto tmp_path = createTemporaryFile(
            [](Document& doc) -> void {
                for (int i = 0; i < 500; ++i) {
                    const PageRef page = std::make_shared<NotePage>(50, 50);
                    doc.addPage(page);
                }
            },
            u8"many-pages.xopp");

    // Benchmark loading time
    benchLoadFile(tmp_path, 1000);

    // Clean up test file
    fs::remove(tmp_path);
}
