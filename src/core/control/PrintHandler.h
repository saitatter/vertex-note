/*
 * VertexNote
 *
 * Prints a document
 *
 * @author VertexNote Team
 * https://github.com/saitatter/vertex-note
 *
 * @license GNU GPLv2 or later
 */

#pragma once

#include <cstddef>  // for size_t

#include <gtk/gtk.h>  // for GtkWindow

class Document;

namespace PrintHandler {
void print(Document* doc, size_t currentPage, GtkWindow* parent);
}
