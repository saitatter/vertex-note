/*
 * VertexNote
 *
 * Serializable interface
 *
 * @author VertexNote Team
 * https://github.com/saitatter/vertex-note
 *
 * @license GNU GPLv2 or later
 */

#pragma once

#include "InputStreamException.h"

class ObjectInputStream;
class ObjectOutputStream;

const static char* const XML_VERSION_STR = "XojStrm1:";

class Serializable {
public:
    virtual void serialize(ObjectOutputStream& out) const = 0;
    virtual void readSerialized(ObjectInputStream& in) = 0;

    virtual ~Serializable() = default;
};
