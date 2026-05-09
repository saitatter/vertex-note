/*
 * VertexNote
 *
 * Toolbar icon for separator (only used for drag and drop and so)
 *
 * @author VertexNote Team
 * https://github.com/saitatter/vertex-note
 *
 * @license GNU GPLv2 or later
 */

#pragma once

#include <gdk-pixbuf/gdk-pixbuf.h>  // for GdkPixbuf
#include <gtk/gtk.h>                // for GtkWidget

enum SeparatorType { SEPARATOR, SPACER };

/**
 * Menuitem handler
 */
namespace ToolbarSeparatorImage {

/**
 * @brief Create Seperator Widget
 * This is used in the toolbar for spacing between items.
 *
 * @return GtkWidget* Separator
 */
GtkWidget* newImage(SeparatorType separator);

/**
 * @brief Create Separator Pixbuf
 * This is used in the toolbar customization to drag the separator
 * from the Customization dialog to the toolbar and vice versa.
 *
 * @return GdkPixbuf* Seperator
 */
GdkPixbuf* getNewToolPixbuf(SeparatorType separator);
};  // namespace ToolbarSeparatorImage
