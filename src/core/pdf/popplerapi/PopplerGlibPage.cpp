#include "PopplerGlibPage.h"

#include <algorithm>  // for max, min
#include <cstdlib>    // for abs, NULL, ptrdiff_t
#include <cstring>    // for memcpy
#include <memory>     // for make_unique
#include <sstream>    // for operator<<, ostringstream, bas...

#include <glib.h>          // for g_free, g_utf8_offset_to_pointer
#include <poppler-page.h>  // for _PopplerRectangle, _PopplerLin...
#include <poppler.h>       // for PopplerRectangle, g_object_ref
#include <poppler/cpp/poppler-image.h>
#include <poppler/cpp/poppler-page-renderer.h>
#include <poppler/cpp/poppler-page.h>

#include "pdf/base/PdfAction.h"     // for PdfAction
#include "pdf/base/PdfPage.h"       // for PdfRectangle, PdfPage::Link
#include "util/Assert.h"               // for xoj_assert
#include "util/GListView.h"            // for GListView, GListView<>::GListV...

#include "PopplerGlibAction.h"  // for PopplerGlibAction

PopplerGlibPage::PopplerGlibPage(PopplerPage* page, PopplerDocument* parentDoc):
        PopplerGlibPage(page, parentDoc, nullptr) {}

PopplerGlibPage::PopplerGlibPage(PopplerPage* page, PopplerDocument* parentDoc,
                                 std::shared_ptr<poppler::document> renderDocument):
        page(page),
        document(parentDoc),
        renderDocument(std::move(renderDocument)) {
    if (page != nullptr) {
        g_object_ref(page);
    }
}

PopplerGlibPage::PopplerGlibPage(const PopplerGlibPage& other):
        page(other.page),
        document(other.document),
        renderDocument(other.renderDocument) {
    if (page != nullptr) {
        g_object_ref(page);
    }
}

PopplerGlibPage::~PopplerGlibPage() {
    if (page) {
        g_object_unref(page);
        page = nullptr;
    }
}

PopplerGlibPage& PopplerGlibPage::operator=(const PopplerGlibPage& other) {
    if (&other == this) {
        return *this;
    }
    if (page) {
        g_object_unref(page);
        page = nullptr;
    }

    page = other.page;
    if (page != nullptr) {
        g_object_ref(page);
    }

    document = other.document;
    renderDocument = other.renderDocument;

    return *this;
}

auto PopplerGlibPage::getWidth() const -> double {
    double width = 0;
    poppler_page_get_size(const_cast<PopplerPage*>(page), &width, nullptr);

    return width;
}

auto PopplerGlibPage::getHeight() const -> double {
    double height = 0;
    poppler_page_get_size(const_cast<PopplerPage*>(page), nullptr, &height);

    return height;
}

auto PopplerGlibPage::renderPreviewRaster(int pixelWidth, int pixelHeight, double pageWidth, double pageHeight) const
        -> vn::util::RasterImageData {
    if (pixelWidth <= 0 || pixelHeight <= 0 || !renderDocument) {
        return {};
    }

    std::unique_ptr<poppler::page> cppPage(renderDocument->create_page(getPageId()));
    if (!cppPage) {
        return {};
    }

    poppler::page_renderer renderer;
    renderer.set_render_hints(poppler::page_renderer::antialiasing | poppler::page_renderer::text_antialiasing);
    renderer.set_image_format(poppler::image::format_argb32);
    renderer.set_paper_color(0xffffffff);

    const double xres = static_cast<double>(pixelWidth) / std::max(pageWidth, 1.0) * 72.0;
    const double yres = static_cast<double>(pixelHeight) / std::max(pageHeight, 1.0) * 72.0;
    const poppler::image image = renderer.render_page(cppPage.get(), xres, yres);
    if (!image.is_valid() || image.format() != poppler::image::format_argb32 || !image.const_data() ||
        image.bytes_per_row() <= 0 || image.width() <= 0 || image.height() <= 0) {
        return {};
    }

    vn::util::RasterImageData raster;
    raster.width = image.width();
    raster.height = image.height();
    raster.stride = image.bytes_per_row();
    raster.format = vn::util::RasterPixelFormat::Argb32Premultiplied;
    const auto* data = reinterpret_cast<const unsigned char*>(image.const_data());
    raster.pixels.assign(data, data + static_cast<std::size_t>(raster.stride * raster.height));
    return raster;
}

