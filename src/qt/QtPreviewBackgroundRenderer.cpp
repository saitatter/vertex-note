/*
 * VertexNote
 *
 * Qt preview background renderer for the Qt shell.
 */

#include "QtPreviewBackgroundRenderer.h"

#include <algorithm>
#include <cmath>

#include <QColor>
#include <QImage>
#include <QPainter>
#include <QPen>
#include <QPointF>
#include <QRectF>

#include "view/render/QtPainterRenderContext.h"

namespace {

auto toQColor(Color c) -> QColor { return QColor(c.red, c.green, c.blue, c.alpha); }

auto qtImageFormatFor(vn::util::RasterPixelFormat format) -> QImage::Format {
    switch (format) {
        case vn::util::RasterPixelFormat::Argb32Premultiplied:
            return QImage::Format_ARGB32_Premultiplied;
        case vn::util::RasterPixelFormat::Rgba8888:
        default:
            return QImage::Format_RGBA8888;
    }
}

auto isDarkBackground(Color bg) -> bool {
    // Simple luminance check
    const double lum = 0.299 * bg.red + 0.587 * bg.green + 0.114 * bg.blue;
    return lum < 128.0;
}

// ---- Constants matching GTK background views ----

// Ruled / Lined
constexpr double RULED_LINE_SPACING = 24.0;
constexpr double RULED_HEADER_SIZE = 80.0;
constexpr double RULED_FOOTER_SIZE = 60.0;
constexpr double RULED_LINE_WIDTH = 0.5;
constexpr double LINED_MARGIN = 72.0;
constexpr Color RULED_LINE_COLOR_LIGHT{0x40, 0xa0, 0xff, 0xff};   // dodgerblue
constexpr Color RULED_LINE_COLOR_DARK{0x43, 0x43, 0x43, 0xff};    // darkslategray
constexpr Color LINED_MARGIN_COLOR_LIGHT{0xff, 0x00, 0x80, 0xff}; // deeppink
constexpr Color LINED_MARGIN_COLOR_DARK{0x22, 0x00, 0x80, 0xff};  // midnightblue

// Graph / IsoGraph
constexpr double GRAPH_SQUARE_SIZE = 14.17;  // 5mm
constexpr double GRAPH_LINE_WIDTH = 0.5;
constexpr Color GRAPH_LINE_COLOR_LIGHT{0xbd, 0xbd, 0xbd, 0xff};  // silver
constexpr Color GRAPH_LINE_COLOR_DARK{0x43, 0x43, 0x43, 0xff};

// Dotted / IsoDotted
constexpr double DOT_SQUARE_SIZE = 14.17;  // 5mm
constexpr double DOT_RADIUS = 0.75;        // half of DEFAULT_LINE_WIDTH 1.5
constexpr Color DOT_COLOR_LIGHT{0xbd, 0xbd, 0xbd, 0xff};
constexpr Color DOT_COLOR_DARK{0x43, 0x43, 0x43, 0xff};

// Staves
constexpr double STAVES_HEADER = 80.0;
constexpr double STAVES_FOOTER = 60.0;
constexpr double STAVES_MARGIN = 50.0;
constexpr double STAVES_SPACING = 40.0;
constexpr double STAVES_LINE_SPACING = 5.0;
constexpr double STAVES_LINE_WIDTH = 0.5;

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

    // Drop shadow — asymmetric like GTK: thin top-left, thicker bottom-right
    painter->setPen(Qt::NoPen);
    painter->setBrush(QColor(0, 0, 0, 40));
    painter->drawRect(pageRect.adjusted(3.0, 3.0, 6.0, 6.0));
    painter->setBrush(QColor(0, 0, 0, 20));
    painter->drawRect(pageRect.adjusted(1.0, 1.0, 3.0, 3.0));

    // Page fill with background color — sharp corners
    painter->setBrush(toQColor(page.backgroundColor));
    painter->drawRect(pageRect);

    const bool dark = isDarkBackground(page.backgroundColor);

    // Raster content (PDF/image pages)
    if (!page.rasterContent.empty()) {
        const auto format = qtImageFormatFor(page.rasterContent.format);
        const QImage raster(page.rasterContent.pixels.data(), page.rasterContent.width, page.rasterContent.height,
                            page.rasterContent.stride, format);
        painter->drawImage(pageRect, raster);
    }

