#include "LinkDestinationGtk.h"

struct _LinkDestObjectClass {
    GObjectClass base_class;
};

G_DEFINE_TYPE(LinkDestObject, link_dest, G_TYPE_OBJECT)  // @suppress("Unused static function")

static void link_dest_init(LinkDestObject* linkAction) { linkAction->dest = nullptr; }

static gpointer parent_class = nullptr;

static void link_dest_finalize(GObject* object) {
    delete LINK_DEST(object)->dest;
    LINK_DEST(object)->dest = nullptr;

    G_OBJECT_CLASS(parent_class)->finalize(object);
}

static void link_dest_dispose(GObject* object) { G_OBJECT_CLASS(parent_class)->dispose(object); }

static void link_dest_class_init(LinkDestObjectClass* linkClass) {
    GObjectClass* gObjectClass = nullptr;

    parent_class = g_type_class_peek_parent(linkClass);

    gObjectClass = G_OBJECT_CLASS(linkClass);

    gObjectClass->dispose = link_dest_dispose;
    gObjectClass->finalize = link_dest_finalize;
}

auto link_dest_new() -> LinkDestObject* { return LINK_DEST(g_object_new(TYPE_LINK_DEST, nullptr)); }
