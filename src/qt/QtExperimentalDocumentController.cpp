/*
 * VertexNote
 *
 * Experimental Qt document controller backed by the shared core model.
 */

#include "QtExperimentalDocumentController.h"

#include <algorithm>
#include <cctype>

#include "control/xojfile/LoadHandler.h"
#include "model/BackgroundImage.h"
#include "model/Element.h"
#include "model/Document.h"
#include "model/Image.h"
#include "model/Layer.h"
#include "model/NotePage.h"
#include "model/Text.h"
#include "model/Stroke.h"
#include "vertexnote/geometry/GeometryElement.h"
#include "util/raii/GObjectSPtr.h"
#include "util/raii/CairoWrappers.h"
#include "view/render/GeometryRenderModelFactory.h"
#include "view/render/PageBackgroundRenderModelFactory.h"
#include "view/render/ImageRenderModelFactory.h"
#include "view/render/StrokeRenderModelFactory.h"
#include "view/render/TextRenderModelFactory.h"

QtExperimentalDocumentController::QtExperimentalDocumentController() { newBlankDocument(); }

void QtExperimentalDocumentController::newBlankDocument() {
    this->document = std::make_unique<Document>(&this->documentHandler);
    this->document->lock();
    this->document->addPage(std::make_shared<NotePage>(595.0, 842.0));
    this->document->unlock();
    this->loadedPath.reset();
    rebuildPageSnapshots();
}

auto QtExperimentalDocumentController::loadFrom(const std::filesystem::path& path, std::string* errorMessage) -> bool {
    try {
        if (isPdfPath(path)) {
            auto loaded = std::make_unique<Document>(&this->documentHandler);
            if (!loaded->readPdf(path, true, false)) {
                if (errorMessage) {
                    *errorMessage = loaded->getLastErrorMsg();
                }
                return false;
            }
            loaded->setFilepath(path);
            this->document = std::move(loaded);
            this->loadedPath = path;
            rebuildPageSnapshots();
            return true;
        }

        LoadHandler loader;
        auto loaded = loader.loadDocument(path);
        this->document = std::move(loaded);
        this->loadedPath = path;
        rebuildPageSnapshots();
        return true;
    } catch (const std::exception& e) {
        if (errorMessage) {
            *errorMessage = e.what();
        }
        return false;
    }
}

auto QtExperimentalDocumentController::hasDocument() const -> bool { return static_cast<bool>(this->document); }

auto QtExperimentalDocumentController::pageCount() const -> std::size_t {
    if (!this->document) {
        return 0U;
    }
    this->document->lock_shared();
    const auto count = this->document->getPageCount();
    this->document->unlock_shared();
    return count;
}

auto QtExperimentalDocumentController::snapshotPages() const -> const std::vector<QtExperimentalPageInfo>& {
    return this->pageSnapshots;
}

auto QtExperimentalDocumentController::sourcePath() const -> const std::optional<std::filesystem::path>& {
    return this->loadedPath;
}

auto QtExperimentalDocumentController::titleText() const -> std::string {
    if (this->loadedPath) {
        return this->loadedPath->filename().string();
    }
    return "Untitled Document";
}

auto QtExperimentalDocumentController::isPdfPath(const std::filesystem::path& path) -> bool {
    return normalizeExtension(path) == ".pdf";
}

