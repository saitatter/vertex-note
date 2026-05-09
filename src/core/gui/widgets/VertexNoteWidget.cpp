#include "VertexNoteWidget.h"

#include <algorithm>  // for max
#include <cmath>      // for NAN
#include <optional>   // for optional
#include <vector>     // for vector

#include <cairo.h>    // for cairo_restore, cairo_save
#include <gdk/gdk.h>  // for GdkRectangle, GdkWindowAttr

#include "control/Control.h"                // for Control
#include "control/settings/Settings.h"      // for Settings
#include "control/tools/EditSelection.h"    // for EditSelection
#include "gui/Layout.h"                     // for Layout
#include "gui/LegacyRedrawable.h"           // for Redrawable
#include "gui/PageView.h"                   // for PageView
#include "gui/Shadow.h"                     // for Shadow
#include "gui/VertexNoteView.h"                // for VertexNoteView
#include "gui/inputdevices/InputContext.h"  // for InputContext
#include "gui/scroll/ScrollHandling.h"      // for ScrollHandling
#include "util/Color.h"                     // for cairo_set_source_rgbi
#include "util/Rectangle.h"                 // for Rectangle

#include "config-debug.h"  // for DEBUG_DRAW_WIDGET

/*
 * Declares:
 *      static void gtk_vertex_note_class_init(GtkVertexNoteClass*);
 *      static void gtk_vertex_note_init(GtkVertexNote*);
 * Defines
 *      gtk_vertex_note_parent_class (pointer to GtkWidgetClass instance)
 *      GType gtk_vertex_note_get_type();
 */
G_DEFINE_TYPE_WITH_CODE(GtkVertexNote, gtk_vertex_note, GTK_TYPE_WIDGET, G_IMPLEMENT_INTERFACE(GTK_TYPE_SCROLLABLE, NULL))

static void gtk_vertex_note_get_preferred_width(GtkWidget* widget, gint* minimal_width, gint* natural_width);
static void gtk_vertex_note_get_preferred_height(GtkWidget* widget, gint* minimal_height, gint* natural_height);
static void gtk_vertex_note_size_allocate(GtkWidget* widget, GtkAllocation* allocation);
static void gtk_vertex_note_realize(GtkWidget* widget);
static auto gtk_vertex_note_draw(GtkWidget* widget, cairo_t* cr) -> gboolean;
static void gtk_vertex_note_dispose(GObject* object);

static void gtk_vertex_note_set_property(GObject* object, guint prop_id, const GValue* value, GParamSpec* pspec);
static void gtk_vertex_note_get_property(GObject* object, guint prop_id, GValue* value, GParamSpec* pspec);

auto gtk_vertex_note_new(VertexNoteView* view, InputContext* inputContext, GtkAdjustment* vadj, GtkAdjustment* hadj)
        -> GtkWidget* {
    GtkVertexNote* noteWidget = GTK_VERTEX_NOTE(g_object_new(gtk_vertex_note_get_type(), nullptr));
    noteWidget->view = view;
    noteWidget->scrollHandling = inputContext->getScrollHandling();
    noteWidget->layout = new Layout(view, inputContext->getScrollHandling());
    noteWidget->selection = nullptr;
    noteWidget->input = inputContext;

    // Scrollable interface
    noteWidget->vadjustment = GTK_ADJUSTMENT(g_object_ref(vadj));
    noteWidget->hadjustment = GTK_ADJUSTMENT(g_object_ref(hadj));
    noteWidget->vscroll_policy = GTK_SCROLL_NATURAL;
    noteWidget->hscroll_policy = GTK_SCROLL_NATURAL;


    noteWidget->input->connect(GTK_WIDGET(noteWidget));

    return GTK_WIDGET(noteWidget);
}

enum { PROP_0, PROP_HADJUSTMENT, PROP_VADJUSTMENT, PROP_HSCROLL_POLICY, PROP_VSCROLL_POLICY };

