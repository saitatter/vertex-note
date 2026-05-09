/*
 * VertexNote unit tests — TextRenderModelFactory
 */

#include <gtest/gtest.h>

#include "model/Font.h"
#include "model/Text.h"
#include "view/render/TextRenderModelFactory.h"

TEST(VertexNoteTextRenderModelFactory, mapsBasicTextFields) {
    Text text;
    text.setText("Hello world");
    text.setFont(NoteFont("Serif", 14.0));
    text.setColor(Color{0x00, 0x00, 0xff, 0xff});
    text.setX(100.0);
    text.setY(200.0);

    const auto model = vn::view::render::TextRenderModelFactory::fromText(text);

    EXPECT_EQ(model.content, "Hello world");
    EXPECT_EQ(model.fontName, "Serif");
    EXPECT_DOUBLE_EQ(model.fontSize, 14.0);
    EXPECT_EQ(model.color.red, 0x00);
    EXPECT_EQ(model.color.blue, 0xff);
    EXPECT_DOUBLE_EQ(model.x, 100.0);
    EXPECT_DOUBLE_EQ(model.y, 200.0);
    EXPECT_FALSE(model.inEditing);
}

TEST(VertexNoteTextRenderModelFactory, mapsInEditingState) {
    Text text;
    text.setText("editing");
    text.setFont(NoteFont("Sans", 12.0));
    text.setInEditing(true);

    const auto model = vn::view::render::TextRenderModelFactory::fromText(text);

    EXPECT_TRUE(model.inEditing);
    EXPECT_EQ(model.content, "editing");
}

TEST(VertexNoteTextRenderModelFactory, emptyTextProducesEmptyContent) {
    Text text;
    text.setFont(NoteFont("Mono", 10.0));
    // setText not called — default empty

    const auto model = vn::view::render::TextRenderModelFactory::fromText(text);

    EXPECT_TRUE(model.content.empty());
}
