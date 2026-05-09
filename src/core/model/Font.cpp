#include "Font.h"

#include <cstdlib>  // for atof
#include <regex>    // for match_results, rege...
#include <sstream>  // for basic_ostream, oper...
#include <utility>  // for move

#include "util/serdesstream.h"
#include "util/serializing/ObjectInputStream.h"   // for ObjectInputStream
#include "util/serializing/ObjectOutputStream.h"  // for ObjectOutputStream

NoteFont::NoteFont(std::string name, double size) {
    setName(std::move(name));
    setSize(size);
}

auto NoteFont::getName() const -> const std::string& { return this->name; }

void NoteFont::setName(std::string name) { this->name = std::move(name); }

auto NoteFont::getSize() const -> double { return size; }

void NoteFont::setSize(double size) { this->size = size; }

NoteFont::NoteFont(const char* description) {
    // See https://stackoverflow.com/questions/44949784/c-regex-which-group-matched for
    // a good overview of regular expressions in C++.
    std::regex pangoFontDescriptionRegex{"^(.*) (\\d+[.]?\\d*)$"};

    std::match_results<const char*> results;
    std::regex_search(description, results, pangoFontDescriptionRegex);

    if (results.size() > 1) {
        this->name = results[1].str();
    } else {
        this->name = "";
    }

    if (results.size() > 2) {
        this->size = atof(results[2].str().c_str());
    } else {
        this->size = 0;
    }
}

NoteFont& NoteFont::operator=(const std::string& description) { return *this = NoteFont(description.c_str()); }

auto NoteFont::asString() const -> std::string {
    auto result = serdes_stream<std::stringstream>();
    result << getName() << " " << getSize();

    return result.str();
}

void NoteFont::serialize(ObjectOutputStream& out) const {
    out.writeObject("NoteFont");

    out.writeString(this->name);
    out.writeDouble(this->size);

    out.endObject();
}

void NoteFont::readSerialized(ObjectInputStream& in) {
    in.readObject("NoteFont");

    this->name = in.readString();
    this->size = in.readDouble();

    in.endObject();
}
