/*
 * VertexNote
 *
 * A font with a name and a size
 *
 * @author VertexNote Team
 * https://github.com/saitatter/vertex-note
 *
 * @license GNU GPLv2 or later
 */

#pragma once

#include <string>  // for string

#include "util/serializing/Serializable.h"  // for Serializable

class ObjectInputStream;
class ObjectOutputStream;


class NoteFont: public Serializable {
public:
    NoteFont() = default;
    NoteFont(std::string name, double size);
    NoteFont(const NoteFont&) = default;
    NoteFont(NoteFont&&) = default;
    ~NoteFont() override = default;

    NoteFont& operator=(const NoteFont&) = default;
    NoteFont& operator=(NoteFont&&) = default;

    /**
     * Set this from a Pango-style font description.
     * E.g.
     *   Serif 12
     * sets this' size to 12 and this font's name to Serif.
     *
     * @param description Pango-style font description.
     */
    explicit NoteFont(const char* description);
    NoteFont& operator=(const std::string& description);

public:
    const std::string& getName() const;
    void setName(std::string name);

    double getSize() const;
    void setSize(double size);

    /**
     * @return The Pango-style string that represents this
     * font.
     */
    std::string asString() const;

public:
    // Serialize interface
    void serialize(ObjectOutputStream& out) const override;
    void readSerialized(ObjectInputStream& in) override;

private:
    void updateFontDesc();

private:
    std::string name;
    double size = 0;
};

using NoteFont = NoteFont;
