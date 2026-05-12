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

#include <filesystem>

#include <config-test.h>
#include <QStringDecoder>
#include <gtest/gtest.h>

#include "model/Document.h"
#include "model/DocumentHandler.h"
#include "util/StringUtils.h"

namespace {
auto isValidUtf8(std::string_view text) -> bool {
    QStringDecoder decoder(QStringDecoder::Utf8);
    (void)decoder(QByteArrayView(text.data(), static_cast<qsizetype>(text.size())));
    return !decoder.hasError();
}
}  // namespace

TEST(DocumentName, testUTF8) {
    DocumentHandler dh;
    Document doc(&dh);
    fs::path p;
    bool failed = false;
    auto trything = [&](Document::DocumentType t) {
        try {
            p = doc.createSaveFilename(t, u8"%% %Y %EY %B %A", u8"%{name} %Y %EY %B %A");
            std::cout << "Resulting path: " << char_cast(p.u8string()) << std::endl;
            if (!isValidUtf8(char_cast(p.u8string()))) {
                failed = true;
                std::cout << "This path yields an invalid UTF8 string" << std::endl;
            }
        } catch (const std::exception& e) {
            failed = true;
            std::cout << e.what() << std::endl;
        }
    };
    trything(Document::PDF);
    trything(Document::XOPP);
    doc.setFilepath(fs::path(u8"ùèçüûin/ë€ds测试q.xopp"));
    trything(Document::PDF);
    trything(Document::XOPP);
    if (failed) {
        FAIL();
    }
}