static void gtk_vertex_note_class_init(GtkVertexNoteClass* cptr) {
    auto* widget_class = reinterpret_cast<GtkWidgetClass*>(cptr);

    widget_class->realize = gtk_vertex_note_realize;
    widget_class->get_preferred_width = gtk_vertex_note_get_preferred_width;
    widget_class->get_preferred_height = gtk_vertex_note_get_preferred_height;
    widget_class->size_allocate = gtk_vertex_note_size_allocate;

    widget_class->draw = gtk_vertex_note_draw;

#ifdef DEBUG_DRAW_WIDGET
    widget_class->queue_draw_region = +[](GtkWidget* w, const cairo_region_t* reg) {
        cairo_rectangle_int_t r;
        cairo_region_get_extents(reg, &r);
        auto width = gtk_widget_get_allocated_width(w);
        auto height = gtk_widget_get_allocated_height(w);

        printf("   * queue_draw_region: %d x %d + (%d ; %d) out of %d x %d\n", r.width, r.height, r.x, r.y, width,
               height);
        GTK_WIDGET_CLASS(gtk_vertex_note_parent_class)->queue_draw_region(w, reg);
    };
#endif

    G_OBJECT_CLASS(cptr)->dispose = gtk_vertex_note_dispose;

    // Scrollable interface
    G_OBJECT_CLASS(cptr)->set_property = gtk_vertex_note_set_property;
    G_OBJECT_CLASS(cptr)->get_property = gtk_vertex_note_get_property;
    g_object_class_override_property(G_OBJECT_CLASS(cptr), PROP_HADJUSTMENT, "hadjustment");
    g_object_class_override_property(G_OBJECT_CLASS(cptr), PROP_VADJUSTMENT, "vadjustment");
    g_object_class_override_property(G_OBJECT_CLASS(cptr), PROP_HSCROLL_POLICY, "hscroll-policy");
    g_object_class_override_property(G_OBJECT_CLASS(cptr), PROP_VSCROLL_POLICY, "vscroll-policy");
}

auto gtk_vertex_note_get_visible_area(GtkWidget* widget, const PageView* p) -> vn::util::Rectangle<double>* {
    if (!p || !p->isVisible()) {
        return nullptr;
    }

    g_return_val_if_fail(widget != nullptr, nullptr);
    g_return_val_if_fail(GTK_IS_VERTEX_NOTE(widget), nullptr);

    GtkVertexNote* noteWidget = GTK_VERTEX_NOTE(widget);

    GtkAdjustment* vadj = noteWidget->scrollHandling->getVertical();
    GtkAdjustment* hadj = noteWidget->scrollHandling->getHorizontal();

    GdkRectangle r2;
    r2.x = static_cast<int>(gtk_adjustment_get_value(hadj));
    r2.y = static_cast<int>(gtk_adjustment_get_value(vadj));
    r2.width = static_cast<int>(gtk_adjustment_get_page_size(hadj));
    r2.height = static_cast<int>(gtk_adjustment_get_page_size(vadj));

    GdkRectangle r1;
    auto pos = p->getPixelPosition();
    r1.x = pos.x;
    r1.y = pos.y;
    r1.width = p->getDisplayWidth();
    r1.height = p->getDisplayHeight();

    GdkRectangle r3 = {0, 0, 0, 0};
    gdk_rectangle_intersect(&r1, &r2, &r3);

    if (r3.width == 0 && r3.height == 0) {
        return nullptr;
    }

    r3.x -= r1.x;
    r3.y -= r1.y;

    double zoom = noteWidget->view->getZoom();

    if (r3.x < 0 || r3.y < 0) {
        g_warning("VertexNoteWidget:gtk_vertex_note_get_visible_area: intersection rectangle coordinates are negative which "
                  "should never happen");
    }

    return new vn::util::Rectangle<double>(std::max(r3.x, 0) / zoom, std::max(r3.y, 0) / zoom, r3.width / zoom,
                                            r3.height / zoom);
}

