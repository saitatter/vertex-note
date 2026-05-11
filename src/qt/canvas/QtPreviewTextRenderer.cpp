/*
 * VertexNote
 *
 * Qt preview text renderer for the Qt shell.
 */

#include "QtPreviewTextRenderer.h"

#include <QColor>
#include <QFont>
#include <QPainter>
#include <QRectF>
#include <QTextDocument>
#include <QTextOption>

#include "view/render/QtPainterRenderContext.h"

namespace vn::view::render {

void QtPreviewTextRenderer::draw(const TextRenderModel& text, RenderContext& context) const {
    if (context.backend() != RenderBackend::QtPainter || text.inEditing || text.content.empty()) {
        return;
    }

    auto* painter = static_cast<QtPainterRenderContext&>(context).native();
    if (!painter) {
        return;
    }

    // Text model font sizes are stored in document/device units; Qt pixel sizing preserves that behavior.
    QFont font(QString::fromStdString(text.fontName));
    font.setPixelSize(static_cast<int>(text.fontSize));

    painter->save();
    painter->translate(text.x, text.y);
    painter->setFont(font);
    painter->setPen(QColor(text.color.red, text.color.green, text.color.blue, text.color.alpha));

    // Use QTextDocument for proper multi-line text layout
    QTextDocument doc;
    doc.setDefaultFont(font);
    doc.setDocumentMargin(0.0);
    doc.setPlainText(QString::fromStdString(text.content));

    doc.drawContents(painter);

    painter->restore();
}

}  // namespace vn::view::render
