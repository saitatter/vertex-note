/*
 * VertexNote
 *
 * Xournal widget which is the "View" widget
 *
 * @author VertexNote Team
 * https://github.com/saitatter/vertex-note
 *
 * @license GNU GPLv2 or later
 */

#pragma once

#include <memory>  // for unique_ptr

#include <glib-object.h>  // for G_TYPE_CHECK_INSTANCE_CAST, G_TYPE_C...
#include <glib.h>         // for G_BEGIN_DECLS, G_END_DECLS
#include <gtk/gtk.h>      // for GtkWidget, GtkWidgetClass

namespace xoj::util {
template <class T>
class Rectangle;
}  // namespace xoj::util

namespace vn {
namespace util = xoj::util;
}

struct _GtkVertexNote;
struct _GtkVertexNoteClass;

G_BEGIN_DECLS

#define GTK_VERTEX_NOTE(obj) G_TYPE_CHECK_INSTANCE_CAST(obj, gtk_vertex_note_get_type(), GtkVertexNote)
#define GTK_VERTEX_NOTE_CLASS(klass) GTK_CHECK_CLASS_CAST(klass, gtk_vertex_note_get_type(), GtkVertexNoteClass)
#define GTK_IS_VERTEX_NOTE(obj) G_TYPE_CHECK_INSTANCE_TYPE(obj, gtk_vertex_note_get_type())

class EditSelection;
class Layout;
class PageView;
class ScrollHandling;
class VertexNoteView;
class InputContext;


typedef struct _GtkVertexNote GtkVertexNote;
typedef struct _GtkVertexNoteClass GtkVertexNoteClass;

struct _GtkVertexNote {
    GtkWidget widget;

    /**
     * The view class
     */
    VertexNoteView* view;

    /**
     * Scrollbars
     */
    ScrollHandling* scrollHandling;


    Layout* layout;


    /**
     * Selected content, if any
     */
    EditSelection* selection;

    /**
     * Input handling
     */
    InputContext* input = nullptr;

    GtkAdjustment* vadjustment;
    GtkAdjustment* hadjustment;
    GtkScrollablePolicy hscroll_policy;
    GtkScrollablePolicy vscroll_policy;
};

struct _GtkVertexNoteClass {
    GtkWidgetClass parent_class;
};

GType gtk_vertex_note_get_type();

GtkWidget* gtk_vertex_note_new(VertexNoteView* view, InputContext* inputContext, GtkAdjustment* vadj, GtkAdjustment* hadj);

Layout* gtk_vertex_note_get_layout(GtkWidget* widget);

void gtk_vertex_note_scroll_relative(GtkWidget* widget, double x, double y);

/// The given area is in Layout pixel-coordinates
void gtk_vertex_note_repaint_area(GtkWidget* widget, int x1, int y1, int x2, int y2);

vn::util::Rectangle<double>* gtk_vertex_note_get_visible_area(GtkWidget* widget, const PageView* p);

G_END_DECLS