    // Use page dimensions for accurate positioning, fall back to rect if missing
    const double pw = page.pageWidth > 0.0 ? page.pageWidth : rect.width;
    const double ph = page.pageHeight > 0.0 ? page.pageHeight : rect.height;
    const double scaleX = rect.width / pw;
    const double scaleY = rect.height / ph;

    switch (page.backgroundFormat) {
        case PageTypeFormat::Graph: {
            const QColor lineColor = toQColor(dark ? GRAPH_LINE_COLOR_DARK : GRAPH_LINE_COLOR_LIGHT);
            painter->setPen(QPen(lineColor, GRAPH_LINE_WIDTH * scaleY, Qt::SolidLine));
            const double spacing = GRAPH_SQUARE_SIZE * scaleX;
            const double spacingY = GRAPH_SQUARE_SIZE * scaleY;
            for (double x = pageRect.left() + spacing; x < pageRect.right() - 1.0; x += spacing) {
                painter->drawLine(QPointF(x, pageRect.top()), QPointF(x, pageRect.bottom()));
            }
            for (double y = pageRect.top() + spacingY; y < pageRect.bottom() - 1.0; y += spacingY) {
                painter->drawLine(QPointF(pageRect.left(), y), QPointF(pageRect.right(), y));
            }
            break;
        }
        case PageTypeFormat::IsoGraph: {
            const QColor lineColor = toQColor(dark ? GRAPH_LINE_COLOR_DARK : GRAPH_LINE_COLOR_LIGHT);
            painter->setPen(QPen(lineColor, GRAPH_LINE_WIDTH * scaleY, Qt::SolidLine));
            const double triW = GRAPH_SQUARE_SIZE * scaleX;
            const double triH = GRAPH_SQUARE_SIZE * std::sqrt(3.0) / 2.0 * scaleY;
            int row = 0;
            for (double y = pageRect.top() + triH; y < pageRect.bottom() - 1.0; y += triH) {
                painter->drawLine(QPointF(pageRect.left(), y), QPointF(pageRect.right(), y));
                const double offset = (row % 2 == 0) ? 0.0 : triW * 0.5;
                for (double x = pageRect.left() + offset; x < pageRect.right() - 1.0; x += triW) {
                    painter->drawLine(QPointF(x, y), QPointF(x, y + triH));
                }
                ++row;
            }
            break;
        }
        case PageTypeFormat::Dotted: {
            const QColor dotColor = toQColor(dark ? DOT_COLOR_DARK : DOT_COLOR_LIGHT);
            painter->setPen(Qt::NoPen);
            painter->setBrush(dotColor);
            const double spacing = DOT_SQUARE_SIZE * scaleX;
            const double spacingY = DOT_SQUARE_SIZE * scaleY;
            const double r = DOT_RADIUS * std::min(scaleX, scaleY);
            for (double y = pageRect.top() + spacingY; y < pageRect.bottom() - 1.0; y += spacingY) {
                for (double x = pageRect.left() + spacing; x < pageRect.right() - 1.0; x += spacing) {
                    painter->drawEllipse(QPointF(x, y), r, r);
                }
            }
            break;
        }
        case PageTypeFormat::IsoDotted: {
            const QColor dotColor = toQColor(dark ? DOT_COLOR_DARK : DOT_COLOR_LIGHT);
            painter->setPen(Qt::NoPen);
            painter->setBrush(dotColor);
            const double triW = GRAPH_SQUARE_SIZE * scaleX;
            const double triH = GRAPH_SQUARE_SIZE * std::sqrt(3.0) / 2.0 * scaleY;
            const double r = DOT_RADIUS * std::min(scaleX, scaleY);
            int row = 0;
            for (double y = pageRect.top() + triH; y < pageRect.bottom() - 1.0; y += triH) {
                const double offset = (row % 2 == 0) ? 0.0 : triW * 0.5;
                for (double x = pageRect.left() + offset; x < pageRect.right() - 1.0; x += triW) {
                    painter->drawEllipse(QPointF(x, y), r, r);
                }
                ++row;
            }
            break;
        }
        case PageTypeFormat::Staves: {
            const QColor lineColor = toQColor(dark ? Colors::white : Colors::black);
            painter->setPen(QPen(lineColor, STAVES_LINE_WIDTH * scaleY, Qt::SolidLine));
            const double header = STAVES_HEADER * scaleY;
            const double footer = STAVES_FOOTER * scaleY;
            const double margin = STAVES_MARGIN * scaleX;
            const double staffSpacing = STAVES_SPACING * scaleY;
            const double lineSpacing = STAVES_LINE_SPACING * scaleY;
            const double staffHeight = 4.0 * lineSpacing;
            const double bandHeight = staffHeight + staffSpacing;
            for (double bandTop = pageRect.top() + header; bandTop + staffHeight < pageRect.bottom() - footer;
                 bandTop += bandHeight) {
                for (int line = 0; line < 5; ++line) {
                    const double y = bandTop + line * lineSpacing;
                    painter->drawLine(QPointF(pageRect.left() + margin, y), QPointF(pageRect.right() - margin, y));
                }
            }
            break;
        }
        case PageTypeFormat::Pdf: {
            // Raster content was already drawn above if available.
            // Show placeholder only when raster preview is missing.
            if (page.rasterContent.empty()) {
                painter->setPen(Qt::NoPen);
                painter->setBrush(QColor(232, 238, 247));
                painter->drawRoundedRect(
                        QRectF(pageRect.left() + 20.0, pageRect.top() + 20.0, pageRect.width() - 40.0, 54.0), 4.0,
                        4.0);
                painter->setPen(QColor(82, 97, 118));
                painter->drawText(
                        QRectF(pageRect.left() + 34.0, pageRect.top() + 22.0, pageRect.width() - 68.0, 50.0),
                        Qt::AlignLeft | Qt::AlignVCenter,
                        QStringLiteral("PDF background page %1").arg(static_cast<int>(page.pdfPageNumber + 1)));
            }
            break;
        }
        case PageTypeFormat::Image: {
            // Raster content was already drawn above if available.
            if (page.rasterContent.empty()) {
                painter->setPen(Qt::NoPen);
                painter->setBrush(QColor(242, 245, 250));
                painter->drawRoundedRect(
                        QRectF(pageRect.left() + 20.0, pageRect.top() + 20.0, pageRect.width() - 40.0, 54.0), 4.0,
                        4.0);
                painter->setPen(QColor(82, 97, 118));
                painter->drawText(
                        QRectF(pageRect.left() + 34.0, pageRect.top() + 22.0, pageRect.width() - 68.0, 50.0),
                        Qt::AlignLeft | Qt::AlignVCenter, QStringLiteral("Image background"));
            }
            break;
        }
        case PageTypeFormat::Plain:
            break;
        case PageTypeFormat::Ruled:
        case PageTypeFormat::Lined:
        default: {
            const QColor lineColor = toQColor(dark ? RULED_LINE_COLOR_DARK : RULED_LINE_COLOR_LIGHT);
            painter->setPen(QPen(lineColor, RULED_LINE_WIDTH * scaleY, Qt::SolidLine));
            const double header = RULED_HEADER_SIZE * scaleY;
            const double footer = RULED_FOOTER_SIZE * scaleY;
            const double spacing = RULED_LINE_SPACING * scaleY;
            for (double y = pageRect.top() + header; y < pageRect.bottom() - footer; y += spacing) {
                painter->drawLine(QPointF(pageRect.left(), y), QPointF(pageRect.right(), y));
            }
            if (page.backgroundFormat == PageTypeFormat::Lined) {
                const QColor marginColor =
                        toQColor(dark ? LINED_MARGIN_COLOR_DARK : LINED_MARGIN_COLOR_LIGHT);
                painter->setPen(QPen(marginColor, RULED_LINE_WIDTH * scaleX, Qt::SolidLine));
                const double marginX = pageRect.left() + LINED_MARGIN * scaleX;
                painter->drawLine(QPointF(marginX, pageRect.top()), QPointF(marginX, pageRect.bottom()));
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
