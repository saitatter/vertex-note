/*
 * VertexNote
 *
 * GTK wrapper for PDF link destinations.
 *
 * @license GNU GPLv2 or later
 */

#pragma once

#include <glib-object.h>
#include <glib.h>

#include "model/LinkDestination.h"

struct _LinkDestObject;
struct _LinkDestObjectClass;

using LinkDestObject = struct _LinkDestObject;
using LinkDestObjectClass = struct _LinkDestObjectClass;

struct _LinkDestObject {
    GObject base_instance;
    LinkDestination* dest;
};

enum {
    DOCUMENT_LINKS_COLUMN_NAME,
    DOCUMENT_LINKS_COLUMN_LINK,
    DOCUMENT_LINKS_COLUMN_EXPAND,
    DOCUMENT_LINKS_COLUMN_PAGE_NUMBER
};

#define TYPE_LINK_DEST (link_dest_get_type())
#define LINK_DEST(object) (G_TYPE_CHECK_INSTANCE_CAST((object), TYPE_LINK_DEST, LinkDestObject))
#define LINK_DEST_CLASS(klass) (G_TYPE_CHECK_CLASS_CAST((klass), TYPE_LINK_DEST, LinkDestObjectClass))
#define IS_LINK_DEST(object) (G_TYPE_CHECK_INSTANCE_TYPE((object), TYPE_LINK_DEST))
#define IS_LINK_DEST_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass), TYPE_LINK_DEST))
#define LINK_DEST_GET_CLASS(object) (G_TYPE_INSTANCE_GET_CLASS((object), TYPE_LINK_DEST, LinkDestObjectClass))

GType link_dest_get_type(void) G_GNUC_CONST;
LinkDestObject* link_dest_new();
