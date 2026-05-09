/*
 * VertexNote
 *
 * Qt inline text editor overlay for the canvas.
 */

#include "QtTextEditor.h"

#include <QFont>
#include <QFontMetrics>
#include <QKeyEvent>

QtTextEditor::QtTextEditor(QWidget* parent): QPlainTextEdit(parent) {
    setFrameStyle(QFrame::Box | QFrame::Plain);
    setLineWidth(1);
    setMinimumSize(80, 30);
    setVisible(false);
    setWordWrapMode(QTextOption::WrapAtWordBoundaryOrAnywhere);
}

void QtTextEditor::beginEditing(Text* textElement, const QRectF& pageRect, double zoom) {
    if (!textElement) {
        return;
    }

    this->currentText = textElement;
    this->ownedNewText.reset();
    this->isNew = false;
    this->editing = true;
    this->textPageX = textElement->getX();
    this->textPageY = textElement->getY();

    // Set font to match the text element
    const auto& noteFont = textElement->getFont();
    QFont font(QString::fromStdString(noteFont.getName()));
    font.setPointSizeF(noteFont.getSize());
    setFont(font);

    // Set text colour
    const auto color = textElement->getColor();
    setStyleSheet(QStringLiteral("QPlainTextEdit { color: rgb(%1,%2,%3); background: rgba(255,255,255,200); }")
                          .arg(color.red)
                          .arg(color.green)
                          .arg(color.blue));

    setPlainText(QString::fromStdString(textElement->getText()));
    textElement->setInEditing(true);

    positionOnCanvas(pageRect, zoom);
    setVisible(true);
    setFocus();
    selectAll();
}

void QtTextEditor::beginNewText(std::size_t pageIndex, double pageX, double pageY, const QRectF& pageRect, double zoom,
                                Color color, const std::string& fontName, double fontSize) {
    this->ownedNewText = std::make_unique<Text>();
    this->ownedNewText->setX(pageX);
    this->ownedNewText->setY(pageY);
    this->ownedNewText->setColor(color);
    NoteFont font;
    font.setName(fontName);
    font.setSize(fontSize);
    this->ownedNewText->setFont(font);

    this->currentText = this->ownedNewText.get();
    this->pageIdx = pageIndex;
    this->textPageX = pageX;
    this->textPageY = pageY;
    this->isNew = true;
    this->editing = true;

    QFont qfont(QString::fromStdString(fontName));
    qfont.setPointSizeF(fontSize);
    setFont(qfont);

    setStyleSheet(QStringLiteral("QPlainTextEdit { color: rgb(%1,%2,%3); background: rgba(255,255,255,200); }")
                          .arg(color.red)
                          .arg(color.green)
                          .arg(color.blue));

    clear();
    positionOnCanvas(pageRect, zoom);
    setVisible(true);
    setFocus();
}

auto QtTextEditor::commit() -> bool {
    if (!this->editing || !this->currentText) {
        return false;
    }

    const QString text = toPlainText().trimmed();
    if (text.isEmpty()) {
        cancel();
        return false;
    }

    this->currentText->setText(text.toStdString());
    this->currentText->setInEditing(false);

    // Approximate width/height from editor
    const QFontMetrics fm(font());
    const auto lines = text.split('\n');
    double maxWidth = 0.0;
    for (const auto& line: lines) {
        maxWidth = std::max(maxWidth, static_cast<double>(fm.horizontalAdvance(line)));
    }
    this->currentText->setWidth(maxWidth + 10.0);
    this->currentText->setHeight(static_cast<double>(fm.lineSpacing() * lines.size()));

    this->editing = false;
    setVisible(false);
    Q_EMIT editingFinished(true);
    return true;
}

void QtTextEditor::cancel() {
    if (this->currentText) {
        this->currentText->setInEditing(false);
    }
    this->editing = false;
    this->currentText = nullptr;
    this->ownedNewText.reset();
    setVisible(false);
    Q_EMIT editingFinished(false);
}

auto QtTextEditor::isEditing() const -> bool { return this->editing; }

auto QtTextEditor::editedText() const -> Text* { return this->currentText; }

auto QtTextEditor::editedPageIndex() const -> std::size_t { return this->pageIdx; }

auto QtTextEditor::isNewText() const -> bool { return this->isNew; }

auto QtTextEditor::newTextElement() -> std::unique_ptr<Text> { return std::move(this->ownedNewText); }

void QtTextEditor::focusOutEvent(QFocusEvent* event) {
    QPlainTextEdit::focusOutEvent(event);
    if (this->editing) {
        commit();
    }
}

void QtTextEditor::keyPressEvent(QKeyEvent* event) {
    if (event->key() == Qt::Key_Escape) {
        cancel();
        event->accept();
        return;
    }
    QPlainTextEdit::keyPressEvent(event);
}

void QtTextEditor::positionOnCanvas(const QRectF& pageRect, double zoom) {
    // Position the editor at the text element's location on the canvas
    const double screenX = (pageRect.x() + this->textPageX) * zoom;
    const double screenY = (pageRect.y() + this->textPageY) * zoom;

    const QFontMetrics fm(font());
    const int minW = 200;
    const int minH = fm.lineSpacing() * 3;

    // Size from existing text
    int w = minW;
    int h = minH;
    if (this->currentText && !this->currentText->getText().empty()) {
        w = std::max(minW, static_cast<int>(this->currentText->getElementWidth() * zoom) + 20);
        h = std::max(minH, static_cast<int>(this->currentText->getElementHeight() * zoom) + 10);
    }

    setGeometry(static_cast<int>(screenX), static_cast<int>(screenY), w, h);
}
