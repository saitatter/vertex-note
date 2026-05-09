/*
 * VertexNote
 *
 * Qt preview background renderer for the Qt shell.
 */

#include "QtPreviewBackgroundRenderer.h"

#include <algorithm>

#include <QColor>
#include <QImage>
#include <QPainter>
#include <QPen>
#include <QPointF>
#include <QRectF>

#include "view/render/QtPainterRenderContext.h"

namespace {

auto penWidthForZoom(double baseWidth, double zoomFactor) -> double {
    return std::max(baseWidth / std::max(zoomFactor, 0.001), 0.35);
}

auto qtImageFormatFor(vn::util::RasterPixelFormat format) -> QImage::Format {
    switch (format) {
        case vn::util::RasterPixelFormat::Argb32Premultiplied:
            return QImage::Format_ARGB32_Premultiplied;
        case vn::util::RasterPixelFormat::Rgba8888:
        default:
            return QImage::Format_RGBA8888;
    }
}

}  // namespace

namespace vn::view::render {

void QtPreviewBackgroundRenderer::draw(const PageBackgroundRenderModel& page, const RenderRect& rect,
                                       RenderContext& context) const {
    if (context.backend() != RenderBackend::QtPainter) {
        return;
    }

    auto* painter = static_cast<QtPainterRenderContext&>(context).native();
    if (!painter) {
        return;
    }

    const QRectF pageRect(rect.x, rect.y, rect.width, rect.height);
    painter->setPen(Qt::NoPen);
    painter->setBrush(QColor(0, 0, 0, 18));
    painter->drawRoundedRect(pageRect.translated(8.0, 8.0), 6.0, 6.0);
    painter->setBrush(Qt::white);
    painter->drawRoundedRect(pageRect, 6.0, 6.0);

    const double zoom = std::max(context.scaleFactor(), 0.001);
    const QColor ruling(134, 177, 255);
    const QColor margin(255, 79, 129);

    if (!page.rasterContent.empty()) {
        const auto format = qtImageFormatFor(page.rasterContent.format);
        const QImage raster(page.rasterContent.pixels.data(), page.rasterContent.width, page.rasterContent.height,
                            page.rasterContent.stride, format);
        painter->drawImage(pageRect, raster);
    }

    switch (page.backgroundFormat) {
        case PageTypeFormat::Graph:
        case PageTypeFormat::IsoGraph: {
            painter->setPen(QPen(QColor(184, 208, 248), penWidthForZoom(0.9, zoom)));
            for (double x = pageRect.left() + 24.0; x < pageRect.right() - 20.0; x += 28.0) {
                painter->drawLine(QPointF(x, pageRect.top() + 20.0), QPointF(x, pageRect.bottom() - 20.0));
            }
            for (double y = pageRect.top() + 24.0; y < pageRect.bottom() - 20.0; y += 28.0) {
                painter->drawLine(QPointF(pageRect.left() + 20.0, y), QPointF(pageRect.right() - 20.0, y));
            }
            break;
        }
        case PageTypeFormat::Dotted:
        case PageTypeFormat::IsoDotted: {
            painter->setPen(Qt::NoPen);
            painter->setBrush(QColor(180, 198, 221));
            for (double y = pageRect.top() + 32.0; y < pageRect.bottom() - 24.0; y += 28.0) {
                for (double x = pageRect.left() + 28.0; x < pageRect.right() - 24.0; x += 28.0) {
                    painter->drawEllipse(QPointF(x, y), penWidthForZoom(1.6, zoom), penWidthForZoom(1.6, zoom));
                }
            }
            break;
        }
        case PageTypeFormat::Staves: {
            painter->setPen(QPen(ruling, penWidthForZoom(1.0, zoom)));
            for (double bandTop = pageRect.top() + 52.0; bandTop < pageRect.bottom() - 60.0; bandTop += 132.0) {
                for (int line = 0; line < 5; ++line) {
                    const double y = bandTop + line * 12.0;
                    painter->drawLine(QPointF(pageRect.left() + 24.0, y), QPointF(pageRect.right() - 24.0, y));
                }
            }
            break;
        }
        case PageTypeFormat::Pdf: {
            painter->setPen(Qt::NoPen);
            painter->setBrush(QColor(232, 238, 247));
            painter->drawRoundedRect(QRectF(pageRect.left() + 20.0, pageRect.top() + 20.0, pageRect.width() - 40.0, 54.0),
                                     4.0, 4.0);
            painter->setPen(QColor(82, 97, 118));
            painter->drawText(QRectF(pageRect.left() + 34.0, pageRect.top() + 22.0, pageRect.width() - 68.0, 50.0),
                              Qt::AlignLeft | Qt::AlignVCenter,
                              QStringLiteral("PDF background page %1").arg(static_cast<int>(page.pdfPageNumber + 1)));
            break;
        }
        case PageTypeFormat::Image: {
            painter->setPen(Qt::NoPen);
            painter->setBrush(QColor(242, 245, 250));
            painter->drawRoundedRect(QRectF(pageRect.left() + 20.0, pageRect.top() + 20.0, pageRect.width() - 40.0, 54.0),
                                     4.0, 4.0);
            painter->setPen(QColor(82, 97, 118));
            painter->drawText(QRectF(pageRect.left() + 34.0, pageRect.top() + 22.0, pageRect.width() - 68.0, 50.0),
                              Qt::AlignLeft | Qt::AlignVCenter, QStringLiteral("Image background"));
            break;
        }
        case PageTypeFormat::Plain:
            break;
        case PageTypeFormat::Ruled:
        case PageTypeFormat::Lined:
        default: {
            painter->setPen(QPen(ruling, penWidthForZoom(1.0, zoom)));
            for (double y = pageRect.top() + 48.0; y < pageRect.bottom() - 24.0; y += 36.0) {
                painter->drawLine(QPointF(pageRect.left() + 20.0, y), QPointF(pageRect.right() - 20.0, y));
            }
            if (page.backgroundFormat == PageTypeFormat::Lined) {
                painter->setPen(QPen(margin, penWidthForZoom(1.0, zoom)));
                painter->drawLine(QPointF(pageRect.left() + 88.0, pageRect.top() + 24.0),
                                  QPointF(pageRect.left() + 88.0, pageRect.bottom() - 24.0));
            }
            break;
        }
    }

    if (page.hasBackgroundName || page.annotated || page.layerCount > 0) {
        painter->setPen(QColor(102, 112, 133));
        const QString footer = QStringLiteral("%1 | Layers %2 | Annotated %3")
                                       .arg(page.hasBackgroundName ? QString::fromStdString(page.backgroundName)
                                                                   : QStringLiteral("Background"))
                                       .arg(static_cast<int>(page.layerCount))
                                       .arg(page.annotated ? QStringLiteral("yes") : QStringLiteral("no"));
        painter->drawText(QRectF(pageRect.left() + 24.0, pageRect.bottom() - 36.0, pageRect.width() - 48.0, 22.0),
                          Qt::AlignLeft | Qt::AlignVCenter, footer);
    }
}

}  // namespace vn::view::render