auto QtExperimentalDocumentController::normalizeExtension(const std::filesystem::path& path) -> std::string {
    auto extension = path.extension().string();
    std::transform(extension.begin(), extension.end(), extension.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return extension;
}

namespace {

auto writeSurfaceToPng(cairo_surface_t* surface) -> std::string {
    if (!surface) {
        return {};
    }

    std::string encoded;
    const auto writer = [](void* closure, const unsigned char* data, unsigned int length) -> cairo_status_t {
        auto* target = static_cast<std::string*>(closure);
        target->append(reinterpret_cast<const char*>(data), length);
        return CAIRO_STATUS_SUCCESS;
    };

    if (cairo_surface_write_to_png_stream(surface, writer, &encoded) != CAIRO_STATUS_SUCCESS) {
        return {};
    }

    return encoded;
}

auto renderPdfBackgroundPreview(const Document& document, size_t pdfPageNumber, double pageWidth, double pageHeight) -> std::string {
    auto pdfPage = document.getPdfPage(pdfPageNumber);
    if (!pdfPage) {
        return {};
    }

    constexpr int previewWidth = 768;
    const double aspectRatio = pageHeight > 0.0 ? pageWidth / pageHeight : 1.0;
    const int previewHeight = std::max(1, static_cast<int>(previewWidth / std::max(aspectRatio, 0.001)));

    vn::util::CairoSurfaceSPtr surface(cairo_image_surface_create(CAIRO_FORMAT_ARGB32, previewWidth, previewHeight),
                                       vn::util::adopt);
    vn::util::CairoSPtr cr(cairo_create(surface.get()), vn::util::adopt);

    cairo_set_source_rgb(cr.get(), 1.0, 1.0, 1.0);
    cairo_paint(cr.get());
    cairo_scale(cr.get(), previewWidth / std::max(pageWidth, 1.0), previewHeight / std::max(pageHeight, 1.0));
    pdfPage->render(cr.get());

    return writeSurfaceToPng(surface.get());
}

auto renderImageBackgroundPreview(const BackgroundImage& image) -> std::string {
    const auto* pixbuf = image.getPixbuf();
    if (!pixbuf) {
        return {};
    }

    gchar* buffer = nullptr;
    gsize size = 0;
    GError* error = nullptr;
    if (!gdk_pixbuf_save_to_buffer(const_cast<GdkPixbuf*>(pixbuf), &buffer, &size, "png", &error, nullptr)) {
        if (error) {
            g_error_free(error);
        }
        return {};
    }

    std::string encoded(buffer, size);
    g_free(buffer);
    return encoded;
}

}  // namespace

void QtExperimentalDocumentController::rebuildPageSnapshots() {
    this->pageSnapshots.clear();
    if (!this->document) {
        return;
    }

    this->document->lock_shared();
    const auto count = this->document->getPageCount();
    this->pageSnapshots.reserve(count);
    for (std::size_t index = 0; index < count; ++index) {
        auto page = this->document->getPage(index);
        if (!page) {
            continue;
        }

        auto background = vn::view::render::PageBackgroundRenderModelFactory::fromPage(page);
        if (background.backgroundFormat == PageTypeFormat::Pdf) {
            background.rasterContentPng =
                    renderPdfBackgroundPreview(*this->document, page->getPdfPageNr(), page->getWidth(), page->getHeight());
        } else if (background.backgroundFormat == PageTypeFormat::Image) {
            background.rasterContentPng = renderImageBackgroundPreview(page->getBackgroundImage());
        }

        std::vector<vn::view::render::PageDrawableRenderModel> drawables;
        for (const Layer* layer: page->getLayersView()) {
            if (!layer || !layer->isVisible()) {
                continue;
            }

            for (const Element* element: layer->getElementsView()) {
                if (!element) {
                    continue;
                }

                switch (element->getType()) {
                    case ELEMENT_STROKE: {
                        const auto* stroke = dynamic_cast<const Stroke*>(element);
                        if (stroke && stroke->getPointCount() >= 2) {
                            drawables.emplace_back(vn::view::render::StrokeRenderModelFactory::fromStroke(*stroke));
                        }
                        break;
                    }
                    case ELEMENT_TEXT: {
                        const auto* text = dynamic_cast<const Text*>(element);
                        if (text && !text->getText().empty()) {
                            drawables.emplace_back(vn::view::render::TextRenderModelFactory::fromText(*text));
                        }
                        break;
                    }
                    case ELEMENT_IMAGE: {
                        const auto* image = dynamic_cast<const Image*>(element);
                        if (image && image->hasData()) {
                            drawables.emplace_back(vn::view::render::ImageRenderModelFactory::fromImage(*image));
                        }
                        break;
                    }
                    case ELEMENT_GEOMETRY: {
                        const auto* geometry = dynamic_cast<const vn::geom::GeometryElement*>(element);
                        if (geometry) {
                            drawables.emplace_back(vn::view::render::GeometryRenderModelFactory::fromGeometryElement(*geometry));
                        }
                        break;
                    }
                    default:
                        break;
                }
            }
        }

        this->pageSnapshots.push_back({.width = page->getWidth(),
                                       .height = page->getHeight(),
                                       .background = std::move(background),
                                       .drawables = std::move(drawables)});
    }
    this->document->unlock_shared();
}
