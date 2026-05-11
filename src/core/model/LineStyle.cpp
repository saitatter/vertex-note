#include "LineStyle.h"

#include <vector>   // for vector

#include "util/serializing/ObjectInputStream.h"   // for ObjectInputStream
#include "util/serializing/ObjectOutputStream.h"  // for ObjectOutputStream


LineStyle::LineStyle() = default;

LineStyle::~LineStyle() = default;

auto LineStyle::operator==(const LineStyle& rhs) const -> bool { return dashes == rhs.dashes; }

void LineStyle::serialize(ObjectOutputStream& out) const {
    out.writeObject("LineStyle");

    out.writeData(this->dashes);

    out.endObject();
}

void LineStyle::readSerialized(ObjectInputStream& in) {
    in.readObject("LineStyle");

    in.readData(dashes);

    in.endObject();
}

auto LineStyle::getDashes() const -> const std::vector<double>& { return dashes; }

void LineStyle::setDashes(std::vector<double>&& dashes) { this->dashes = std::move(dashes); }

auto LineStyle::hasDashes() const -> bool { return !dashes.empty(); }