auto gtk_vertex_note_get_layout(GtkWidget* widget) -> Layout* {
    g_return_val_if_fail(widget != nullptr, nullptr);
    g_return_val_if_fail(GTK_IS_VERTEX_NOTE(widget), nullptr);

    GtkVertexNote* noteWidget = GTK_VERTEX_NOTE(widget);
    return noteWidget->layout;
}

static void gtk_vertex_note_init(GtkVertexNote* noteWidget) {
    GtkWidget* widget = GTK_WIDGET(noteWidget);

    gtk_widget_set_can_focus(widget, true);
}

static void gtk_vertex_note_get_preferred_width(GtkWidget* widget, gint* minimal_width, gint* natural_width) {
    g_return_if_fail(GTK_IS_VERTEX_NOTE(widget));
    GtkVertexNote* noteWidget = GTK_VERTEX_NOTE(widget);
    g_return_if_fail(noteWidget->layout);
    *minimal_width = *natural_width = noteWidget->layout->getMinimalPixelWidth();
}

static void gtk_vertex_note_get_preferred_height(GtkWidget* widget, gint* minimal_height, gint* natural_height) {
    g_return_if_fail(GTK_IS_VERTEX_NOTE(widget));
    GtkVertexNote* noteWidget = GTK_VERTEX_NOTE(widget);
    g_return_if_fail(noteWidget->layout);
    *minimal_height = *natural_height = noteWidget->layout->getMinimalPixelHeight();
}

/**
 * This method is called while scrolling or after the VertexNoteWidget size has changed
 */
static void gtk_vertex_note_size_allocate(GtkWidget* widget, GtkAllocation* allocation) {
    g_return_if_fail(widget != nullptr);
    g_return_if_fail(GTK_IS_VERTEX_NOTE(widget));
    g_return_if_fail(allocation != nullptr);

    gtk_widget_set_allocation(widget, allocation);

    if (gtk_widget_get_realized(widget)) {
        gdk_window_move_resize(gtk_widget_get_window(widget), allocation->x, allocation->y, allocation->width,
                               allocation->height);
    }

    GtkVertexNote* noteWidget = GTK_VERTEX_NOTE(widget);

    gtk_adjustment_set_page_size(noteWidget->hadjustment, allocation->width);
    gtk_adjustment_set_page_size(noteWidget->vadjustment, allocation->height);

    gtk_vertex_note_get_layout(widget)->recomputeCenteringPadding(allocation->width, allocation->height);
}

static void gtk_vertex_note_realize(GtkWidget* widget) {
    GdkWindowAttr attributes;

    g_return_if_fail(widget != nullptr);
    g_return_if_fail(GTK_IS_VERTEX_NOTE(widget));

    gtk_widget_set_realized(widget, true);

    gtk_widget_set_hexpand(widget, true);
    gtk_widget_set_vexpand(widget, true);

    GtkAllocation allocation;
    gtk_widget_get_allocation(widget, &allocation);
    attributes.window_type = GDK_WINDOW_CHILD;
    attributes.x = allocation.x;
    attributes.y = allocation.y;
    attributes.width = allocation.width;
    attributes.height = allocation.height;

    attributes.wclass = GDK_INPUT_OUTPUT;
    attributes.event_mask = gtk_widget_get_events(widget) | GDK_EXPOSURE_MASK;

    gint attributes_mask = GDK_WA_X | GDK_WA_Y;

    GdkWindow* win = gdk_window_new(gtk_widget_get_parent_window(widget), &attributes, attributes_mask);
    gtk_widget_set_window(widget, win);
    gtk_widget_register_window(widget, win);
}

