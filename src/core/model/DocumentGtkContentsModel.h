/*
 * VertexNote
 *
 * GTK bookmark model adapter for Document.
 *
 * @license GNU GPLv2 or later
 */

#pragma once

#include <gtk/gtk.h>

#include "util/raii/GObjectSPtr.h"

class Document;

namespace vn::legacy {

vn::util::GObjectSPtr<GtkTreeModel> createDocumentGtkContentsModel(const Document& doc);

}  // namespace vn::legacy
