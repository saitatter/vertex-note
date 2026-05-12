/*
 * VertexNote
 *
 * Shared page snapshot builder for interactive render backends.
 */

#include "PageRenderSnapshotFactory.h"

#include "model/Document.h"
#include "model/Element.h"
#include "model/Image.h"
#include "model/Layer.h"
#include "model/NotePage.h"
#include "model/Stroke.h"
#include "model/TexImage.h"
#include "model/Text.h"
#include "vertexnote/geometry/GeometryElement.h"
#include "GeometryRenderModelFactory.h"
#include "ImageRenderModelFactory.h"
#include "PageBackgroundRenderModelFactory.h"
#include "PageRasterPreviewFactory.h"
#include "StrokeRenderModelFactory.h"
#include "TextRenderModelFactory.h"

namespace vn::view::render {

auto buildPageRenderSnapshots(Document& document, PageRenderSnapshotOptions options) -> std::vector<PageRenderSnapshot> {
    std::vector<PageRenderSnapshot> snapshots;

    document.lock_shared();
    const auto count = document.getPageCount();
    snapshots.reserve(count);
    for (std::size_t index = 0; index < count; ++index) {
        auto page = document.getPage(index);
        if (!page) {
            continue;
        }

        auto background = PageBackgroundRenderModelFactory::fromPage(page);
        if (background.backgroundFormat == PageTypeFormat::Pdf && options.renderPdfBackgrounds) {
            background.rasterContent =
                    createPdfPagePreviewRaster(document, page->getPdfPageNr(), page->getWidth(), page->getHeight());
        } else if (background.backgroundFormat == PageTypeFormat::Image) {
            background.rasterContent = createBackgroundImagePreviewRaster(page->getBackgroundImage());
        }

        std::vector<PageDrawableRenderModel> drawables;
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
                            drawables.emplace_back(StrokeRenderModelFactory::fromStroke(*stroke));
                        }
                        break;
                    }
                    case ELEMENT_TEXT: {
                        const auto* text = dynamic_cast<const Text*>(element);
                        if (text && !text->getText().empty()) {
                            drawables.emplace_back(TextRenderModelFactory::fromText(*text));
                        }
                        break;
                    }
                    case ELEMENT_IMAGE: {
                        const auto* image = dynamic_cast<const Image*>(element);
                        if (image && image->hasData()) {
                            drawables.emplace_back(ImageRenderModelFactory::fromImage(*image));
                        }
                        break;
                    }
                    case ELEMENT_TEXIMAGE: {
                        const auto* image = dynamic_cast<const TexImage*>(element);
                        if (image && image->getPdf()) {
                            drawables.emplace_back(ImageRenderModelFactory::fromTexImage(*image));
                        }
                        break;
                    }
                    case ELEMENT_GEOMETRY: {
                        const auto* geometry = dynamic_cast<const vn::geom::GeometryElement*>(element);
                        if (geometry) {
                            drawables.emplace_back(GeometryRenderModelFactory::fromGeometryElement(*geometry));
                        }
                        break;
                    }
                    default:
                        break;
                }
            }
        }

        PageRenderSnapshot snapshot;
        snapshot.width = page->getWidth();
        snapshot.height = page->getHeight();
        snapshot.background = std::move(background);
        snapshot.drawables = std::move(drawables);
        snapshots.push_back(std::move(snapshot));
    }
    document.unlock_shared();

    return snapshots;
}

}  // namespace vn::view::render
