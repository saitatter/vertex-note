#include "RepaintHandler.h"

#include <gtk/gtk.h>  // for gtk_widget_queue_draw

#include "gui/widgets/VertexNoteWidget.h"  // for gtk_vertex_note_repaint_area

#include "PageView.h"     // for PageView
#include "VertexNoteView.h"  // for VertexNoteView

RepaintHandler::RepaintHandler(VertexNoteView* xournal): xournal(xournal) {}

RepaintHandler::~RepaintHandler() { this->xournal = nullptr; }

void RepaintHandler::repaintPage(const PageView* view) {
    auto p = view->getPixelPosition();
    int x2 = p.x + view->getDisplayWidth();
    int y2 = p.y + view->getDisplayHeight();
    gtk_vertex_note_repaint_area(this->xournal->getWidget(), p.x, p.y, x2, y2);
}

void RepaintHandler::repaintPageArea(const PageView* view, int x1, int y1, int x2, int y2) {
    auto p = view->getPixelPosition();
    gtk_vertex_note_repaint_area(this->xournal->getWidget(), p.x + x1, p.y + y1, p.x + x2, p.y + y2);
}

void RepaintHandler::repaintPageBorder(const PageView* view) { gtk_widget_queue_draw(this->xournal->getWidget()); }
