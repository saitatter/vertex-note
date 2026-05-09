/*
 * VertexNote
 *
 * Binary encoded serialized stream
 *
 * @author VertexNote Team
 * https://github.com/saitatter/vertex-note
 *
 * @license GNU GPLv2 or later
 */

#pragma once

#include "ObjectEncoding.h"

class BinObjectEncoding: public ObjectEncoding {
public:
    BinObjectEncoding();
    ~BinObjectEncoding() override;

public:
    void addData(const void* data, size_t len) override;

private:
};
