/*
 * VertexNote
 *
 * VertexNote element wrapper for object-based geometry.
 */

#include "GeometryElement.h"

#include <algorithm>
#include <cmath>
#include <iterator>
#include <limits>
#include <memory>
#include <string>
#include <utility>

#include "model/Point.h"
#include "model/Stroke.h"
#include "util/Rectangle.h"
#include "util/serializing/InputStreamException.h"
#include "util/serializing/ObjectInputStream.h"
#include "util/serializing/ObjectOutputStream.h"
#include "vertexnote/geometry/GeometryIdGenerator.h"
#include "vertexnote/io/GeometryXoppMetadata.h"

using xoj::util::Rectangle;

namespace vn::geom {

namespace {

auto distanceToSegment(Vec2 point, Vec2 start, Vec2 end) -> double {
    const double dx = end.x - start.x;
    const double dy = end.y - start.y;
    const double lengthSquared = dx * dx + dy * dy;

    if (lengthSquared == 0.0) {
        return std::hypot(point.x - start.x, point.y - start.y);
    }

    const double t = std::clamp(((point.x - start.x) * dx + (point.y - start.y) * dy) / lengthSquared, 0.0, 1.0);
    const Vec2 projection{start.x + t * dx, start.y + t * dy};
    return std::hypot(point.x - projection.x, point.y - projection.y);
}

}  // namespace

GeometryElement::GeometryElement(): Element(ELEMENT_GEOMETRY) {}

GeometryElement::GeometryElement(GeometryObject object): Element(ELEMENT_GEOMETRY), object(std::move(object)) {}

auto GeometryElement::geometry() -> GeometryObject& {
    this->sizeCalculated = false;
    return this->object;
}

auto GeometryElement::geometry() const -> const GeometryObject& { return this->object; }

void GeometryElement::setStrokeWidth(double width) {
    this->strokeWidth = width;
    this->sizeCalculated = false;
}

auto GeometryElement::getStrokeWidth() const -> double { return this->strokeWidth; }

auto GeometryElement::makeStrokeFallback() const -> std::unique_ptr<Stroke> {
    return this->object.makeStrokeFallback(this->strokeWidth, this->getColor());
}

void GeometryElement::assignNewObjectId() { this->object.setObjectId(GeometryIdGenerator::nextObjectId()); }

auto GeometryElement::setVertexPosition(VertexId id, Vec2 position) -> bool {
    const bool changed = this->object.setVertexPosition(id, position);
    if (changed) {
        this->sizeCalculated = false;
    }
    return changed;
}

auto GeometryElement::removeVertex(VertexId id) -> bool {
    const bool changed = this->object.removeVertex(id);
    if (changed) {
        this->sizeCalculated = false;
    }
    return changed;
}

auto GeometryElement::insertVertexOnEdge(EdgeId edge, Vec2 position) -> std::optional<VertexId> {
    auto inserted = this->object.insertVertexOnEdge(edge, position);
    if (inserted) {
        this->sizeCalculated = false;
    }
    return inserted;
}

void GeometryElement::move(double dx, double dy) {
    this->object.move(dx, dy);
    if (this->sizeCalculated) {
        Element::move(dx, dy);
    }
}

void GeometryElement::scale(double x0, double y0, double fx, double fy, double rotation, bool restoreLineWidth) {
    this->object.scale(x0, y0, fx, fy, rotation);
    if (!restoreLineWidth) {
        this->strokeWidth *= std::sqrt(std::abs(fx * fy));
    }
    this->sizeCalculated = false;
}

void GeometryElement::rotate(double x0, double y0, double th) {
    this->object.rotate(x0, y0, th);
    this->sizeCalculated = false;
}

auto GeometryElement::distanceTo(double x, double y) const -> double {
    const auto polyline = this->object.toPolyline();
    if (polyline.size() < 2U) {
        return Element::distanceTo(x, y);
    }

    double distance = std::numeric_limits<double>::max();
    const Vec2 point{x, y};
    for (auto it = std::next(polyline.begin()); it != polyline.end(); ++it) {
        distance = std::min(distance, distanceToSegment(point, *std::prev(it), *it));
    }

    return std::max(0.0, distance - 0.5 * this->strokeWidth);
}

auto GeometryElement::clone() const -> ElementPtr {
    auto element = std::make_unique<GeometryElement>(this->object);
    element->setColor(this->getColor());
    element->strokeWidth = this->strokeWidth;
    element->x = this->x;
    element->y = this->y;
    element->width = this->width;
    element->height = this->height;
    element->snappedBounds = this->snappedBounds;
    element->sizeCalculated = this->sizeCalculated;
    return element;
}

void GeometryElement::serialize(ObjectOutputStream& out) const {
    out.writeObject("GeometryElement");

    this->Element::serialize(out);
    out.writeDouble(this->strokeWidth);

    const auto metadata = vn::io::serializeGeometryStrokeMetadata(this->object);
    out.writeString(metadata.format);
    out.writeString(metadata.objectId);
    out.writeString(metadata.vertices);
    out.writeString(metadata.edges);
    out.writeString(metadata.constraints);

    out.endObject();
}

void GeometryElement::readSerialized(ObjectInputStream& in) {
    in.readObject("GeometryElement");

    this->Element::readSerialized(in);
    this->strokeWidth = in.readDouble();

    vn::io::GeometryStrokeMetadata metadata;
    metadata.format = in.readString();
    metadata.objectId = in.readString();
    metadata.vertices = in.readString();
    metadata.edges = in.readString();
    metadata.constraints = in.readString();

    std::string error;
    auto parsed = vn::io::parseGeometryStrokeMetadata(metadata, &error);
    if (!parsed) {
        throw InputStreamException("Could not read VertexNote geometry clipboard data: " + error, __FILE__, __LINE__);
    }

    this->object = std::move(*parsed);
    GeometryIdGenerator::observeObjectId(this->object.objectId());
    this->sizeCalculated = false;

    in.endObject();
}

void GeometryElement::calcSize() const {
    const auto bounds = this->object.bounds();
    if (!bounds) {
        this->x = 0.0;
        this->y = 0.0;
        this->width = 0.0;
        this->height = 0.0;
        this->snappedBounds = Rectangle<double>();
        return;
    }

    this->snappedBounds =
            Rectangle<double>(bounds->minX, bounds->minY, bounds->maxX - bounds->minX, bounds->maxY - bounds->minY);

    const double padding = 0.5 * this->strokeWidth;
    this->x = bounds->minX - padding;
    this->y = bounds->minY - padding;
    this->width = bounds->maxX - bounds->minX + this->strokeWidth;
    this->height = bounds->maxY - bounds->minY + this->strokeWidth;
}

}  // namespace vn::geom
