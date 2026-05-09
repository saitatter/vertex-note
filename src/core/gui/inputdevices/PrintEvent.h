/*
 * VertexNote
 *
 * [Header description]
 *
 * @author VertexNote Team
 * https://github.com/saitatter/vertex-note
 *
 * @license GNU GPLv2 or later
 */

#pragma once

#include <ostream>

#include <gdk/gdk.h>

#include "InputEvents.h"

namespace xoj::input {
void printEvent(std::ostream& str, const InputEvent& e, guint32 timeRef);
void printGdkEvent(std::ostream& str, GdkEvent* e, guint32 timeRef);
};  // namespace xoj::input
