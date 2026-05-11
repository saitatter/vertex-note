#include "PopplerGlibPage.h"

#include <algorithm>  // for max, min
#include <cmath>
#include <cstdlib>  // for NULL
#include <memory>   // for make_unique
#include <optional>
#include <sstream>  // for operator<<, ostringstream, bas...

#include <Annot.h>
#include <Link.h>
#include <PDFDoc.h>
#include <PDFRectangle.h>
#include <poppler/cpp/poppler-global.h>
#include <poppler/cpp/poppler-image.h>
#include <poppler/cpp/poppler-page-renderer.h>
#include <poppler/cpp/poppler-page.h>

#include "pdf/base/PdfAction.h"  // for PdfAction
#include "pdf/base/PdfPage.h"    // for PdfRectangle, PdfPage::Link

#include "PopplerGlibAction.h"  // for PopplerGlibAction

namespace {

auto toStdString(const poppler::ustring& value) -> std::string {
    const auto bytes = value.to_utf8();
    return {bytes.begin(), bytes.end()};
}

auto normalized(const PdfRectangle& rect) -> PdfRectangle {
    return {std::min(rect.x1, rect.x2), std::min(rect.y1, rect.y2), std::max(rect.x1, rect.x2),
            std::max(rect.y1, rect.y2)};
}

auto toPopplerRect(const PdfRectangle& rect) -> poppler::rectf {
    const auto r = normalized(rect);
    return {r.x1, r.y1, r.x2 - r.x1, r.y2 - r.y1};
}

auto toPdfRectangle(const poppler::rectf& rect) -> PdfRectangle {
    return {rect.left(), rect.top(), rect.right(), rect.bottom()};
}

auto intersects(const PdfRectangle& selection, const poppler::rectf& rect) -> bool {
    const auto x1 = std::max(selection.x1, rect.left());
    const auto y1 = std::max(selection.y1, rect.top());
    const auto x2 = std::min(selection.x2, rect.right());
    const auto y2 = std::min(selection.y2, rect.bottom());
    return x2 > x1 && y2 > y1;
}

}  // namespace

PopplerGlibPage::PopplerGlibPage(int pageIndex, std::shared_ptr<poppler::document> doc,
                                 std::shared_ptr<PDFDoc> linkDocument):
        pageIndex(pageIndex),
        document(std::move(doc)),
        linkDocument(std::move(linkDocument)) {}

PopplerGlibPage::PopplerGlibPage(const PopplerGlibPage& other):
        pageIndex(other.pageIndex),
        document(other.document),
        linkDocument(other.linkDocument) {}

PopplerGlibPage::~PopplerGlibPage() = default;

PopplerGlibPage& PopplerGlibPage::operator=(const PopplerGlibPage& other) {
    if (&other == this) {
        return *this;
    }

    pageIndex = other.pageIndex;
    document = other.document;
    linkDocument = other.linkDocument;

    return *this;
}

auto PopplerGlibPage::createPage() const -> std::unique_ptr<poppler::page> {
    if (!document || pageIndex < 0 || pageIndex >= document->pages()) {
        return nullptr;
    }
    return std::unique_ptr<poppler::page>(document->create_page(pageIndex));
}

auto PopplerGlibPage::getWidth() const -> double {
    const auto page = createPage();
    return page ? page->page_rect().width() : 0.0;
}

auto PopplerGlibPage::getHeight() const -> double {
    const auto page = createPage();
    return page ? page->page_rect().height() : 0.0;
}

auto PopplerGlibPage::renderPreviewRaster(int pixelWidth, int pixelHeight, double pageWidth, double pageHeight) const
        -> vn::util::RasterImageData {
    if (pixelWidth <= 0 || pixelHeight <= 0 || !document) {
        return {};
    }

    auto cppPage = createPage();
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

auto PopplerGlibPage::getPageId() const -> int { return pageIndex; }

auto PopplerGlibPage::getPageLabel() const -> std::string {
    const auto page = createPage();
    return page ? toStdString(page->label()) : std::string{};
}

auto PopplerGlibPage::findText(const std::string& text) -> std::vector<PdfRectangle> {
    std::vector<PdfRectangle> findings;
    const auto page = createPage();
    if (!page || text.empty()) {
        return findings;
    }

    poppler::rectf match;
    auto direction = poppler::page::search_from_top;
    while (page->search(poppler::ustring::from_utf8(text.c_str()), match, direction, poppler::case_insensitive)) {
        findings.push_back(toPdfRectangle(match));
        direction = poppler::page::search_next_result;
    }

    return findings;
}

auto PopplerGlibPage::selectText(const PdfRectangle& rect, PdfPageSelectionStyle style) -> std::string {
    const auto page = createPage();
    if (!page) {
        return {};
    }

    (void) style;
    return toStdString(page->text(toPopplerRect(rect)));
}

auto PopplerGlibPage::selectTextLines(const PdfRectangle& selectRect, PdfPageSelectionStyle style)
        -> TextSelection {
    std::vector<PdfRectangle> textRects;
    const auto page = createPage();
    if (!page) {
        return {textRects};
    }

    // The selection rectangle may be "improper" when selecting right-to-left
    // or bottom-to-top, so construct a normalized rectangle for hit testing.
    const auto rect = normalized(selectRect);
    const auto boxes = page->text_list();
    if (boxes.empty()) {
        return {textRects};
    }

    const auto isSameLine = [](const poppler::rectf& r1, const poppler::rectf& r2) {
        const auto eps = 1e-5;
        return std::abs(r1.top() - r2.top()) < eps && std::abs(r1.bottom() - r2.bottom()) < eps;
    };

    (void) style;
    std::optional<poppler::rectf> current;
    for (const auto& box: boxes) {
        const auto bbox = box.bbox();
        if (!intersects(rect, bbox)) {
            continue;
        }
        if (current && isSameLine(*current, bbox)) {
            current->set_left(std::min(current->left(), bbox.left()));
            current->set_right(std::max(current->right(), bbox.right()));
            current->set_top(std::min(current->top(), bbox.top()));
            current->set_bottom(std::max(current->bottom(), bbox.bottom()));
            continue;
        }
        if (current) {
            textRects.push_back(toPdfRectangle(*current));
        }
        current = bbox;
    }
    if (current) {
        textRects.push_back(toPdfRectangle(*current));
    }

    return {textRects};
}

auto PopplerGlibPage::getLinks() -> std::vector<Link> {
    std::vector<Link> results;
    if (!linkDocument || pageIndex < 0) {
        return results;
    }
    const double height = getHeight();

    const auto links = linkDocument->getLinks(pageIndex + 1);
    if (!links) {
        return results;
    }
    for (const auto& link: links->getLinks()) {
        if (!link || !link->getAction()) {
            continue;
        }
        const auto& area = link->getRect();
        PdfRectangle rect{area.x1, height - area.y2, area.x2, height - area.y1};
        results.emplace_back(Link{rect, std::make_unique<PopplerGlibAction>(link->getAction(), linkDocument)});
    }

    return results;
}