static void gtk_vertex_note_draw_shadow(GtkVertexNote* noteWidget, cairo_t* cr, int left, int top, int width, int height,
                                    bool selected) {
    Settings* settings = noteWidget->view->getControl()->getSettings();
    bool showShadow = settings->isShowPageShadow();
    if (selected) {
        if (showShadow) {
            Shadow::drawShadow(cr, left - 2, top - 2, width + 4, height + 4);
        }

        // Draw border
        Util::cairo_set_source_rgbi(cr, settings->getBorderColor());
        cairo_set_line_width(cr, 2.0);
        cairo_set_line_cap(cr, CAIRO_LINE_CAP_SQUARE);
        cairo_set_line_join(cr, CAIRO_LINE_JOIN_MITER);

        cairo_rectangle(cr, left - 1, top - 1, width + 2, height + 2);
        cairo_stroke(cr);
    } else if (showShadow) {
        Shadow::drawShadow(cr, left, top, width, height);
    }
}

void gtk_vertex_note_repaint_area(GtkWidget* widget, int x1, int y1, int x2, int y2) {
    g_return_if_fail(widget != nullptr);
    g_return_if_fail(GTK_IS_VERTEX_NOTE(widget));

    Range rg(x1, y1, x2, y2);
    if (!rg.isValid()) {
        return;
    }

    Range visible(vn::util::Rectangle<double>(gtk_adjustment_get_value(GTK_VERTEX_NOTE(widget)->hadjustment),
                                               gtk_adjustment_get_value(GTK_VERTEX_NOTE(widget)->vadjustment),
                                               gtk_adjustment_get_page_size(GTK_VERTEX_NOTE(widget)->hadjustment),
                                               gtk_adjustment_get_page_size(GTK_VERTEX_NOTE(widget)->vadjustment)));

    rg = visible.intersect(rg);
    if (rg.empty()) {
        return;
    }

    rg.translate(-visible.minX, -visible.minY);
    int minX = floor_cast<int>(rg.minX);
    int minY = floor_cast<int>(rg.minY);
    int width = ceil_cast<int>(rg.maxX) - minX;
    int height = ceil_cast<int>(rg.maxY) - minY;

    gtk_widget_queue_draw_area(widget, minX, minY, width, height);
}

static auto gtk_vertex_note_draw(GtkWidget* widget, cairo_t* cr) -> gboolean {
    g_return_val_if_fail(widget != nullptr, false);
    g_return_val_if_fail(GTK_IS_VERTEX_NOTE(widget), false);


#ifdef DEBUG_DRAW_WIDGET
    {
        double x1 = NAN, x2 = NAN, y1 = NAN, y2 = NAN;
        cairo_clip_extents(cr, &x1, &y1, &x2, &y2);
        printf("$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$\n\n"
               "      DRAW  %d x %d + (%d ; %d)\n\n"
               "$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$&&&&&&&&&&&&&&&&&&&&&&\n",
               round_cast<int>(x2 - x1), round_cast<int>(y2 - y1), round_cast<int>(x1), round_cast<int>(y1));
    }
#endif

    GtkVertexNote* noteWidget = GTK_VERTEX_NOTE(widget);

    cairo_translate(cr, -gtk_adjustment_get_value(noteWidget->hadjustment),
                    -gtk_adjustment_get_value(noteWidget->vadjustment));

    Range clip;
    cairo_clip_extents(cr, &clip.minX, &clip.minY, &clip.maxX, &clip.maxY);

    // Draw background
    Settings* settings = noteWidget->view->getControl()->getSettings();
    Util::cairo_set_source_rgbi(cr, settings->getBackgroundColor());
    cairo_paint(cr);

    // Add a padding for the shadow of the pages
    clip.addPadding(10);

    const auto& views = noteWidget->view->getViewPages();
    // Store the pages to release the layout mutex ASAP
    std::vector<std::pair<size_t, vn::util::Point<int>>> pages;
    noteWidget->layout->forEachEntriesIntersectingRange(
            clip, [&](size_t index, const Range&, vn::util::Point<int> pos) { pages.emplace_back(index, pos); });

    for (auto [index, pos]: pages) {
        const auto& pv = views[index];
        int pw = pv->getDisplayWidth();
        int ph = pv->getDisplayHeight();

        gtk_vertex_note_draw_shadow(noteWidget, cr, pos.x, pos.y, pw, ph, pv->isSelected());

        cairo_save(cr);
        cairo_translate(cr, pos.x, pos.y);

        pv->paintPage(cr, nullptr);
        cairo_restore(cr);
    }

    if (noteWidget->selection) {
        cairo_save(cr);
        double zoom = noteWidget->view->getZoom();

        auto pos = noteWidget->selection->getView()->getPixelPosition();
        cairo_translate(cr, pos.x, pos.y);

        noteWidget->selection->paint(cr, zoom);
        cairo_restore(cr);
    }

    std::optional<Recolor> recolor = settings->getRecolorParameters().recolorizeMainView ?
                                             std::make_optional(settings->getRecolorParameters().recolor) :
                                             std::nullopt;

    if (recolor) {
        recolor->recolorCurrentCairoRegion(cr);
    }

    return true;
}

