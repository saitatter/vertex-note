/*
 * VertexNote
 *
 * Serialized output stream
 *
 * @author VertexNote Team
 * https://github.com/saitatter/vertex-note
 *
 * @license GNU GPLv2 or later
 */

#pragma once

#include <cstddef>      // for size_t
#include <cstdint>      // for uint32_t
#include <string>       // for string
#include <string_view>  // for string_view
#include <vector>       // for vector

class ObjectEncoding;

class ObjectOutputStream {
public:
    ObjectOutputStream(ObjectEncoding* encoder);
    virtual ~ObjectOutputStream();

public:
    void writeObject(const char* name);
    void endObject();

    void writeInt(int i);
    void writeUInt(uint32_t u);
    void writeDouble(double d);
    void writeSizeT(size_t st);
    void writeString(const char* str);
    void writeString(std::string_view s);

    void writeData(const void* data, size_t len, size_t width);

    template <typename T>
    void writeData(const std::vector<T>& data);

    /// Writes the raw image data to the output stream.
    void writeImage(const std::string_view& imgData);

    std::string stealData();

private:
    ObjectEncoding* encoder = nullptr;
};

template <typename T>
void ObjectOutputStream::writeData(const std::vector<T>& data) {
    writeData(data.data(), data.size(), sizeof(T));
}
