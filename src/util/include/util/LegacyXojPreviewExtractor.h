/*
 * VertexNote
 *
 * Extracts a preview of an .xoj file, used by vertexnote-thumbnailer and vertex-note
 * Because of this xournal type checks cannot be used
 *
 * @author VertexNote Team
 * https://github.com/saitatter/vertex-note
 *
 * @license GNU GPLv2 or later
 */

#pragma once

#include <cstddef>  // for size_t
#include <vector>   // for vector

#include "filesystem.h"  // for path

enum PreviewExtractResult {

    /**
     * Successfully read an image from file
     */
    PREVIEW_RESULT_IMAGE_READ = 0,

    /**
     * File extension is wrong
     */
    PREVIEW_RESULT_BAD_FILE_EXTENSION,

    /**
     * The file could not be openend / found
     */
    PREVIEW_RESULT_COULD_NOT_OPEN_FILE,

    /**
     * The preview could not be extracted
     */
    PREVIEW_RESULT_ERROR_READING_PREVIEW,

    /**
     * The file contains no preview
     */
    PREVIEW_RESULT_NO_PREVIEW,
};

class LegacyXojPreviewExtractor {
public:
    LegacyXojPreviewExtractor();
    ~LegacyXojPreviewExtractor();

public:
    /**
     * Try to read the preview from file
     * @param file .xoj File
     * @return If an image was read, or the error
     */
    PreviewExtractResult readFile(const fs::path& file);

    /**
     * Try to read the preview from byte buffer
     * @param buffer Buffer
     * @param len Buffer len
     * @return If an image was read, or the error
     */
    PreviewExtractResult readPreview(char* buffer, int len);

    /**
     * @return The preview data, should be a binary PNG
     */
    const unsigned char* getData(size_t& dataLen) const;

    // Member
private:
    /**
     * Preview data
     */
    std::vector<unsigned char> data;
};
