/*
 * VertexNote
 *
 * Experimental Qt clipboard service.
 */

#include "QtExperimentalClipboardService.h"

#include <QApplication>
#include <QClipboard>
#include <QMimeData>
#include <QString>

void QtExperimentalClipboardService::setText(std::string_view text) {
    QApplication::clipboard()->setText(QString::fromUtf8(text.data(), static_cast<int>(text.size())));
}

auto QtExperimentalClipboardService::text() const -> std::string { return QApplication::clipboard()->text().toStdString(); }

auto QtExperimentalClipboardService::hasText() const -> bool { return QApplication::clipboard()->mimeData()->hasText(); }

void QtExperimentalClipboardService::clear() { QApplication::clipboard()->clear(); }