auto PopplerGlibPage::getPageId() const -> int { return poppler_page_get_index(page); }

auto PopplerGlibPage::getPageLabel() const -> std::string {
    gchar* label{poppler_page_get_label(page)};
    std::string cpp_label{label};
    g_free(label);
    return cpp_label;
}

auto PopplerGlibPage::findText(const std::string& text) -> std::vector<PdfRectangle> {
    std::vector<PdfRectangle> findings;

    double height = getHeight();
    GList* matches = poppler_page_find_text(page, text.c_str());
    for (auto& rect: GListView<PopplerRectangle>(matches)) {
        findings.emplace_back(rect.x1, height - rect.y2, rect.x2, height - rect.y1);
        poppler_rectangle_free(&rect);
    }
    g_list_free(matches);

    return findings;
}

auto getPopplerSelectionStyle(PdfPageSelectionStyle style) -> PopplerSelectionStyle {
    switch (style) {
        case PdfPageSelectionStyle::Word:
            return POPPLER_SELECTION_WORD;
        case PdfPageSelectionStyle::Line:
            return POPPLER_SELECTION_LINE;
        case PdfPageSelectionStyle::Linear:
        case PdfPageSelectionStyle::Area:
            return POPPLER_SELECTION_GLYPH;
        default:
            xoj_assert_message(false, "unimplemented");
    }
    return POPPLER_SELECTION_GLYPH;
}

auto PopplerGlibPage::selectText(const PdfRectangle& rect, PdfPageSelectionStyle style) -> std::string {
    PopplerRectangle pRect = {rect.x1, rect.y1, rect.x2, rect.y2};
    const auto pStyle = getPopplerSelectionStyle(style);
    if (style == PdfPageSelectionStyle::Area) {
        PopplerRectangle* rectArray = nullptr;
        guint numRects = 0;
        if (!poppler_page_get_text_layout_for_area(this->page, &pRect, &rectArray, &numRects)) {
            return "";
        }
        char* textBytes = poppler_page_get_text_for_area(page, &pRect);
        xoj_assert(textBytes);

        double y = rectArray[0].y2;
        std::ostringstream ss;
        for (guint i = 0; i < numRects; i++) {
            // do not copy characters whose bounding box has a non-empty intersection with rect
            const auto& r = rectArray[i];
            {
                auto x1 = std::max(rect.x1, r.x1);
                auto y1 = std::max(rect.y1, r.y1);
                auto x2 = std::min(rect.x2, r.x2);
                auto y2 = std::min(rect.y2, r.y2);

                bool inBounds = x2 > x1 && y2 > y1;
                if (!inBounds)
                    continue;
            }

            const auto eps = 1e-5;
            if (std::abs(y - r.y2) > eps) {
                // new line
                ss << '\n';
                y = rectArray[i].y2;
            }

            char* const startPos = g_utf8_offset_to_pointer(textBytes, i);
            char* const endPos = g_utf8_offset_to_pointer(textBytes, i + 1);
            for (long j = 0; j < static_cast<ptrdiff_t>(endPos - startPos); ++j) { ss << startPos[j]; }
        }
        g_free(textBytes);
        return ss.str();
    } else {
        char* text = poppler_page_get_selected_text(page, pStyle, &pRect);
        if (text) {
            std::string ret(text);
            g_free(text);
            return ret;
        } else {
            return "";
        }
    }
}

namespace {

auto intersects(const PopplerRectangle& selection, const PopplerRectangle& rect) -> bool {
    const auto x1 = std::max(selection.x1, rect.x1);
    const auto y1 = std::max(selection.y1, rect.y1);
    const auto x2 = std::min(selection.x2, rect.x2);
    const auto y2 = std::min(selection.y2, rect.y2);
    return x2 > x1 && y2 > y1;
}
}  // namespace

