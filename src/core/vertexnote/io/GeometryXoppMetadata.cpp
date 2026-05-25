/*
 * VertexNote
 *
 * Compact .xopp metadata for object-based geometry stored on stroke fallbacks.
 */

#include "GeometryXoppMetadata.h"

#include <algorithm>
#include <charconv>
#include <cstdint>
#include <limits>
#include <sstream>
#include <stdexcept>
#include <string_view>

namespace vn::io {

namespace {

constexpr std::string_view LineEdgeName = "line";
constexpr std::string_view ArcEdgeName = "arc";
constexpr std::string_view CubicBezierEdgeName = "cubic-bezier";
constexpr std::string_view ConstructionLineEdgeName = "construction-line";
constexpr std::string_view ConstructionCircleEdgeName = "construction-circle";

constexpr std::string_view CoincidentConstraintName = "coincident";
constexpr std::string_view HorizontalConstraintName = "horizontal";
constexpr std::string_view VerticalConstraintName = "vertical";
constexpr std::string_view ParallelConstraintName = "parallel";
constexpr std::string_view PerpendicularConstraintName = "perpendicular";
constexpr std::string_view EqualLengthConstraintName = "equal-length";
constexpr std::string_view FixedLengthConstraintName = "fixed-length";
constexpr std::string_view FixedAngleConstraintName = "fixed-angle";
constexpr std::string_view RadiusConstraintName = "radius";
constexpr std::string_view OnEdgeConstraintName = "on-edge";

auto edgeKindName(geom::EdgeKind kind) -> std::string_view {
    switch (kind) {
        case geom::EdgeKind::Line:
            return LineEdgeName;
        case geom::EdgeKind::Arc:
            return ArcEdgeName;
        case geom::EdgeKind::CubicBezier:
            return CubicBezierEdgeName;
        case geom::EdgeKind::ConstructionLine:
            return ConstructionLineEdgeName;
        case geom::EdgeKind::ConstructionCircle:
            return ConstructionCircleEdgeName;
    }
    return LineEdgeName;
}

auto parseEdgeKind(std::string_view name) -> std::optional<geom::EdgeKind> {
    if (name == LineEdgeName) {
        return geom::EdgeKind::Line;
    }
    if (name == ArcEdgeName) {
        return geom::EdgeKind::Arc;
    }
    if (name == CubicBezierEdgeName) {
        return geom::EdgeKind::CubicBezier;
    }
    if (name == ConstructionLineEdgeName) {
        return geom::EdgeKind::ConstructionLine;
    }
    if (name == ConstructionCircleEdgeName) {
        return geom::EdgeKind::ConstructionCircle;
    }
    return std::nullopt;
}

auto constraintKindName(geom::ConstraintKind kind) -> std::string_view {
    switch (kind) {
        case geom::ConstraintKind::Coincident:
            return CoincidentConstraintName;
        case geom::ConstraintKind::Horizontal:
            return HorizontalConstraintName;
        case geom::ConstraintKind::Vertical:
            return VerticalConstraintName;
        case geom::ConstraintKind::Parallel:
            return ParallelConstraintName;
        case geom::ConstraintKind::Perpendicular:
            return PerpendicularConstraintName;
        case geom::ConstraintKind::EqualLength:
            return EqualLengthConstraintName;
        case geom::ConstraintKind::FixedLength:
            return FixedLengthConstraintName;
        case geom::ConstraintKind::FixedAngle:
            return FixedAngleConstraintName;
        case geom::ConstraintKind::Radius:
            return RadiusConstraintName;
        case geom::ConstraintKind::OnEdge:
            return OnEdgeConstraintName;
    }
    return CoincidentConstraintName;
}

auto parseConstraintKind(std::string_view name) -> std::optional<geom::ConstraintKind> {
    if (name == CoincidentConstraintName) {
        return geom::ConstraintKind::Coincident;
    }
    if (name == HorizontalConstraintName) {
        return geom::ConstraintKind::Horizontal;
    }
    if (name == VerticalConstraintName) {
        return geom::ConstraintKind::Vertical;
    }
    if (name == ParallelConstraintName) {
        return geom::ConstraintKind::Parallel;
    }
    if (name == PerpendicularConstraintName) {
        return geom::ConstraintKind::Perpendicular;
    }
    if (name == EqualLengthConstraintName) {
        return geom::ConstraintKind::EqualLength;
    }
    if (name == FixedLengthConstraintName) {
        return geom::ConstraintKind::FixedLength;
    }
    if (name == FixedAngleConstraintName) {
        return geom::ConstraintKind::FixedAngle;
    }
    if (name == RadiusConstraintName) {
        return geom::ConstraintKind::Radius;
    }
    if (name == OnEdgeConstraintName) {
        return geom::ConstraintKind::OnEdge;
    }
    return std::nullopt;
}

auto split(std::string_view value, char delimiter) -> std::vector<std::string_view> {
    std::vector<std::string_view> parts;
    while (true) {
        const auto pos = value.find(delimiter);
        parts.push_back(value.substr(0U, pos));
        if (pos == std::string_view::npos) {
            break;
        }
        value.remove_prefix(pos + 1U);
    }
    return parts;
}

template <typename T>
auto parseInteger(std::string_view value) -> std::optional<T> {
    T parsed{};
    const auto* begin = value.data();
    const auto* end = value.data() + value.size();
    const auto [ptr, ec] = std::from_chars(begin, end, parsed);
    if (ec != std::errc{} || ptr != end) {
        return std::nullopt;
    }
    return parsed;
}

auto parseDouble(std::string_view value) -> std::optional<double> {
    double parsed{};
    const auto* begin = value.data();
    const auto* end = value.data() + value.size();
    const auto [ptr, ec] = std::from_chars(begin, end, parsed);
    if (ec != std::errc{} || ptr != end) {
        return std::nullopt;
    }
    return parsed;
}

auto formatDouble(double value) -> std::string {
    std::ostringstream out;
    out.precision(std::numeric_limits<double>::max_digits10);
    out << value;
    return out.str();
}

template <typename Id>
auto joinIds(const std::vector<Id>& ids) -> std::string {
    std::ostringstream out;
    for (auto it = ids.begin(); it != ids.end(); ++it) {
        if (it != ids.begin()) {
            out << "|";
        }
        out << *it;
    }
    return out.str();
}

template <typename Id>
auto parseIds(std::string_view value) -> std::optional<std::vector<Id>> {
    std::vector<Id> ids;
    if (value.empty()) {
        return ids;
    }
    for (const auto part: split(value, '|')) {
        auto parsed = parseInteger<Id>(part);
        if (!parsed) {
            return std::nullopt;
        }
        ids.push_back(*parsed);
    }
    return ids;
}

void setError(std::string* error, const std::string& message) {
    if (error) {
        *error = message;
    }
}

}  // namespace

auto serializeGeometryStrokeMetadata(const geom::GeometryObject& object) -> GeometryStrokeMetadata {
    GeometryStrokeMetadata metadata;
    metadata.format = GeometryFormatV1;
    metadata.objectId = std::to_string(object.objectId());

    std::ostringstream vertices;
    for (const auto& vertex: object.vertices()) {
        if (vertices.tellp() > 0) {
            vertices << ";";
        }
        vertices << vertex.id << "," << formatDouble(vertex.position.x) << "," << formatDouble(vertex.position.y)
                 << "," << formatDouble(vertex.modelPosition.x) << "," << formatDouble(vertex.modelPosition.y)
                 << "," << formatDouble(vertex.modelPosition.z) << ","
                 << static_cast<std::uint32_t>(vertex.flags);
    }
    metadata.vertices = vertices.str();

    std::ostringstream edges;
    for (const auto& edge: object.edges()) {
        if (edges.tellp() > 0) {
            edges << ";";
        }
        edges << edge.id << "," << edgeKindName(edge.kind) << "," << edge.start << "," << edge.end << ","
              << joinIds(edge.controls);
    }
    metadata.edges = edges.str();

    std::ostringstream faces;
    for (const auto& face: object.faces()) {
        if (faces.tellp() > 0) {
            faces << ";";
        }
        faces << face.id << "," << joinIds(face.vertices) << "," << face.fill;
    }
    metadata.faces = faces.str();

    std::ostringstream constraints;
    for (const auto& constraint: object.constraints()) {
        if (constraints.tellp() > 0) {
            constraints << ";";
        }
        constraints << constraint.id << "," << constraintKindName(constraint.kind) << ","
                    << joinIds(constraint.vertices) << "," << joinIds(constraint.edges) << ","
                    << formatDouble(constraint.value);
    }
    metadata.constraints = constraints.str();

    return metadata;
}

auto parseGeometryStrokeMetadata(const GeometryStrokeMetadata& metadata, std::string* error)
        -> std::optional<geom::GeometryObject> {
    if (metadata.format != GeometryFormatV1) {
        setError(error, "Unsupported VertexNote geometry metadata format");
        return std::nullopt;
    }

    const auto objectId = parseInteger<geom::ObjectId>(metadata.objectId);
    if (!objectId || *objectId == geom::InvalidObjectId) {
        setError(error, "Invalid VertexNote geometry object id");
        return std::nullopt;
    }

    geom::GeometryObject object(*objectId);

    if (!metadata.vertices.empty()) {
        for (const auto record: split(metadata.vertices, ';')) {
            const auto fields = split(record, ',');
            if (fields.size() != 4U && fields.size() != 7U) {
                setError(error, "Invalid VertexNote geometry vertex record");
                return std::nullopt;
            }
            const auto vertexId = parseInteger<geom::VertexId>(fields[0]);
            const auto x = parseDouble(fields[1]);
            const auto y = parseDouble(fields[2]);
            const auto modelX = fields.size() == 7U ? parseDouble(fields[3]) : x;
            const auto modelY = fields.size() == 7U ? parseDouble(fields[4]) : y;
            const auto modelZ = fields.size() == 7U ? parseDouble(fields[5]) : std::optional<double>(0.0);
            const auto flags = parseInteger<std::uint32_t>(fields[fields.size() == 7U ? 6U : 3U]);
            if (!vertexId || !x || !y || !modelX || !modelY || !modelZ || !flags ||
                *vertexId == geom::InvalidVertexId) {
                setError(error, "Invalid VertexNote geometry vertex value");
                return std::nullopt;
            }
            object.addVertex3DWithId(*vertexId, geom::Vec3{*modelX, *modelY, *modelZ}, geom::Vec2{*x, *y},
                                     static_cast<geom::VertexFlags>(*flags));
        }
    }

    if (!metadata.edges.empty()) {
        for (const auto record: split(metadata.edges, ';')) {
            const auto fields = split(record, ',');
            if (fields.size() != 5U) {
                setError(error, "Invalid VertexNote geometry edge record");
                return std::nullopt;
            }
            const auto edgeId = parseInteger<geom::EdgeId>(fields[0]);
            const auto kind = parseEdgeKind(fields[1]);
            const auto start = parseInteger<geom::VertexId>(fields[2]);
            const auto end = parseInteger<geom::VertexId>(fields[3]);
            const auto controls = parseIds<geom::VertexId>(fields[4]);
            if (!edgeId || !kind || !start || !end || !controls || *edgeId == geom::InvalidEdgeId) {
                setError(error, "Invalid VertexNote geometry edge value");
                return std::nullopt;
            }
            object.addEdgeWithId(*edgeId, *kind, *start, *end, *controls);
        }
    }

    if (!metadata.faces.empty()) {
        for (const auto record: split(metadata.faces, ';')) {
            const auto fields = split(record, ',');
            if (fields.size() != 3U) {
                setError(error, "Invalid VertexNote geometry face record");
                return std::nullopt;
            }
            const auto faceId = parseInteger<geom::FaceId>(fields[0]);
            const auto vertices = parseIds<geom::VertexId>(fields[1]);
            const auto fill = parseInteger<int>(fields[2]);
            if (!faceId || !vertices || !fill || *faceId == geom::InvalidFaceId) {
                setError(error, "Invalid VertexNote geometry face value");
                return std::nullopt;
            }
            object.addFaceWithId(*faceId, *vertices, *fill);
        }
    }

    if (!metadata.constraints.empty()) {
        for (const auto record: split(metadata.constraints, ';')) {
            const auto fields = split(record, ',');
            if (fields.size() != 5U) {
                setError(error, "Invalid VertexNote geometry constraint record");
                return std::nullopt;
            }
            const auto constraintId = parseInteger<geom::ConstraintId>(fields[0]);
            const auto kind = parseConstraintKind(fields[1]);
            const auto vertices = parseIds<geom::VertexId>(fields[2]);
            const auto edges = parseIds<geom::EdgeId>(fields[3]);
            const auto value = parseDouble(fields[4]);
            if (!constraintId || !kind || !vertices || !edges || !value ||
                *constraintId == geom::InvalidConstraintId) {
                setError(error, "Invalid VertexNote geometry constraint value");
                return std::nullopt;
            }
            object.addConstraintWithId(*constraintId, *kind, *vertices, *edges, *value);
        }
    }

    return object;
}

}  // namespace vn::io
