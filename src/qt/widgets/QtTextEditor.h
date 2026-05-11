/*
 * VertexNote
 *
 * Qt inline text editor overlay for the canvas.
 */

#pragma once

#include <memory>

#include <QPlainTextEdit>

#include "model/Text.h"
#include "util/Color.h"

class QtTextEditor: public QPlainTextEdit {
    Q_OBJECT

public:
    explicit QtTextEditor(QWidget* parent = nullptr);

    void beginEditing(Text* textElement, const QRectF& pageRect, double zoom);
    void beginNewText(std::size_t pageIndex, double pageX, double pageY, const QRectF& pageRect, double zoom,
                      Color color, const std::string& fontName, double fontSize);
    void setTabOptions(bool useSpaces, int numberOfSpaces);

    auto commit() -> bool;
    void cancel();

    [[nodiscard]] auto isEditing() const -> bool;
    [[nodiscard]] auto editedText() const -> Text*;
    [[nodiscard]] auto editedPageIndex() const -> std::size_t;
    [[nodiscard]] auto isNewText() const -> bool;
    [[nodiscard]] auto newTextElement() -> std::unique_ptr<Text>;

Q_SIGNALS:
    void editingFinished(bool committed);

protected:
    void focusOutEvent(QFocusEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;

private:
    void positionOnCanvas(const QRectF& pageRect, double zoom);

private:
    Text* currentText = nullptr;
    std::unique_ptr<Text> ownedNewText;
    std::size_t pageIdx = 0U;
    double textPageX = 0.0;
    double textPageY = 0.0;
    bool useSpacesForTab = false;
    int numberOfSpacesForTab = 4;
    bool editing = false;
    bool isNew = false;
};
