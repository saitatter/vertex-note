/*
 * VertexNote
 *
 * Qt preview image renderer for the Qt shell.
 */

#include "QtPreviewImageRenderer.h"

#include <QByteArray>
#include <QImage>
#include <QPainter>
#include <QRectF>

#include "view/render/QtPainterRenderContext.h"

namespace vn::view::render {

void QtPreviewImageRenderer::draw(const ImageRenderModel& image, RenderContext& context) const {
    if (context.backend() != RenderBackend::QtPainter || image.encodedBytes.empty()) {
        return;
    }

    auto* painter = static_cast<QtPainterRenderContext&>(context).native();
    if (!painter) {
        return;
    }

    QImage raster;
    if (!raster.loadFromData(QByteArray(image.encodedBytes.data(), static_cast<qsizetype>(image.encodedBytes.size())))) {
        return;
    }

    painter->drawImage(QRectF(image.x, image.y, image.width, image.height), raster);
}

}  // namespace vn::view::render
