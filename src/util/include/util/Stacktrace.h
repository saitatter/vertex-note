/*
 * VertexNote
 *
 * Prints a Stacktrace
 *
 * @author VertexNote Team
 * https://github.com/saitatter/vertex-note
 *
 * @license GNU GPLv2 or later
 */

#pragma once

#include <iostream>

#include "filesystem.h"

class Stacktrace final {
private:
    Stacktrace();
    ~Stacktrace();

public:
    static void printStacktrace();
    static void printStacktrace(std::ostream& stream);
};
