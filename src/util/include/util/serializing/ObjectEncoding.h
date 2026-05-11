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

#include <string>  // for string


class ObjectEncoding {
public:
    ObjectEncoding();
    virtual ~ObjectEncoding();

public:
    void addStr(const char* str);
    virtual void addData(const void* data, size_t len) = 0;

    std::string stealData();

public:
    std::string data;
};
