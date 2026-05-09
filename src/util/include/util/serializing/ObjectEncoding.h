/*
 * VertexNote
 *
 * Encoding for serialized streams
 *
 * @author VertexNote Team
 * https://github.com/saitatter/vertex-note
 *
 * @license GNU GPLv2 or later
 */

#pragma once

#include <glib.h>  // for GString


class ObjectEncoding {
public:
    ObjectEncoding();
    virtual ~ObjectEncoding();

public:
    void addStr(const char* str) const;
    virtual void addData(const void* data, size_t len) = 0;

    /// The caller takes ownership of the returned data
    GString* stealData();

public:
    GString* data;
};
