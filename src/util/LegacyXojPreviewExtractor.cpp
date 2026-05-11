#include "util/LegacyXojPreviewExtractor.h"

#include <array>    // for array
#include <cctype>   // for isspace
#include <cstring>  // for strlen, strncmp
#include <string>   // for allocator, string
#include <string_view>  // for string_view
#include <vector>   // for vector

#include <zip.h>      // for zip_close, zip_fclose, zip_stat_t, zip_fopen
#include <zipconf.h>  // for zip_int64_t, zip_uint64_t
#include <zlib.h>     // for gzclose, gzread, gzFile

#include "util/GzUtil.h"      // for GzUtil
#include "util/PathUtil.h"    // for hasXournalFileExt
#include "util/StringUtils.h"  // for char_cast
#include "util/safe_casts.h"  // for as_signed

#include "filesystem.h"  // for path

const char* TAG_PREVIEW_NAME = "preview";
const size_t TAG_PREVIEW_NAME_LEN = strlen(TAG_PREVIEW_NAME);
const char* TAG_PAGE_NAME = "page";
const size_t TAG_PAGE_NAME_LEN = strlen(TAG_PAGE_NAME);
const char* TAG_PREVIEW_END_NAME = "/preview";
const size_t TAG_PREVIEW_END_NAME_LEN = strlen(TAG_PREVIEW_END_NAME);
// max png size is: (1.02*(3*128+1)*128)+68 approx 50334
// see https://stackoverflow.com/a/22507715/2907484
// max base64-overhead is ceil(50334/3)*4 = 67112
// see https://stackoverflow.com/a/4715480/2907484
// round it up a bit
constexpr auto BUF_SIZE = 68000;

namespace {
auto decodeBase64(std::string_view encoded) -> std::vector<unsigned char> {
    constexpr std::array signedLookup = [] {
        std::array<int, 256> table{};
        table.fill(-1);
        for (int i = 0; i < 26; ++i) {
            table[static_cast<std::size_t>('A' + i)] = i;
            table[static_cast<std::size_t>('a' + i)] = i + 26;
        }
        for (int i = 0; i < 10; ++i) { table[static_cast<std::size_t>('0' + i)] = i + 52; }
        table[static_cast<std::size_t>('+')] = 62;
        table[static_cast<std::size_t>('/')] = 63;
        return table;
    }();

    std::vector<unsigned char> decoded;
    decoded.reserve((encoded.size() / 4U) * 3U);

    int value = 0;
    int bits = -8;
    for (const unsigned char ch: encoded) {
        if (std::isspace(ch)) {
            continue;
        }
        if (ch == '=') {
            break;
        }
        const int digit = signedLookup[ch];
        if (digit < 0) {
            continue;
        }
        value = (value << 6U) + digit;
        bits += 6;
        if (bits >= 0) {
            decoded.push_back(static_cast<unsigned char>((value >> bits) & 0xFF));
            bits -= 8;
        }
    }

    return decoded;
}
}  // namespace

LegacyXojPreviewExtractor::LegacyXojPreviewExtractor() = default;

LegacyXojPreviewExtractor::~LegacyXojPreviewExtractor() = default;

/**
 * @return The preview data, should be a binary PNG
 */
auto LegacyXojPreviewExtractor::getData(size_t& dataLen) const -> const unsigned char* {
    dataLen = this->data.size();
    return this->data.data();
}

/**
 * Try to read the preview from byte buffer
 * @param buffer Buffer
 * @param len Buffer len
 * @return If an image was read, or the error
 */
