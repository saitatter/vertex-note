/*
 * VertexNote
 *
 * Input stream for a gzipped file
 *
 * @author VertexNote Team
 * https://github.com/saitatter/vertex-note
 *
 * @license GNU GPLv2 or later
 */

#pragma once

#include <zlib.h>

#include "util/InputStream.h"  // for InputStream
#include "util/NamespaceAliases.h"

#include "filesystem.h"  // for path


namespace xoj::util {

class GzInputStream final: public InputStream {
public:
    GzInputStream();
    GzInputStream(const fs::path& filepath);
    ~GzInputStream() override;

    int read(char* buffer, unsigned int len) noexcept override;
    void open(const fs::path& filepath);
    void close() override;

private:
    gzFile file;
};

}  // namespace xoj::util
