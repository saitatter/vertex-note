/*
 * VertexNote
 *
 * get version info on various components and libraries
 *
 * @author VertexNote Team
 * https://github.com/saitatter/vertex-note
 *
 * @license GNU GPLv2 or later
 */
#pragma once

#include <string>

namespace xoj::util {
/// Get a string "VertexNote a.b.c + commit info"
std::string getVertexNoteVersion();

/// Get a string describing the OS
std::string getOsInfo();

/// Get a paragraph with all version info
std::string getVersionInfo();
};  // namespace xoj::util

#include "util/NamespaceAliases.h"
