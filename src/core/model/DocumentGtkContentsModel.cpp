#include "DocumentGtkContentsModel.h"

#include <glib-object.h>

#include "model/Document.h"
#include "model/LinkDestinationGtk.h"
#include "pdf/base/PdfAction.h"
#include "pdf/base/PdfBookmarkIterator.h"
#include "util/Util.h"
#include "util/glib_casts.h"

namespace {

void buildTreeContentsModel(GtkTreeModel* model, GtkTreeIter* parent, PdfBookmarkIterator* iter) {
    do {
        GtkTreeIter treeIter = {0};

        PdfAction* action = iter->getAction();
        LinkDestination* dest = new LinkDestination(*action->getDestination());
        LinkDestObject* link = link_dest_new();
        link->dest = dest;

        if (action->getTitle().empty()) {
            g_object_unref(link);
            delete action;
            continue;
        }

        link->dest->setExpand(iter->isOpen());

        gtk_tree_store_append(GTK_TREE_STORE(model), &treeIter, parent);
        char* titleMarkup = g_markup_escape_text(action->getTitle().c_str(), -1);

        gtk_tree_store_set(GTK_TREE_STORE(model), &treeIter, DOCUMENT_LINKS_COLUMN_NAME, titleMarkup,
                           DOCUMENT_LINKS_COLUMN_LINK, link, DOCUMENT_LINKS_COLUMN_PAGE_NUMBER, "", -1);

        g_free(titleMarkup);
        g_object_unref(link);

        PdfBookmarkIterator* child = iter->getChildIter();
        if (child) {
            buildTreeContentsModel(model, &treeIter, child);
            delete child;
        }

        delete action;

    } while (iter->next());
}

auto fillPageLabels(GtkTreeModel* treeModel, GtkTreePath* path, GtkTreeIter* iter, const Document* doc) -> bool {
    (void) path;

    LinkDestObject* link = nullptr;
    gtk_tree_model_get(treeModel, iter, DOCUMENT_LINKS_COLUMN_LINK, &link, -1);

    if (link == nullptr) {
        return false;
    }

    auto page = doc->findPdfPage(link->dest->getPdfPage());

    gchar* pageLabel = nullptr;
    if (page != npos) {
        pageLabel = g_strdup_printf("%zu", page + 1);
    }
    gtk_tree_store_set(GTK_TREE_STORE(treeModel), iter, DOCUMENT_LINKS_COLUMN_PAGE_NUMBER, pageLabel, -1);
    g_free(pageLabel);

    g_object_unref(link);
    return false;
}

}  // namespace

namespace vn::legacy {

vn::util::GObjectSPtr<GtkTreeModel> createDocumentGtkContentsModel(const Document& doc) {
    PdfBookmarkIterator* iter = doc.getPdfDocument().getContentsIter();
    if (iter == nullptr) {
        return {};
    }

    vn::util::GObjectSPtr<GtkTreeModel> model(
            reinterpret_cast<GtkTreeModel*>(gtk_tree_store_new(4, G_TYPE_STRING, G_TYPE_OBJECT, G_TYPE_BOOLEAN,
                                                               G_TYPE_STRING)),
            vn::util::adopt);
    buildTreeContentsModel(model.get(), nullptr, iter);
    delete iter;

    gtk_tree_model_foreach(model.get(), vn::util::wrap_v<fillPageLabels>, const_cast<Document*>(&doc));
    return model;
}

}  // namespace vn::legacy
