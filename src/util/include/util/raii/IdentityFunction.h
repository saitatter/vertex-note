/*
 * VertexNote
 *
 * Identity function
 *
 * @author VertexNote Team
 * https://github.com/saitatter/vertex-note
 *
 * @license GNU GPLv2 or later
 */

#pragma once

#include "util/NamespaceAliases.h"

namespace xoj::util {

inline namespace raii {
namespace specialization {
template <typename T>  // Todo(cpp20): use std:identity() and remove this header
constexpr auto identity = [](T* p) { return p; };
};
};  // namespace raii
};  // namespace vn::util