auto PopplerGlibPage::selectTextLines(const PdfRectangle& selectRect, PdfPageSelectionStyle style)
        -> TextSelection {
    std::vector<PdfRectangle> textRects;

    // The selection rectangle may be "improper" when selecting right-to-left
    // or bottom-to-top, so construct a normalized rectangle for hit testing.
    PopplerRectangle rect{std::min(selectRect.x1, selectRect.x2), std::min(selectRect.y1, selectRect.y2),
                          std::max(selectRect.x1, selectRect.x2), std::max(selectRect.y1, selectRect.y2)};

    PopplerRectangle* rectArray = nullptr;
    guint numRects = 0;
    if (style == PdfPageSelectionStyle::Area) {
        // We always want to select in the "proper" rectangle.
        PopplerRectangle area{rect.x1, rect.y1, rect.x2, rect.y2};
        if (!poppler_page_get_text_layout_for_area(this->page, &area, &rectArray, &numRects)) {
            return {textRects};
        }
    } else {
        if (!poppler_page_get_text_layout(this->page, &rectArray, &numRects)) {
            return {textRects};
        }
    }
    if (numRects == 0) {
        g_free(rectArray);
        return {textRects};
    }

    const auto isSameLine = [&](const auto& r1, const auto& r2) {
        const auto eps = 1e-5;
        return std::abs(r1.y1 - r2.y1) < eps && std::abs(r1.y2 - r2.y2) < eps;
    };

    PopplerRectangle prevRect = rectArray[0];
    if (style == PdfPageSelectionStyle::Area) {
        // helper to add only those rectangles that have nonempty intersection with the selected area
        const auto addTextRectsInArea = [&](const PopplerRectangle& r) {
            auto x1 = std::max(rect.x1, r.x1);
            auto y1 = std::max(rect.y1, r.y1);
            auto x2 = std::min(rect.x2, r.x2);
            auto y2 = std::min(rect.y2, r.y2);

            bool inBounds = x2 > x1 && y2 > y1;
            if (inBounds) {
                textRects.emplace_back(r.x1, r.y1, r.x2, r.y2);
            }
        };

        // construct the text rectangles
        for (guint i = 1; i < numRects; i++) {
            PopplerRectangle nextRect = rectArray[i];
            if (isSameLine(prevRect, nextRect)) {
                // Merge if both prev & next rectangles are in bounds. Note that
                // only x is checked since rectArray was constructed for the
                // selected area.
                bool shouldMerge = (rect.x1 <= prevRect.x1 && prevRect.x2 <= rect.x2 && rect.x1 <= nextRect.x1 &&
                                    nextRect.x2 <= rect.x2);
                if (shouldMerge) {
                    prevRect.x1 = std::min(prevRect.x1, nextRect.x2);
                    prevRect.x2 = std::max(prevRect.x2, nextRect.x2);
                    continue;
                }
            }

            addTextRectsInArea(prevRect);
            prevRect = nextRect;
        }
        addTextRectsInArea(prevRect);
    } else {
        // this is for all other styles (e.g., linear)

        const auto addTextRectsInSelection = [&](const PopplerRectangle& r) {
            if (intersects(rect, r)) {
                textRects.emplace_back(r.x1, r.y1, r.x2, r.y2);
            }
        };

        // construct the text rectangles
        for (guint i = 1; i < numRects; i++) {
            PopplerRectangle nextRect = rectArray[i];
            if (isSameLine(prevRect, nextRect)) {
                // merge the rectangles if their combined bounds still intersect the selection
                auto x1 = std::min(prevRect.x1, nextRect.x2);
                auto x2 = std::max(prevRect.x2, nextRect.x2);
                const PopplerRectangle merged{x1, prevRect.y1, x2, prevRect.y2};
                if (intersects(rect, merged)) {
                    prevRect.x1 = x1;
                    prevRect.x2 = x2;
                    continue;
                }
            }
            addTextRectsInSelection(prevRect);
            prevRect = nextRect;
        }
        addTextRectsInSelection(prevRect);
    }

    g_free(rectArray);
    return {textRects};
}

auto PopplerGlibPage::getLinks() -> std::vector<Link> {
    std::vector<Link> results;
    const double height = getHeight();

    GList* links = poppler_page_get_link_mapping(this->page);
    for (GList* l = links; l != NULL; l = g_list_next(l)) {
        const auto& link = *static_cast<PopplerLinkMapping*>(l->data);

        if (link.action) {
            PdfRectangle rect{link.area.x1, height - link.area.y2, link.area.x2, height - link.area.y1};
            results.emplace_back(Link{rect, std::make_unique<PopplerGlibAction>(link.action, document)});
        }
    }
    poppler_page_free_link_mapping(links);

    return results;
}
