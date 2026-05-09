/*
 * VertexNote
 *
 * RAII wrappers for C library classes
 *
 * @author VertexNote Team
 * https://github.com/saitatter/vertex-note
 *
 * @license GNU GPLv2 or later
 */

#pragma once

#include <memory>

#include <gtk/gtk.h>

namespace xoj::util {

inline namespace raii {
namespace specialization {
struct GtkPaperSizeDeleter {
    void operator()(GtkPaperSize* ps) {
        if (ps) {
            gtk_paper_size_free(ps);
        }
    }
};
};  // namespace specialization
using GtkPaperSizeUPtr = std::unique_ptr<GtkPaperSize, specialization::GtkPaperSizeDeleter>;


};  // namespace raii
};  // namespace xoj::util