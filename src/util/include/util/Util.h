/*
 * VertexNote
 *
 * Xournal util functions
 *
 * @author VertexNote Team
 * https://github.com/saitatter/vertex-note
 *
 * @license GNU GPLv2 or later
 */

#pragma once

#include <cstdlib>     // size_t
#include <limits>      // for numeric_limits
#include <string>      // for string
#include <typeinfo>    // for typeid

#include "config-features.h"
#ifdef ENABLE_CPPTRACE
#include <cpptrace/cpptrace.hpp>
#endif

#include "Point.h"

class OutputStream;

namespace Util {

#if defined(_MSC_VER)
using PID = uint32_t;  // DWORD
#else
using PID = int32_t;  // pid
#endif

auto getPid() -> PID;

/**
 * Wrap the system call to redirect errors to a dialog
 */
void systemWithMessage(const char* command);

/**
 * Check if currently running in a Flatpak sandbox
 */
bool isFlatpakInstallation();

/**
 * Format coordinates to use 8 digits of precision https://m.xkcd.com/2170/
 * This function directly writes to the given OutputStream.
 */
extern void writeCoordinateString(OutputStream* out, double xVal, double yVal);

constexpr const char* PRECISION_FORMAT_STRING = "%.8g";

constexpr const auto DPI_NORMALIZATION_FACTOR = 72.0;

/**
 * Get the demangled name string of type T
 */
template <typename T>
std::string demangledTypeName() {
    const auto mangledName = typeid(T).name();
#ifdef ENABLE_CPPTRACE
    return cpptrace::demangle(mangledName);
#else
    // Cannot demangle name, but it might still be readable or not mangled in the first place
    return mangledName;
#endif
}

}  // namespace Util

constexpr auto npos = std::numeric_limits<size_t>::max();
