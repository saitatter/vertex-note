/*
 * VertexNote unit tests - Qt preview stroke renderer
 */

#include <gtest/gtest.h>

#include <QColor>
#include <QImage>
#include <QPainter>

#include "model/Stroke.h"
#include "QtPreviewStrokeRenderer.h"
#include "view/render/QtPainterRenderContext.h"

TEST(VertexNoteQtPreviewStrokeRenderer, pressureSensitiveStrokeUsesFinalPointRadiusForRoundCap) {
    QImage image(48, 40, QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::transparent);

    QPainter painter(&image);
    painter.setRenderHint(QPainter::Antialiasing, false);

    vn::view::render::StrokeRenderModel stroke;
    stroke.points = {Point(10.0, 20.0, 4.0), Point(30.0, 20.0, 10.0)};
    stroke.color = Colors::black;
    stroke.width = 10.0;
    stroke.pressureSensitive = true;
    stroke.capStyle = static_cast<int>(StrokeCapStyle::ROUND);

    vn::view::render::QtPainterRenderContext context(&painter);
    vn::view::render::QtPreviewStrokeRenderer renderer;
    renderer.draw(stroke, context);
    painter.end();

    EXPECT_GT(QColor(image.pixel(34, 20)).alpha(), 0);
}
