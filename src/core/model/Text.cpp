#include "Text.h"

#include <algorithm>
#include <cmath>
#include <iostream>
#include <memory>
#include <utility>  // for move

#include <QFont>
#include <QFontMetricsF>
#include <QString>
#include <QStringList>

#include "model/AudioElement.h"   // for AudioElement
#include "model/Element.h"        // for ELEMENT_TEXT, Eleme...
#include "model/Font.h"           // for NoteFont
#include "pdf/base/PdfPage.h"  // for PdfRectangle
#include "util/Rectangle.h"       // for Rectangle
#include "util/Stacktrace.h"      // for Stacktrace
#include "util/serializing/ObjectInputStream.h"   // for ObjectInputStream
#include "util/serializing/ObjectOutputStream.h"  // for ObjectOutputStream

using vn::util::Rectangle;

namespace {

auto fontForText(const Text& text) -> QFont {
    QFont font(QString::fromStdString(text.getFontName()));
    font.setPixelSize(std::max(1, static_cast<int>(std::round(text.getFontSize()))));
    return font;
}

auto lineStartFor(const QString& text, int position) -> int {
    const int previousBreak = static_cast<int>(text.lastIndexOf(QChar('\n'), std::max(0, position - 1)));
    return previousBreak < 0 ? 0 : previousBreak + 1;
}

auto lineIndexFor(const QString& text, int position) -> int {
    int line = 0;
    for (int i = 0; i < position && i < text.size(); ++i) {
        if (text.at(i) == QChar('\n')) {
            ++line;
        }
    }
    return line;
}

auto advanceFor(const QFontMetricsF& metrics, const QString& text, int start, int length) -> double {
    if (length <= 0) {
        return 0.0;
    }
    return metrics.horizontalAdvance(text.mid(start, length));
}

}  // namespace

Text::Text(): AudioElement(ELEMENT_TEXT) {
    this->font.setName("Sans");
    this->font.setSize(12);
}

Text::~Text() = default;

auto Text::cloneText() const -> std::unique_ptr<Text> {
    auto text = std::make_unique<Text>();
    text->font = this->font;
    text->text = this->text;
    text->setColor(this->getColor());
    text->x = this->x;
    text->y = this->y;
    text->width = this->width;
    text->height = this->height;
    text->cloneAudioData(this);
    text->snappedBounds = this->snappedBounds;
    text->sizeCalculated = this->sizeCalculated;
    text->inEditing = this->inEditing;

    return text;
}

auto Text::clone() const -> ElementPtr { return cloneText(); }

auto Text::getFont() -> NoteFont& { return font; }
auto Text::getFont() const -> const NoteFont& { return font; }

void Text::setFont(const NoteFont& font) {
    this->font = font;
    sizeCalculated = false;
}

auto Text::getFontSize() const -> double { return font.getSize(); }

auto Text::getFontName() const -> std::string { return font.getName(); }

auto Text::getText() const -> const std::string& { return this->text; }

void Text::setText(std::string text) {
    this->text = std::move(text);
    sizeCalculated = false;
}

void Text::calcSize() const {
    const QString content = QString::fromStdString(this->text);
    const QFontMetricsF metrics(fontForText(*this));

    const auto lines = content.split(QChar('\n'), Qt::KeepEmptyParts);
    double maxWidth = 0.0;
    for (const auto& line: lines) {
        maxWidth = std::max(maxWidth, metrics.horizontalAdvance(line));
    }

    this->width = maxWidth;
    this->height = static_cast<double>(std::max<qsizetype>(1, lines.size())) * metrics.height();
    this->updateSnapping();
}

void Text::setWidth(double width) {
    this->width = width;
    this->updateSnapping();
}

void Text::setHeight(double height) {
    this->height = height;
    this->updateSnapping();
}

void Text::setInEditing(bool inEditing) { this->inEditing = inEditing; }

void Text::scale(double x0, double y0, double fx, double fy, double rotation,
                 bool) {  // line width scaling option is not used
    // only proportional scale allowed...
    if (fx != fy) {
        std::cerr << "rescale font with fx != fy not supported: " << fx << " / " << fy << std::endl;
        Stacktrace::printStacktrace();
    }

    this->x -= x0;
    this->x *= fx;
    this->x += x0;
    this->y -= y0;
    this->y *= fy;
    this->y += y0;

    double size = this->font.getSize() * fx;
    this->font.setSize(size);

    sizeCalculated = false;
}

void Text::rotate(double x0, double y0, double th) {}

auto Text::isInEditing() const -> bool { return this->inEditing; }

auto Text::rescaleOnlyAspectRatio() const -> bool { return true; }

void Text::serialize(ObjectOutputStream& out) const {
    out.writeObject("Text");

    this->AudioElement::serialize(out);

    out.writeString(this->text);

    font.serialize(out);

    out.endObject();
}

void Text::readSerialized(ObjectInputStream& in) {
    in.readObject("Text");

    this->AudioElement::readSerialized(in);

    this->text = in.readString();

    font.readSerialized(in);

    in.endObject();
}

void Text::updateSnapping() const {
    this->snappedBounds = Rectangle<double>(this->x, this->y, this->width, this->height);
}

auto Text::findText(const std::string& search) const -> std::vector<PdfRectangle> {
    const QString content = QString::fromStdString(this->text);
    const QString pattern = QString::fromStdString(search);
    if (pattern.isEmpty()) {
        return {};
    }

    const QString lowerText = content.toLower();
    const QString lowerPattern = pattern.toLower();
    const QFontMetricsF metrics(fontForText(*this));
    std::vector<PdfRectangle> list;

    for (int pos = static_cast<int>(lowerText.indexOf(lowerPattern)); pos >= 0;
         pos = static_cast<int>(lowerText.indexOf(lowerPattern, pos + 1))) {
        const int endPos = pos + static_cast<int>(lowerPattern.size());
        const int startLine = lineIndexFor(content, pos);
        const int endLine = lineIndexFor(content, std::max(pos, endPos - 1));
        const int startLineStart = lineStartFor(content, pos);
        const int endLineStart = lineStartFor(content, std::max(pos, endPos - 1));

        PdfRectangle mark;
        mark.x1 = advanceFor(metrics, content, startLineStart, pos - startLineStart) + this->getX();
        mark.y1 = static_cast<double>(startLine) * metrics.height() + this->getY();
        mark.x2 = advanceFor(metrics, content, endLineStart, endPos - endLineStart) + this->getX();
        mark.y2 = (static_cast<double>(endLine) + 1.0) * metrics.height() + this->getY();

        list.push_back(mark);
    }

    return list;
}
