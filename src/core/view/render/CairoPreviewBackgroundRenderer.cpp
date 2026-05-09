/*
 * VertexNote
 *
 * Cairo preview background renderer for GTK-side preview surfaces.
 */

#include "CairoPreviewBackgroundRenderer.h"

#include <algorithm>

#include <cairo.h>

#include "view/render/CairoRenderContext.h"

namespace {

auto penWidthForZoom(double baseWidth, double zoomFactor) -> double {
    return std::max(baseWidth / std::max(zoomFactor, 0.001), 0.35);
}

void setSourceRgb(cairo_t* cr, double r, double g, double b) { cairo_set_source_rgb(cr, r, g, b); }

void setSourceRgba(cairo_t* cr, double r, double g, double b, double a) { cairo_set_source_rgba(cr, r, g, b, a); }

void roundedRect(cairo_t* cr, double x, double y, double width, double height, double radius) {
    const double right = x + width;
    const double bottom = y + height;
    cairo_new_sub_path(cr);
    cairo_arc(cr, right - radius, y + radius, radius, -1.57079632679, 0.0);
    cairo_arc(cr, right - radius, bottom - radius, radius, 0.0, 1.57079632679);
    cairo_arc(cr, x + radius, bottom - radius, radius, 1.57079632679, 3.14159265359);
    cairo_arc(cr, x + radius, y + radius, radius, 3.14159265359, 4.71238898038);
    cairo_close_path(cr);
}

void drawLine(cairo_t* cr, double x1, double y1, double x2, double y2) {
    cairo_move_to(cr, x1, y1);
    cairo_line_to(cr, x2, y2);
    cairo_stroke(cr);
}

}  // namespace

namespace vn::view::render {

void CairoPreviewBackgroundRenderer::draw(const PageBackgroundRenderModel& page, const RenderRect& rect,
                                          RenderContext& context) const {
    if (context.backend() != RenderBackend::Cairo) {
        return;
    }

    auto* cr = static_cast<CairoRenderContext&>(context).native();
    if (!cr) {
        return;
    }

    roundedRect(cr, rect.x + 8.0, rect.y + 8.0, rect.width, rect.height, 6.0);
    setSourceRgba(cr, 0.0, 0.0, 0.0, 0.07);
    cairo_fill(cr);

    roundedRect(cr, rect.x, rect.y, rect.width, rect.height, 6.0);
    setSourceRgb(cr, 1.0, 1.0, 1.0);
    cairo_fill(cr);

    const double zoom = std::max(context.scaleFactor(), 0.001);
    const double pageLeft = rect.x;
    const double pageTop = rect.y;
    const double pageRight = rect.x + rect.width;
    const double pageBottom = rect.y + rect.height;

    switch (page.backgroundFormat) {
        case PageTypeFormat::Graph:
        case PageTypeFormat::IsoGraph: {
            cairo_set_line_width(cr, penWidthForZoom(0.9, zoom));
            setSourceRgb(cr, 184.0 / 255.0, 208.0 / 255.0, 248.0 / 255.0);
            for (double x = pageLeft + 24.0; x < pageRight - 20.0; x += 28.0) {
                drawLine(cr, x, pageTop + 20.0, x, pageBottom - 20.0);
            }
            for (double y = pageTop + 24.0; y < pageBottom - 20.0; y += 28.0) {
                drawLine(cr, pageLeft + 20.0, y, pageRight - 20.0, y);
            }
            break;
        }
        case PageTypeFormat::Dotted:
        case PageTypeFormat::IsoDotted: {
            setSourceRgb(cr, 180.0 / 255.0, 198.0 / 255.0, 221.0 / 255.0);
            for (double y = pageTop + 32.0; y < pageBottom - 24.0; y += 28.0) {
                for (double x = pageLeft + 28.0; x < pageRight - 24.0; x += 28.0) {
                    cairo_arc(cr, x, y, penWidthForZoom(1.6, zoom), 0.0, 6.28318530718);
                    cairo_fill(cr);
                }
            }
            break;
        }
        case PageTypeFormat::Staves: {
            cairo_set_line_width(cr, penWidthForZoom(1.0, zoom));
            setSourceRgb(cr, 134.0 / 255.0, 177.0 / 255.0, 1.0);
            for (double bandTop = pageTop + 52.0; bandTop < pageBottom - 60.0; bandTop += 132.0) {
                for (int line = 0; line < 5; ++line) {
                    const double y = bandTop + line * 12.0;
                    drawLine(cr, pageLeft + 24.0, y, pageRight - 24.0, y);
                }
            }
            break;
        }
        case PageTypeFormat::Pdf:
        case PageTypeFormat::Image: {
            roundedRect(cr, pageLeft + 20.0, pageTop + 20.0, rect.width - 40.0, 54.0, 4.0);
            if (page.backgroundFormat == PageTypeFormat::Pdf) {
                setSourceRgb(cr, 232.0 / 255.0, 238.0 / 255.0, 247.0 / 255.0);
            } else {
                setSourceRgb(cr, 242.0 / 255.0, 245.0 / 255.0, 250.0 / 255.0);
            }
            cairo_fill(cr);
            break;
        }
        case PageTypeFormat::Plain:
            break;
        case PageTypeFormat::Ruled:
        case PageTypeFormat::Lined:
        default: {
            cairo_set_line_width(cr, penWidthForZoom(1.0, zoom));
            setSourceRgb(cr, 134.0 / 255.0, 177.0 / 255.0, 1.0);
            for (double y = pageTop + 48.0; y < pageBottom - 24.0; y += 36.0) {
                drawLine(cr, pageLeft + 20.0, y, pageRight - 20.0, y);
            }
            if (page.backgroundFormat == PageTypeFormat::Lined) {
                cairo_set_line_width(cr, penWidthForZoom(1.0, zoom));
                setSourceRgb(cr, 1.0, 79.0 / 255.0, 129.0 / 255.0);
                drawLine(cr, pageLeft + 88.0, pageTop + 24.0, pageLeft + 88.0, pageBottom - 24.0);
            }
            break;
        }
    }
}

}  // namespace vn::view::render