static void gtk_vertex_note_dispose(GObject* object) {
    g_return_if_fail(object != nullptr);
    g_return_if_fail(GTK_IS_VERTEX_NOTE(object));
    GtkVertexNote* noteWidget = GTK_VERTEX_NOTE(object);

    g_object_unref(noteWidget->vadjustment);
    noteWidget->vadjustment = nullptr;
    g_object_unref(noteWidget->hadjustment);
    noteWidget->hadjustment = nullptr;

    delete noteWidget->selection;
    noteWidget->selection = nullptr;

    delete noteWidget->layout;
    noteWidget->layout = nullptr;

    delete noteWidget->input;
    noteWidget->input = nullptr;

    G_OBJECT_CLASS(gtk_vertex_note_parent_class)->dispose(object);
}

static void gtk_vertex_note_set_property(GObject* object, guint prop_id, const GValue* value, GParamSpec* pspec) {
    GtkVertexNote* noteWidget = GTK_VERTEX_NOTE(object);

    switch (prop_id) {
        case PROP_HADJUSTMENT:
            noteWidget->hadjustment = GTK_ADJUSTMENT(g_value_get_object(value));
            break;
        case PROP_VADJUSTMENT:
            noteWidget->vadjustment = GTK_ADJUSTMENT(g_value_get_object(value));
            break;
        case PROP_HSCROLL_POLICY:
            if (noteWidget->hscroll_policy != g_value_get_enum(value)) {
                noteWidget->hscroll_policy = static_cast<GtkScrollablePolicy>(g_value_get_enum(value));
                gtk_widget_queue_resize(GTK_WIDGET(noteWidget));
                g_object_notify_by_pspec(object, pspec);
            }
            break;
        case PROP_VSCROLL_POLICY:
            if (noteWidget->vscroll_policy != g_value_get_enum(value)) {
                noteWidget->vscroll_policy = static_cast<GtkScrollablePolicy>(g_value_get_enum(value));
                gtk_widget_queue_resize(GTK_WIDGET(noteWidget));
                g_object_notify_by_pspec(object, pspec);
            }
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
            break;
    }
}
static void gtk_vertex_note_get_property(GObject* object, guint prop_id, GValue* value, GParamSpec* pspec) {
    GtkVertexNote* noteWidget = GTK_VERTEX_NOTE(object);

    switch (prop_id) {
        case PROP_HADJUSTMENT:
            g_value_set_object(value, noteWidget->hadjustment);
            break;
        case PROP_VADJUSTMENT:
            g_value_set_object(value, noteWidget->vadjustment);
            break;
        case PROP_HSCROLL_POLICY:
            g_value_set_enum(value, noteWidget->hscroll_policy);
            break;
        case PROP_VSCROLL_POLICY:
            g_value_set_enum(value, noteWidget->vscroll_policy);
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
            break;
    }
}

