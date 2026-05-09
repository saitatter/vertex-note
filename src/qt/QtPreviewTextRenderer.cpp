/*
 * VertexNote
 *
 * Qt preview text renderer for the experimental shell.
 */

#include "QtPreviewTextRenderer.h"

#include <QColor>
#include <QFont>
#include <QPainter>

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

    QFont font(QString::fromStdString(text.fontName));
    font.setPointSizeF(text.fontSize);

    painter->save();
    painter->setFont(font);
    painter->setPen(QColor(text.color.red, text.color.green, text.color.blue));
    painter->drawText(QPointF(text.x, text.y + text.fontSize), QString::fromStdString(text.content));
    painter->restore();
}

}  // namespace vn::view::render
