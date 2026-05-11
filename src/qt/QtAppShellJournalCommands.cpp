/*
 * VertexNote
 *
 * Qt app shell command registration.
 */

#include "QtAppShell.h"

#include <string>

#include <QDesktopServices>
#include <QInputDialog>
#include <QStatusBar>
#include <QString>
#include <QUrl>
void QtAppShell::registerJournalCommands() {
    auto* ch = this->window.commandHost();

    // =====================================================================
    // Menu 5: Journal
    // =====================================================================
    ch->registerCommand(
            {.id = "page.add-before", .text = "New Page Before", .tooltip = "Add page before current", .menu = "Journal"},
            [this]() { addPageBefore(); });
    ch->registerCommand(
            {.id = "page.add", .text = "New Page After", .tooltip = "Add page after current", .shortcut = "Ctrl+D", .menu = "Journal"},
            [this]() { addPage(); });
    ch->registerCommand(
            {.id = "page.add-end", .text = "New Page at End", .tooltip = "Add page at end of document", .menu = "Journal"},
            [this]() { addPageAtEnd(); });
    ch->registerCommand(
            {.id = "page.duplicate", .text = "Duplicate Page", .tooltip = "Duplicate the current page", .menu = "Journal"},
            [this]() { duplicatePage(); });
    ch->registerCommand(
            {.id = "page.move-up", .text = "Move Page Up", .tooltip = "Move the current page toward the start of the document",
             .menu = "Journal"},
            [this]() { movePageUp(); });
    ch->registerCommand(
            {.id = "page.move-down", .text = "Move Page Down", .tooltip = "Move the current page toward the end of the document",
             .menu = "Journal"},
            [this]() { movePageDown(); });
    ch->registerCommand(
            {.id = "journal.append-new-pdf-pages", .text = "Append New PDF Pages",
             .tooltip = "Append PDF pages not yet present in the document", .menu = "Journal"},
            [this]() { appendNewPdfPages(); });
    ch->addMenuSeparator("Journal");
    ch->registerCommand(
            {.id = "page.delete", .text = "Delete Page", .tooltip = "Delete the current page", .shortcut = "Ctrl+Shift+Delete", .menu = "Journal"},
            [this]() { deletePage(); });
    ch->addMenuSeparator("Journal");
    ch->registerCommand(
            {.id = "layer.add-above", .text = "Add Layer Above", .tooltip = "Add layer above current", .shortcut = "Ctrl+L", .menu = "Journal"},
            [this]() { addLayerAbove(); });
    ch->registerCommand(
            {.id = "layer.add-below", .text = "Add Layer Below", .tooltip = "Add layer below current", .menu = "Journal"},
            [this]() { addLayerBelow(); });
    ch->registerCommand(
            {.id = "layer.copy", .text = "Copy Layer", .tooltip = "Copy the current layer", .menu = "Journal"},
            [this]() { copyLayer(); });
    ch->registerCommand(
            {.id = "page.delete-layer", .text = "Delete Layer", .tooltip = "Delete the current layer", .shortcut = "Ctrl+Shift+L", .menu = "Journal"},
            [this]() { deleteLayer(); });
    ch->registerCommand(
            {.id = "layer.merge-down", .text = "Merge Layer Down", .tooltip = "Merge into layer below", .shortcut = "Ctrl+M", .menu = "Journal"},
            [this]() { mergeLayerDown(); });
    ch->registerCommand(
            {.id = "layer.rename", .text = "Rename Layer...", .tooltip = "Rename the current layer", .shortcut = "Ctrl+R", .menu = "Journal"},
            [this]() { renameLayerDialog(); });
    ch->registerCommand(
            {.id = "layer.show-all", .text = "Show All Layers", .tooltip = "Show all layers on the current page", .menu = "Journal"},
            [this]() { showAllLayers(); });
    ch->registerCommand(
            {.id = "layer.hide-all", .text = "Hide All Layers", .tooltip = "Hide all layers on the current page", .menu = "Journal"},
            [this]() { hideAllLayers(); });
    ch->addMenuSeparator("Journal");
    ch->registerCommand(
            {.id = "page.format", .text = "Paper Format...", .tooltip = "Set page size and orientation", .menu = "Journal"},
            [this]() { paperFormatDialog(); });
    ch->registerCommand(
            {.id = "page.template", .text = "Configure Page Template...",
             .tooltip = "Set the default page template for new Qt pages", .menu = "Journal"},
            [this]() { configurePageTemplateDialog(); });
    ch->registerCommand(
            {.id = "page.background", .text = "Paper Color...", .tooltip = "Change page background color", .menu = "Journal"},
            [this]() { showBackgroundDialog(); });

}
