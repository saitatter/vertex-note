/*
 * VertexNote
 *
 * A text element
 *
 * @author VertexNote Team
 * https://github.com/saitatter/vertex-note
 *
 * @license GNU GPLv2 or later
 */

#pragma once

#include <string>  // for string
#include <vector>

#include "model/Element.h"

#include "AudioElement.h"  // for AudioElement
#include "Font.h"          // for NoteFont

class Element;
class ObjectInputStream;
class ObjectOutputStream;
class PdfRectangle;

class Text: public AudioElement {
public:
    Text();
    ~Text() override;

public:
    void setFont(const NoteFont& font);
    NoteFont& getFont();
    const NoteFont& getFont() const;
    double getFontSize() const;       // same result as getFont()->getSize(), but const
    std::string getFontName() const;  // same result as getFont()->getName(), but const

    const std::string& getText() const;
    void setText(std::string text);

    void setWidth(double width);
    void setHeight(double height);

    void setInEditing(bool inEditing);
    bool isInEditing() const;

    void scale(double x0, double y0, double fx, double fy, double rotation, bool restoreLineWidth) override;
    void rotate(double x0, double y0, double th) override;

    bool rescaleOnlyAspectRatio() const override;

    auto cloneText() const -> std::unique_ptr<Text>;
    auto clone() const -> ElementPtr override;

public:
    // Serialize interface
    void serialize(ObjectOutputStream& out) const override;
    void readSerialized(ObjectInputStream& in) override;

protected:
    void calcSize() const override;
    void updateSnapping() const;

public:
    std::vector<PdfRectangle> findText(const std::string& search) const;

private:
    NoteFont font;

    std::string text;

    bool inEditing = false;
};
