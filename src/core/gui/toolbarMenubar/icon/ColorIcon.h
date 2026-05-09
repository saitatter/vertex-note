/*
 * VertexNote
 *
 * Icon for color buttons
 *
 * @author VertexNote Team
 * https://github.com/saitatter/vertex-note
 *
 * @license GNU GPLv2 or later
 */

#pragma once

#include <optional>

#include <gdk-pixbuf/gdk-pixbuf.h>  // for GdkPixbuf
#include <gtk/gtk.h>                // for GtkWidget

#include "util/Color.h"  // for Color
#include "util/raii/GObjectSPtr.h"

namespace ColorIcon {
/**
 * @brief Create a new GtkImage with preview color
 * @return The pointer is a floating ref
 */
GtkWidget* newGtkImage(Color color, int size = 22, bool circle = true,
                       std::optional<Color> secondaryColor = std::nullopt);

/**
 * @brief Create a new GdkPixbuf* with preview color
 */
vn::util::GObjectSPtr<GdkPixbuf> newGdkPixbuf(Color color, int size = 22, bool circle = true,
                                               std::optional<Color> secondaryColor = std::nullopt);
};  // namespace ColorIcon
