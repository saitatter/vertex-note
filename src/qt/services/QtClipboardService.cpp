/*
 * VertexNote
 *
 * Experimental Qt clipboard service.
 */

#include "QtClipboardService.h"

#include <QApplication>
#include <QClipboard>
#include <QMimeData>
#include <QString>

void QtClipboardService::setText(std::string_view text) {
    QApplication::clipboard()->setText(QString::fromUtf8(text.data(), static_cast<int>(text.size())));
}

auto QtClipboardService::text() const -> std::string { return QApplication::clipboard()->text().toStdString(); }

auto QtClipboardService::hasText() const -> bool { return QApplication::clipboard()->mimeData()->hasText(); }

void QtClipboardService::clear() { QApplication::clipboard()->clear(); }
