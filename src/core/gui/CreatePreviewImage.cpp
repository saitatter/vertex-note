#include "CreatePreviewImage.h"

#include <memory>

#include "model/PageType.h"  // for PageType
#include "util/raii/CairoWrappers.h"
#include "util/raii/GObjectSPtr.h"
#include "view/render/CairoPreviewBackgroundRenderer.h"
#include "view/render/CairoRenderContext.h"
#include "view/render/PageBackgroundRenderModelFactory.h"

namespace vn::helper {
auto createPreviewImage(const PageType& pt) -> GtkWidget* {
    const double zoom = 0.5;

    vn::util::CairoSurfaceSPtr surface(cairo_image_surface_create(CAIRO_FORMAT_ARGB32, PREVIEW_WIDTH, PREVIEW_HEIGHT),
                                        vn::util::adopt);
    vn::util::CairoSPtr crSPtr(cairo_create(surface.get()), vn::util::adopt);
    auto cr = crSPtr.get();

    cairo_scale(cr, zoom, zoom);

    vn::view::render::CairoRenderContext renderContext(cr, 1.0 / zoom);
    vn::view::render::CairoPreviewBackgroundRenderer backgroundRenderer;
    const auto model = vn::view::render::PageBackgroundRenderModelFactory::fromPageType(pt);
    backgroundRenderer.draw(model, {.x = 0.0, .y = 0.0, .width = PREVIEW_WIDTH / zoom, .height = PREVIEW_HEIGHT / zoom},
                            renderContext);

    cairo_identity_matrix(cr);

    cairo_set_line_width(cr, 2);
    cairo_set_source_rgb(cr, 0.8, 0.8, 0.8);
    cairo_move_to(cr, 0, 0);
    cairo_line_to(cr, PREVIEW_WIDTH, 0);
    cairo_line_to(cr, PREVIEW_WIDTH, PREVIEW_HEIGHT);
    cairo_line_to(cr, 0, PREVIEW_HEIGHT);
    cairo_line_to(cr, 0, 0);
    cairo_stroke(cr);

    vn::util::GObjectSPtr<GdkPixbuf> pixbuf(
            gdk_pixbuf_get_from_surface(surface.get(), 0, 0, PREVIEW_WIDTH, PREVIEW_HEIGHT), vn::util::adopt);
    return gtk_image_new_from_pixbuf(pixbuf.get());
}
};  // namespace vn::helper
