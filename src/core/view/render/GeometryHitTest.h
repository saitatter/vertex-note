/*
 * VertexNote
 *
 * Shared hit-testing helpers for object-based geometry render models.
 */

#pragma once

#include <optional>

#include "vertexnote/snapping/SnapTypes.h"
#include "view/render/Renderers.h"

namespace vn::view::render {

enum class GeometryHitType {
    Vertex,
    Edge,
    Face,
};

struct GeometryHitResult {
    GeometryHitType type = GeometryHitType::Vertex;
    vn::geom::ObjectId objectId = vn::geom::InvalidObjectId;
    vn::geom::VertexId vertexId = vn::geom::InvalidVertexId;
    vn::geom::EdgeId edgeId = vn::geom::InvalidEdgeId;
    vn::geom::FaceId faceId = vn::geom::InvalidFaceId;
    Point point;
    std::optional<vn::snap::SnapKind> snapKind;
    double screenDistance = 0.0;
};

[[nodiscard]] auto hitTestGeometry(const GeometryRenderModel& geometry, double pageX, double pageY, double zoom,
                                   double maxScreenDistance) -> std::optional<GeometryHitResult>;

}  // namespace vn::view::render