auto LegacyXojPreviewExtractor::readPreview(char* buffer, int len) -> PreviewExtractResult {
    bool inTag = false;
    int startTag = 0;
    int startPreview = -1;
    int endPreview = -1;
    int pageStart = -1;
    for (int i = 0; i < len; i++) {
        if (inTag) {
            if (buffer[i] == '>') {
                inTag = false;
                int tagLen = i - startTag;
                if (tagLen == as_signed(TAG_PREVIEW_NAME_LEN) &&
                    strncmp(TAG_PREVIEW_NAME, buffer + startTag, TAG_PREVIEW_NAME_LEN) == 0) {
                    startPreview = i + 1;
                }
                if (tagLen == as_signed(TAG_PREVIEW_END_NAME_LEN) &&
                    strncmp(TAG_PREVIEW_END_NAME, buffer + startTag, TAG_PREVIEW_END_NAME_LEN) == 0) {
                    endPreview = i - static_cast<int>(TAG_PREVIEW_END_NAME_LEN) - 1;
                    break;
                }
                if (tagLen >= as_signed(TAG_PAGE_NAME_LEN) &&
                    strncmp(TAG_PAGE_NAME, buffer + startTag, TAG_PAGE_NAME_LEN) == 0) {
                    pageStart = i;
                    break;
                }
            }
            continue;
        }

        if (buffer[i] == '<') {
            inTag = true;
            startTag = i + 1;
            continue;
        }
    }

    if (startPreview != -1 && endPreview != -1) {
        this->data = decodeBase64(std::string_view(buffer + startPreview, static_cast<size_t>(endPreview - startPreview)));
        return PREVIEW_RESULT_IMAGE_READ;
    }

    if (pageStart != -1) {
        return PREVIEW_RESULT_NO_PREVIEW;
    }

    return PREVIEW_RESULT_ERROR_READING_PREVIEW;
}

/**
 * Try to read the preview from file
 * @param file .xoj File
 * @return true if a preview was read, false if not
 */
auto LegacyXojPreviewExtractor::readFile(const fs::path& file) -> PreviewExtractResult {
    // check file extensions
    if (!Util::hasXournalFileExt(file)) {
        return PREVIEW_RESULT_BAD_FILE_EXTENSION;
    }
    // read the new file format
    int zipError = 0;
    zip_t* zipFp = zip_open(char_cast(file.u8string().c_str()), ZIP_RDONLY, &zipError);

    if (!zipFp && zipError == ZIP_ER_NOZIP) {
        gzFile fp = GzUtil::openPath(file, "r");
        if (!fp) {
            return PREVIEW_RESULT_COULD_NOT_OPEN_FILE;
        }

        // The <preview> Tag is within the first 179 Bytes

        std::array<char, BUF_SIZE> buffer{};
        int readLen = gzread(fp, buffer.data(), BUF_SIZE);

        PreviewExtractResult result = readPreview(buffer.data(), readLen);

        gzclose(fp);
        return result;
    }
    if (!zipFp) {
        return PREVIEW_RESULT_COULD_NOT_OPEN_FILE;
    }

    zip_stat_t thumbStat;
    int statStatus = zip_stat(zipFp, "thumbnails/thumbnail.png", 0, &thumbStat);
    if (statStatus != 0) {
        zip_close(zipFp);
        return PREVIEW_RESULT_NO_PREVIEW;
    }

    if (!(thumbStat.valid & ZIP_STAT_SIZE)) {
        zip_close(zipFp);
        return PREVIEW_RESULT_ERROR_READING_PREVIEW;
    }

    zip_file_t* thumb = zip_fopen(zipFp, "thumbnails/thumbnail.png", 0);

    if (!thumb) {
        zip_close(zipFp);
        return PREVIEW_RESULT_ERROR_READING_PREVIEW;
    }

    data.assign(static_cast<size_t>(thumbStat.size), 0U);
    zip_uint64_t readBytes = 0;
    while (readBytes < thumbStat.size) {
        zip_int64_t read = zip_fread(thumb, data.data() + readBytes, thumbStat.size - readBytes);
        if (read == -1) {
            data.clear();
            zip_fclose(thumb);
            zip_close(zipFp);
            return PREVIEW_RESULT_ERROR_READING_PREVIEW;
        }
        readBytes += static_cast<zip_uint64_t>(read);
    }

    zip_fclose(thumb);
    zip_close(zipFp);
    return PREVIEW_RESULT_IMAGE_READ;
}
