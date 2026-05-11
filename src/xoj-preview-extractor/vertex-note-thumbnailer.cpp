/*
 * VertexNote
 *
 * This small program extracts a preview out of a xoj file
 *
 * @author VertexNote Team
 * https://github.com/saitatter/vertex-note
 *
 * @license GPL
 */

// Set to true to write a log with errors and debug logs to /tmp/xojtmb.log
#define DEBUG_THUMBNAILER false

#include <cstdio>     // for fclose, fopen, fwrite, FILE
#include <iostream>   // for endl, ostream, basic_ostream
#include <locale>     // for locale
#include <string>     // for string, basic_string, allocator

#include <QImage>

#include <glib.h>     // for gchar
#include <libintl.h>  // for bindtextdomain, textdomain

#include "util/PathUtil.h"             // for getLocalePath
#include "util/PlaceholderString.h"    // for PlaceholderString
#include "util/LegacyXojPreviewExtractor.h"  // for LegacyXojPreviewExtractor, PREVIEW_R...
#include "util/i18n.h"                 // for _F, _

#include "config.h"      // for GETTEXT_PACKAGE, ENABLE_NLS
#include "filesystem.h"  // for path, operator/, exists

#ifdef DEBUG_THUMBERNAILER
#include <fstream>
#endif

using std::cerr;
using std::cout;
using std::endl;
using std::string;

void initLocalisation() {
#ifdef ENABLE_NLS
    bindtextdomain(GETTEXT_PACKAGE, char_cast(Util::getLocalePath().u8string().c_str()));
    textdomain(GETTEXT_PACKAGE);
#endif  // ENABLE_NLS

    std::locale::global(std::locale(""));  //"" - system default locale
    std::cout.imbue(std::locale());
}

void logMessage(string msg, bool error) {
    if (error) {
        cerr << msg << endl;
    } else {
        cout << msg << endl;
    }

#if DEBUG_THUMBNAILER
    std::ofstream ofs;
    ofs.open("/tmp/xojtmb.log", std::ofstream::out | std::ofstream::app);

    if (error) {
        ofs << "E: ";
    } else {
        ofs << "I: ";
    }

    ofs << msg << endl;

    ofs.close();
#endif
}

int main(int argc, char* argv[]) {
    initLocalisation();

    // check args count
    if (argc != 3) {
        logMessage(_("xoj-preview-extractor: call with INPUT.xoj OUTPUT.png"), true);
        return 1;
    }

    LegacyXojPreviewExtractor extractor;
    PreviewExtractResult result = extractor.readFile(argv[1]);

    switch (result) {
        case PREVIEW_RESULT_IMAGE_READ:
            // continue to write preview
            break;

        case PREVIEW_RESULT_BAD_FILE_EXTENSION:
            logMessage((_F("xoj-preview-extractor: file \"{1}\" is not .xoj file") % argv[1]).str(), true);
            return 2;

        case PREVIEW_RESULT_COULD_NOT_OPEN_FILE:
            logMessage((_F("xoj-preview-extractor: opening input file \"{1}\" failed") % argv[1]).str(), true);
            return 3;

        case PREVIEW_RESULT_NO_PREVIEW:
            logMessage((_F("xoj-preview-extractor: file \"{1}\" contains no preview") % argv[1]).str(), true);
            return 4;

        case PREVIEW_RESULT_ERROR_READING_PREVIEW:
        default:
            logMessage(_("xoj-preview-extractor: no preview and page found, maybe an invalid file?"), true);
            return 5;
    }


    gsize dataLen = 0;
    unsigned char* imageData = extractor.getData(dataLen);

    QImage thumbnail;
    if (thumbnail.loadFromData(imageData, static_cast<int>(dataLen), "PNG")) {
        if (!thumbnail.save(argv[2], "PNG")) {
            logMessage((_F("xoj-preview-extractor: opening output file \"{1}\" failed") % argv[2]).str(), true);
            return 6;
        }
    } else {
        // If Qt cannot decode the preview, preserve the embedded PNG bytes.
        FILE* fp = fopen(argv[2], "wb");
        if (!fp) {
            logMessage((_F("xoj-preview-extractor: opening output file \"{1}\" failed") % argv[2]).str(), true);
            return 6;
        }
        fwrite(imageData, dataLen, 1, fp);
        fclose(fp);
    }

    logMessage(_("xoj-preview-extractor: successfully extracted"), false);
    return 0;
}
