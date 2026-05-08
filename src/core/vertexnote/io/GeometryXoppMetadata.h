/*
 * VertexNote
 *
 * Compact .xopp metadata for object-based geometry stored on stroke fallbacks.
 */

#pragma once

#include <optional>
#include <string>

#include "vertexnote/geometry/GeometryObject.h"

namespace vn::io {

constexpr auto XoppNamespaceAttr = u8"xmlns:vertexnote";
constexpr auto XoppNamespaceUri = u8"https://vertexnote.app/xopp/1";
constexpr auto GeometryFormatAttr = u8"vertexnote:format";
constexpr auto GeometryObjectIdAttr = u8"vertexnote:object-id";
constexpr auto GeometryVerticesAttr = u8"vertexnote:vertices";
constexpr auto GeometryEdgesAttr = u8"vertexnote:edges";
constexpr auto GeometryConstraintsAttr = u8"vertexnote:constraints";
constexpr auto GeometryFormatV1 = "geometry-v1";

struct GeometryStrokeMetadata {
    std::string format;
    std::string objectId;
    std::string vertices;
    std::string edges;
    std::string constraints;
};

[[nodiscard]] auto serializeGeometryStrokeMetadata(const geom::GeometryObject& object) -> GeometryStrokeMetadata;
[[nodiscard]] auto parseGeometryStrokeMetadata(const GeometryStrokeMetadata& metadata, std::string* error = nullptr)
        -> std::optional<geom::GeometryObject>;

}  // namespace vn::io
