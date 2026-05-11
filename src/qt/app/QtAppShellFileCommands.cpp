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
void QtAppShell::registerFileCommands() {
    auto* ch = this->window.commandHost();

    // =====================================================================
    // Menu 1: File
    // =====================================================================
    ch->registerCommand(
            {.id = "app.new", .text = "New", .tooltip = "Create a new document", .shortcut = "Ctrl+N", .menu = "File"},
            [this]() { newSession(); });
    ch->registerCommand(
            {.id = "app.open", .text = "Open...", .tooltip = "Open a document", .shortcut = "Ctrl+O", .menu = "File"},
            [this]() { openSession(); });
    ch->registerCommand(
            {.id = "file.annotate-pdf", .text = "Annotate PDF...", .tooltip = "Open a PDF as an annotation document",
             .menu = "File"},
            [this]() { annotatePdf(); });
    (void) ch->menuForPath("File>Recent Documents");
    ch->addMenuSeparator("File");
    ch->registerCommand(
            {.id = "file.save", .text = "Save", .tooltip = "Save the document", .shortcut = "Ctrl+S", .menu = "File"},
            [this]() { saveDocument(); });
    ch->registerCommand(
            {.id = "app.save-as", .text = "Save As...", .tooltip = "Save to a new file", .shortcut = "Ctrl+Shift+S", .menu = "File"},
            [this]() { saveSessionAs(); });
    ch->addMenuSeparator("File");
    ch->registerCommand(
            {.id = "export.pdf", .text = "Export as PDF...", .tooltip = "Export all pages as PDF", .menu = "File"},
            [this]() { exportPdf(); });
    ch->registerCommand(
            {.id = "export.png", .text = "Export as...", .tooltip = "Export current page as image", .shortcut = "Ctrl+E", .menu = "File"},
            [this]() { exportPng(); });
    ch->addMenuSeparator("File");
    ch->registerCommand(
            {.id = "file.print", .text = "Print...", .tooltip = "Print the document", .shortcut = "Ctrl+P", .menu = "File"},
            [this]() { printDocument(); });
    ch->addMenuSeparator("File");
    ch->registerCommand(
            {.id = "app.quit", .text = "Quit", .tooltip = "Close VertexNote", .shortcut = "Ctrl+Q", .menu = "File"},
            [this]() { requestQuit(); });

}
