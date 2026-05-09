/*
 * VertexNote
 *
 * GTK Open dialog to select a VertexNote-compatible document with preview
 *
 * @author VertexNote Team
 * https://github.com/saitatter/vertex-note
 *
 * @license GNU GPLv2 or later
 */

#pragma once

#include <functional>

#include <gtk/gtk.h>  // for GtkWindow

#include "filesystem.h"  // for path

class Settings;

namespace vn::OpenDlg {
void showOpenFileDialog(GtkWindow* parent, Settings* settings, std::function<void(fs::path)> callback);
/// @param callback(path, attachPdf)
void showAnnotatePdfDialog(GtkWindow* parent, Settings* settings, std::function<void(fs::path, bool)> callback);
void showOpenTemplateDialog(GtkWindow* parent, Settings* settings, std::function<void(fs::path)> callback);

/// @param callback(path, attachImg)
void showOpenImageDialog(GtkWindow* parent, Settings* settings, std::function<void(fs::path, bool)> callback);

void showMultiFormatDialog(GtkWindow* parent, std::vector<std::string> formats, std::function<void(fs::path)> callback);
};  // namespace vn::OpenDlg
