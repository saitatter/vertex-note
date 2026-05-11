/*
 * VertexNote
 *
 * GTK image adapters.
 *
 * @license GNU GPLv2 or later
 */

#pragma once

#include <optional>
#include <string>

#include <gdk-pixbuf/gdk-pixbuf.h>

class Image;

namespace vn::legacy {

std::optional<std::string> setImageFromGdkPixbuf(Image& image, GdkPixbuf* pixbuf);

}  // namespace vn::legacy
