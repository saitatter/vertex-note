/*
 * VertexNote
 *
 * Qt document controller backed by the shared core model.
 */

#include "QtDocumentController.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <limits>
#include <memory>
#include <numeric>
#include <span>
#include <stdexcept>
#include <string_view>
#include <unordered_map>
#include <variant>
#include <unordered_set>
#include <utility>

#include "control/shaperecognizer/ShapeRecognizer.h"
#include "control/xojfile/LoadHandler.h"
#include "control/xojfile/SaveHandler.h"
#include "model/Element.h"
#include "model/Document.h"
#include "model/Image.h"
#include "model/Layer.h"
#include "model/NotePage.h"
#include "model/Text.h"
#include "model/Stroke.h"
#include "model/StrokeStyle.h"
#include "model/SplineSegment.h"
#include "vertexnote/constraints/GeometryConstraintSolver.h"
#include "vertexnote/geometry/GeometryElement.h"
#include "vertexnote/geometry/GeometryIdGenerator.h"
#include "vertexnote/geometry/GeometryProjection.h"
#include "vertexnote/geometry/GeometryTransform.h"
#include "vertexnote/geometry/SurfaceMesh.h"
#include "vertexnote/snapping/GeometrySnapProvider.h"
#include "vertexnote/snapping/GridSnapProvider.h"
#include "vertexnote/snapping/ISnapProvider.h"
#include "vertexnote/snapping/PageGeometryCollector.h"
#include "vertexnote/snapping/SnapEngine.h"
#include "view/render/PageRasterPreviewFactory.h"

namespace {

constexpr double PI = 3.14159265358979323846;
constexpr double CoincidentVertexEpsilon = 1e-6;
constexpr vn::geom::Vec2 TopologyDetachOffset{8.0, 8.0};

[[nodiscard]] auto distance(vn::geom::Vec2 lhs, vn::geom::Vec2 rhs) -> double {
    return std::hypot(rhs.x - lhs.x, rhs.y - lhs.y);
}

[[nodiscard]] auto coincidentPoint(vn::geom::Vec2 lhs, vn::geom::Vec2 rhs) -> bool {
    return distance(lhs, rhs) <= CoincidentVertexEpsilon;
}

[[nodiscard]] auto containsCoincidentPoint(const std::vector<vn::geom::Vec2>& points, vn::geom::Vec2 point) -> bool {
    return std::ranges::any_of(points, [point](vn::geom::Vec2 candidate) {
        return coincidentPoint(candidate, point);
    });
}

[[nodiscard]] auto projectionOnSegment(vn::geom::Vec2 point, vn::geom::Vec2 start, vn::geom::Vec2 end)
        -> std::optional<vn::geom::Vec2> {
    const double dx = end.x - start.x;
    const double dy = end.y - start.y;
    const double lengthSquared = dx * dx + dy * dy;
    if (lengthSquared <= 1e-9) {
        return std::nullopt;
    }

    const double t = ((point.x - start.x) * dx + (point.y - start.y) * dy) / lengthSquared;
    if (t < 0.0 || t > 1.0) {
        return std::nullopt;
    }
    return vn::geom::Vec2{.x = start.x + t * dx, .y = start.y + t * dy};
}

[[nodiscard]] auto containsVertexId(const std::vector<vn::geom::VertexId>& vertices, vn::geom::VertexId vertex) -> bool {
    return std::find(vertices.begin(), vertices.end(), vertex) != vertices.end();
}

template <typename Id>
void appendUnique(std::vector<Id>& ids, Id id) {
    if (std::find(ids.begin(), ids.end(), id) == ids.end()) {
        ids.push_back(id);
    }
}

[[nodiscard]] auto edgeReferencesVertex(const vn::geom::Edge& edge, vn::geom::VertexId vertexId) -> bool {
    return edge.start == vertexId || edge.end == vertexId ||
           std::find(edge.controls.begin(), edge.controls.end(), vertexId) != edge.controls.end();
}

[[nodiscard]] auto isLineLikeEdge(const vn::geom::Edge& edge) -> bool {
    return edge.kind == vn::geom::EdgeKind::Line || edge.kind == vn::geom::EdgeKind::ConstructionLine;
}

[[nodiscard]] auto isOnEdgeSupportedEdge(const vn::geom::Edge& edge) -> bool {
    return isLineLikeEdge(edge) || edge.kind == vn::geom::EdgeKind::Arc ||
           edge.kind == vn::geom::EdgeKind::ConstructionCircle;
}

[[nodiscard]] auto edgeAngle(const vn::geom::GeometryObject& object, const vn::geom::Edge& edge)
        -> std::optional<double> {
    const auto* start = object.vertex(edge.start);
    const auto* end = object.vertex(edge.end);
    if (!start || !end) {
        return std::nullopt;
    }
    const double dx = end->position.x - start->position.x;
    const double dy = end->position.y - start->position.y;
    if (std::hypot(dx, dy) <= 1e-9) {
        return std::nullopt;
    }
    return std::atan2(dy, dx);
}

void replaceEdgeVertexReference(vn::geom::Edge& edge, vn::geom::VertexId oldVertexId,
                                vn::geom::VertexId newVertexId) {
    if (edge.start == oldVertexId) {
        edge.start = newVertexId;
    }
    if (edge.end == oldVertexId) {
        edge.end = newVertexId;
    }
    for (auto& control: edge.controls) {
        if (control == oldVertexId) {
            control = newVertexId;
        }
    }
}

[[nodiscard]] auto edgeVertexReferences(const vn::geom::Edge& edge) -> std::vector<vn::geom::VertexId> {
    std::vector<vn::geom::VertexId> result;
    appendUnique(result, edge.start);
    appendUnique(result, edge.end);
    for (auto control: edge.controls) {
        appendUnique(result, control);
    }
    result.erase(std::remove(result.begin(), result.end(), vn::geom::InvalidVertexId), result.end());
    return result;
}

[[nodiscard]] auto incidentEdgeIds(const vn::geom::GeometryObject& object, vn::geom::VertexId vertexId)
        -> std::vector<vn::geom::EdgeId> {
    std::vector<vn::geom::EdgeId> result;
    for (const auto& edge: object.edges()) {
        if (edgeReferencesVertex(edge, vertexId)) {
            result.push_back(edge.id);
        }
    }
    return result;
}

[[nodiscard]] auto sameVertexSet(const std::vector<vn::geom::VertexId>& lhs,
                                 const std::vector<vn::geom::VertexId>& rhs) -> bool {
    if (lhs.size() != rhs.size()) {
        return false;
    }
    std::unordered_set<vn::geom::VertexId> values(lhs.begin(), lhs.end());
    return std::ranges::all_of(rhs, [&values](auto vertexId) { return values.contains(vertexId); });
}

[[nodiscard]] auto orderedClosedLineLoop(const vn::geom::GeometryObject& object,
                                         const std::vector<vn::geom::EdgeId>& edgeIds)
        -> std::optional<std::vector<vn::geom::VertexId>> {
    std::vector<const vn::geom::Edge*> edges;
    if (edgeIds.empty()) {
        edges.reserve(object.edges().size());
        for (const auto& edge: object.edges()) {
            if (edge.kind == vn::geom::EdgeKind::Line) {
                edges.push_back(&edge);
            } else if (edge.kind != vn::geom::EdgeKind::ConstructionLine) {
                return std::nullopt;
            }
        }
    } else {
        edges.reserve(edgeIds.size());
        for (auto edgeId: edgeIds) {
            const auto* edge = object.edge(edgeId);
            if (!edge || edge->kind != vn::geom::EdgeKind::Line) {
                return std::nullopt;
            }
            edges.push_back(edge);
        }
    }
    if (edges.size() < 3U) {
        return std::nullopt;
    }

    std::unordered_map<vn::geom::VertexId, std::vector<vn::geom::VertexId>> adjacency;
    for (const auto* edge: edges) {
        adjacency[edge->start].push_back(edge->end);
        adjacency[edge->end].push_back(edge->start);
    }
    for (const auto& [_, neighbors]: adjacency) {
        if (neighbors.size() != 2U) {
            return std::nullopt;
        }
    }

    const auto start = edges.front()->start;
    std::vector<vn::geom::VertexId> loop{start};
    std::unordered_set<vn::geom::VertexId> visited{start};
    auto previous = vn::geom::InvalidVertexId;
    auto current = start;
    for (std::size_t step = 0; step < edges.size(); ++step) {
        const auto& neighbors = adjacency[current];
        const auto next = neighbors[0] == previous ? neighbors[1] : neighbors[0];
        if (next == start) {
            return loop.size() == edges.size() ? std::optional<std::vector<vn::geom::VertexId>>{loop} : std::nullopt;
        }
        if (!visited.insert(next).second) {
            return std::nullopt;
        }
        loop.push_back(next);
        previous = current;
        current = next;
    }
    return std::nullopt;
}

[[nodiscard]] auto evaluateClosedLineLoop(const vn::geom::GeometryObject& object,
                                          const std::vector<vn::geom::EdgeId>& edgeIds)
        -> QtGeometryFaceLoopStatus {
    if (edgeIds.empty()) {
        return {.kind = QtGeometryFaceLoopStatusKind::NoEdges,
                .message = "Select the edges that form one closed loop"};
    }
    if (edgeIds.size() < 3U) {
        return {.kind = QtGeometryFaceLoopStatusKind::NeedMoreEdges,
                .message = "Fill needs at least three selected line edges"};
    }

    std::unordered_map<vn::geom::VertexId, std::size_t> degree;
    for (auto edgeId: edgeIds) {
        const auto* edge = object.edge(edgeId);
        if (!edge) {
            return {.kind = QtGeometryFaceLoopStatusKind::OpenOrBranching,
                    .message = "Selected loop contains a missing edge"};
        }
        if (edge->kind != vn::geom::EdgeKind::Line) {
            return {.kind = QtGeometryFaceLoopStatusKind::UnsupportedEdge,
                    .message = "Fill currently supports straight line loops only"};
        }
        ++degree[edge->start];
        ++degree[edge->end];
    }

    if (degree.size() != edgeIds.size()) {
        return {.kind = QtGeometryFaceLoopStatusKind::OpenOrBranching,
                .message = "Loop is open or branching; select one closed chain"};
    }
    for (const auto& [_, count]: degree) {
        if (count != 2U) {
            return {.kind = QtGeometryFaceLoopStatusKind::OpenOrBranching,
                    .message = "Loop is open or branching; every vertex must connect to two selected edges"};
        }
    }

    auto loop = orderedClosedLineLoop(object, edgeIds);
    if (!loop) {
        return {.kind = QtGeometryFaceLoopStatusKind::OpenOrBranching,
                .message = "Loop is not closed; select one continuous boundary"};
    }
    if (std::ranges::any_of(object.faces(), [&loop](const auto& face) {
            return sameVertexSet(face.vertices, *loop);
        })) {
        return {.kind = QtGeometryFaceLoopStatusKind::AlreadyFilled,
                .loop = *loop,
                .message = "This loop already has a face"};
    }

    return {.kind = QtGeometryFaceLoopStatusKind::Ready,
            .loop = *loop,
            .message = "Closed loop ready for Fill"};
}

[[nodiscard]] auto fillCandidateEdgeIds(const vn::geom::GeometryObject& object,
                                        const std::vector<vn::geom::EdgeId>& selectedEdgeIds)
        -> std::vector<vn::geom::EdgeId> {
    if (!selectedEdgeIds.empty()) {
        return selectedEdgeIds;
    }

    std::vector<vn::geom::EdgeId> edgeIds;
    edgeIds.reserve(object.edges().size());
    for (const auto& edge: object.edges()) {
        if (edge.kind == vn::geom::EdgeKind::ConstructionLine) {
            continue;
        }
        edgeIds.push_back(edge.id);
    }
    return edgeIds;
}

[[nodiscard]] auto hasLineBetween(const vn::geom::GeometryObject& object, vn::geom::VertexId lhs,
                                  vn::geom::VertexId rhs) -> bool {
    return std::ranges::any_of(object.edges(), [lhs, rhs](const auto& edge) {
        return edge.kind == vn::geom::EdgeKind::Line &&
               ((edge.start == lhs && edge.end == rhs) || (edge.start == rhs && edge.end == lhs));
    });
}

void ensureLineBetween(vn::geom::GeometryObject& object, vn::geom::VertexId lhs, vn::geom::VertexId rhs) {
    if (lhs == rhs || hasLineBetween(object, lhs, rhs)) {
        return;
    }
    object.addLine(lhs, rhs);
}

[[nodiscard]] auto allGeometryVertexIds(const vn::geom::GeometryObject& object) -> std::vector<vn::geom::VertexId> {
    std::vector<vn::geom::VertexId> result;
    result.reserve(object.vertices().size());
    for (const auto& vertex: object.vertices()) {
        result.push_back(vertex.id);
    }
    return result;
}

void normalizePageSpaceModelForProjection(vn::geom::GeometryObject& object,
                                          std::span<const vn::geom::VertexId> vertexIds,
                                          const vn::geom::ProjectionCamera& camera) {
    const bool pageSpaceModel = std::ranges::all_of(vertexIds, [&](auto vertexId) {
        const auto* vertex = object.vertex(vertexId);
        return vertex && std::abs(vertex->modelPosition.x - vertex->position.x) <= 1e-6 &&
               std::abs(vertex->modelPosition.y - vertex->position.y) <= 1e-6 &&
               std::abs(vertex->modelPosition.z) <= 1e-6;
    });
    if (!pageSpaceModel) {
        return;
    }

    for (auto vertexId: vertexIds) {
        if (auto* vertex = object.vertex(vertexId)) {
            static_cast<void>(object.setVertexModelPosition(
                    vertexId, vn::geom::Vec3{vertex->position.x - camera.offset.x,
                                             vertex->position.y - camera.offset.y,
                                             vertex->modelPosition.z}));
        }
    }
}

[[nodiscard]] auto facePathBetween(const std::vector<vn::geom::VertexId>& vertices, std::size_t start,
                                   std::size_t end) -> std::vector<vn::geom::VertexId> {
    std::vector<vn::geom::VertexId> result;
    if (vertices.empty()) {
        return result;
    }
    for (std::size_t index = start;; index = (index + 1U) % vertices.size()) {
        result.push_back(vertices[index]);
        if (index == end) {
            break;
        }
    }
    return result;
}

[[nodiscard]] auto splitFaceByIndices(vn::geom::GeometryObject& object, vn::geom::FaceId faceId,
                                      std::size_t lhsIndex, std::size_t rhsIndex) -> bool {
    const auto* face = object.face(faceId);
    if (!face || face->vertices.size() < 4U || lhsIndex >= face->vertices.size() || rhsIndex >= face->vertices.size()) {
        return false;
    }

    if (lhsIndex == rhsIndex) {
        return false;
    }
    if (lhsIndex > rhsIndex) {
        std::swap(lhsIndex, rhsIndex);
    }
    if (rhsIndex == lhsIndex + 1U || (lhsIndex == 0U && rhsIndex + 1U == face->vertices.size())) {
        return false;
    }

    const auto vertices = face->vertices;
    const int fill = face->fill;
    const auto lhs = vertices[lhsIndex];
    const auto rhs = vertices[rhsIndex];

    ensureLineBetween(object, lhs, rhs);
    if (!object.removeFace(faceId)) {
        return false;
    }

    object.addFace(facePathBetween(vertices, lhsIndex, rhsIndex), fill);
    object.addFace(facePathBetween(vertices, rhsIndex, lhsIndex), fill);
    return true;
}

[[nodiscard]] auto triangulateFace(vn::geom::GeometryObject& object, vn::geom::FaceId faceId) -> bool {
    const auto* face = object.face(faceId);
    if (!face || face->vertices.size() < 4U) {
        return false;
    }

    const auto vertices = face->vertices;
    const int fill = face->fill;
    const auto anchor = vertices.front();
    for (std::size_t index = 2U; index + 1U < vertices.size(); ++index) {
        ensureLineBetween(object, anchor, vertices[index]);
    }
    if (!object.removeFace(faceId)) {
        return false;
    }
    for (std::size_t index = 1U; index + 1U < vertices.size(); ++index) {
        object.addFace({anchor, vertices[index], vertices[index + 1U]}, fill);
    }
    return true;
}

void removeConstraintsReferencing(vn::geom::GeometryObject& object,
                                  const std::vector<vn::geom::VertexId>& vertexIds,
                                  const std::vector<vn::geom::EdgeId>& edgeIds) {
    if (vertexIds.empty() && edgeIds.empty()) {
        return;
    }

    const std::unordered_set<vn::geom::VertexId> vertexSet(vertexIds.begin(), vertexIds.end());
    const std::unordered_set<vn::geom::EdgeId> edgeSet(edgeIds.begin(), edgeIds.end());
    std::vector<vn::geom::ConstraintId> removedConstraints;
    for (const auto& constraint: object.constraints()) {
        const bool referencesVertex = std::ranges::any_of(constraint.vertices, [&vertexSet](auto vertexId) {
            return vertexSet.contains(vertexId);
        });
        const bool referencesEdge = std::ranges::any_of(constraint.edges, [&edgeSet](auto edgeId) {
            return edgeSet.contains(edgeId);
        });
        if (referencesVertex || referencesEdge) {
            removedConstraints.push_back(constraint.id);
        }
    }
    for (auto constraintId: removedConstraints) {
        (void)object.removeConstraint(constraintId);
    }
}

[[nodiscard]] auto snapToEditableSelfEdge(const vn::geom::GeometryObject& object,
                                          const std::vector<vn::geom::VertexId>& movingVertices,
                                          vn::geom::Vec2 queryPoint, double zoom, double maxScreenDistance)
        -> std::optional<vn::snap::SnapCandidate> {
    std::optional<vn::snap::SnapCandidate> best;
    for (const auto& edge: object.edges()) {
        if ((edge.kind != vn::geom::EdgeKind::Line && edge.kind != vn::geom::EdgeKind::ConstructionLine) ||
            containsVertexId(movingVertices, edge.start) || containsVertexId(movingVertices, edge.end)) {
            continue;
        }

        const auto* start = object.vertex(edge.start);
        const auto* end = object.vertex(edge.end);
        if (!start || !end) {
            continue;
        }
        auto projection = projectionOnSegment(queryPoint, start->position, end->position);
        if (!projection) {
            continue;
        }

        const double screenDistance = distance(queryPoint, *projection) * zoom;
        if (screenDistance > maxScreenDistance) {
            continue;
        }
        if (!best || screenDistance < best->screenDistance) {
            best = vn::snap::SnapCandidate{.kind = vn::snap::SnapKind::EdgeProjection,
                                           .pagePoint = *projection,
                                           .screenDistance = screenDistance,
                                           .priority = 50.0,
                                           .object = object.objectId(),
                                           .vertex = vn::geom::InvalidVertexId,
                                           .edge = edge.id};
        }
    }
    return best;
}

[[nodiscard]] auto snapToEditableSelfVertex(const vn::geom::GeometryObject& object,
                                            const std::vector<vn::geom::VertexId>& movingVertices,
                                            vn::geom::Vec2 queryPoint, double zoom, double maxScreenDistance)
        -> std::optional<vn::snap::SnapCandidate> {
    std::optional<vn::snap::SnapCandidate> best;
    for (const auto& vertex: object.vertices()) {
        if (containsVertexId(movingVertices, vertex.id)) {
            continue;
        }

        const double screenDistance = distance(queryPoint, vertex.position) * zoom;
        if (screenDistance > maxScreenDistance) {
            continue;
        }
        if (!best || screenDistance < best->screenDistance) {
            best = vn::snap::SnapCandidate{.kind = vn::snap::SnapKind::ExplicitVertex,
                                           .pagePoint = vertex.position,
                                           .screenDistance = screenDistance,
                                           .priority = 100.0,
                                           .object = object.objectId(),
                                           .vertex = vertex.id,
                                           .edge = vn::geom::InvalidEdgeId};
        }
    }
    return best;
}

[[nodiscard]] auto mergedGeometryForEdgeWeld(const vn::geom::GeometryObject& source,
                                             const vn::geom::GeometryObject& target,
                                             vn::geom::EdgeId targetEdgeId,
                                             vn::geom::VertexId weldVertexId)
        -> std::optional<vn::geom::GeometryObject> {
    const auto* targetEdge = target.edge(targetEdgeId);
    if (!targetEdge || !source.vertex(weldVertexId) ||
        (targetEdge->kind != vn::geom::EdgeKind::Line && targetEdge->kind != vn::geom::EdgeKind::ConstructionLine)) {
        return std::nullopt;
    }

    vn::geom::GeometryObject merged = source;
    std::unordered_map<vn::geom::VertexId, vn::geom::VertexId> vertexMap;
    std::unordered_map<vn::geom::EdgeId, std::vector<vn::geom::EdgeId>> edgeMap;

    try {
        for (const auto& vertex: target.vertices()) {
            vertexMap[vertex.id] = merged.addVertex(vertex.position, vertex.flags);
        }

        const auto splitTargetEdge = [&](const vn::geom::Edge& edge) -> bool {
            const auto startIt = vertexMap.find(edge.start);
            const auto endIt = vertexMap.find(edge.end);
            if (startIt == vertexMap.end() || endIt == vertexMap.end()) {
                return false;
            }
            const auto first = merged.addEdge(edge.kind, startIt->second, weldVertexId);
            const auto second = merged.addEdge(edge.kind, weldVertexId, endIt->second);
            edgeMap[edge.id] = {first, second};
            return true;
        };

        for (const auto& edge: target.edges()) {
            if (edge.id == targetEdgeId) {
                if (!splitTargetEdge(edge)) {
                    return std::nullopt;
                }
                continue;
            }

            const auto startIt = vertexMap.find(edge.start);
            const auto endIt = vertexMap.find(edge.end);
            if (startIt == vertexMap.end() || endIt == vertexMap.end()) {
                continue;
            }
            std::vector<vn::geom::VertexId> controls;
            controls.reserve(edge.controls.size());
            bool validControls = true;
            for (const auto controlId: edge.controls) {
                const auto controlIt = vertexMap.find(controlId);
                if (controlIt == vertexMap.end()) {
                    validControls = false;
                    break;
                }
                controls.push_back(controlIt->second);
            }
            if (!validControls) {
                continue;
            }
            edgeMap[edge.id] = {merged.addEdge(edge.kind, startIt->second, endIt->second, std::move(controls))};
        }

        for (const auto& constraint: target.constraints()) {
            std::vector<vn::geom::VertexId> vertices;
            vertices.reserve(constraint.vertices.size());
            bool valid = true;
            for (const auto vertexId: constraint.vertices) {
                const auto vertexIt = vertexMap.find(vertexId);
                if (vertexIt == vertexMap.end()) {
                    valid = false;
                    break;
                }
                vertices.push_back(vertexIt->second);
            }
            if (!valid) {
                continue;
            }

            std::vector<vn::geom::EdgeId> edges;
            edges.reserve(constraint.edges.size());
            for (const auto edgeId: constraint.edges) {
                const auto edgeIt = edgeMap.find(edgeId);
                if (edgeIt == edgeMap.end() || edgeIt->second.size() != 1U) {
                    valid = false;
                    break;
                }
                edges.push_back(edgeIt->second.front());
            }
            if (!valid) {
                continue;
            }
            merged.addConstraint(constraint.kind, std::move(vertices), std::move(edges), constraint.value);
        }
    } catch (const std::invalid_argument&) {
        return std::nullopt;
    }

    return merged;
}

[[nodiscard]] auto mergedGeometryForVertexWeld(const vn::geom::GeometryObject& source,
                                               const vn::geom::GeometryObject& target,
                                               vn::geom::VertexId targetVertexId,
                                               vn::geom::VertexId weldVertexId)
        -> std::optional<vn::geom::GeometryObject> {
    if (!source.vertex(weldVertexId) || !target.vertex(targetVertexId)) {
        return std::nullopt;
    }

    vn::geom::GeometryObject merged = source;
    std::unordered_map<vn::geom::VertexId, vn::geom::VertexId> vertexMap;
    std::unordered_map<vn::geom::EdgeId, vn::geom::EdgeId> edgeMap;
    vertexMap[targetVertexId] = weldVertexId;

    try {
        for (const auto& vertex: target.vertices()) {
            if (vertex.id == targetVertexId) {
                continue;
            }
            vertexMap[vertex.id] = merged.addVertex(vertex.position, vertex.flags);
        }

        for (const auto& edge: target.edges()) {
            const auto startIt = vertexMap.find(edge.start);
            const auto endIt = vertexMap.find(edge.end);
            if (startIt == vertexMap.end() || endIt == vertexMap.end() ||
                (startIt->second == endIt->second &&
                 (edge.kind == vn::geom::EdgeKind::Line || edge.kind == vn::geom::EdgeKind::ConstructionLine))) {
                continue;
            }

            std::vector<vn::geom::VertexId> controls;
            controls.reserve(edge.controls.size());
            bool validControls = true;
            for (const auto controlId: edge.controls) {
                const auto controlIt = vertexMap.find(controlId);
                if (controlIt == vertexMap.end()) {
                    validControls = false;
                    break;
                }
                controls.push_back(controlIt->second);
            }
            if (!validControls) {
                continue;
            }
            edgeMap[edge.id] = merged.addEdge(edge.kind, startIt->second, endIt->second, std::move(controls));
        }

        for (const auto& constraint: target.constraints()) {
            std::vector<vn::geom::VertexId> vertices;
            vertices.reserve(constraint.vertices.size());
            bool valid = true;
            for (const auto vertexId: constraint.vertices) {
                const auto vertexIt = vertexMap.find(vertexId);
                if (vertexIt == vertexMap.end()) {
                    valid = false;
                    break;
                }
                vertices.push_back(vertexIt->second);
            }
            if (!valid) {
                continue;
            }

            std::ranges::sort(vertices);
            vertices.erase(std::unique(vertices.begin(), vertices.end()), vertices.end());

            std::vector<vn::geom::EdgeId> edges;
            edges.reserve(constraint.edges.size());
            for (const auto edgeId: constraint.edges) {
                const auto edgeIt = edgeMap.find(edgeId);
                if (edgeIt == edgeMap.end()) {
                    valid = false;
                    break;
                }
                edges.push_back(edgeIt->second);
            }
            if (!valid) {
                continue;
            }
            merged.addConstraint(constraint.kind, std::move(vertices), std::move(edges), constraint.value);
        }
    } catch (const std::invalid_argument&) {
        return std::nullopt;
    }

    return merged;
}

}  // namespace

QtDocumentController::QtDocumentController() { newBlankDocument(); }

void QtDocumentController::newBlankDocument() {
    this->document = std::make_unique<Document>(&this->documentHandler);
    this->document->lock();
    this->document->addPage(std::make_shared<NotePage>(595.0, 842.0));
    this->document->unlock();
    this->loadedPath.reset();
    clearPdfRasterCache();
    clearGeometryHistory();
    clearInteractiveGeometryState();
    this->activePdfTextSelection.reset();
    rebuildPageSnapshots();
}

auto QtDocumentController::loadFrom(const std::filesystem::path& path, std::string* errorMessage) -> bool {
    try {
        if (isPdfPath(path)) {
            return loadPdfAsDocument(path, false, errorMessage);
        }

        LoadHandler loader;
        auto loaded = loader.loadDocument(path);
        this->document = std::move(loaded);
        this->loadedPath = path;
        clearPdfRasterCache();
        clearGeometryHistory();
        clearInteractiveGeometryState();
        this->activePdfTextSelection.reset();
        rebuildPageSnapshots();
        return true;
    } catch (const std::exception& e) {
        if (errorMessage) {
            *errorMessage = e.what();
        }
        return false;
    }
}

auto QtDocumentController::loadPdfAsDocument(const std::filesystem::path& path, bool attachToDocument,
                                             std::string* errorMessage) -> bool {
    try {
        auto loaded = std::make_unique<Document>(&this->documentHandler);
        if (!loaded->readPdf(path, true, attachToDocument)) {
            if (errorMessage) {
                *errorMessage = loaded->getLastErrorMsg();
            }
            return false;
        }
        loaded->setFilepath(path);
        this->document = std::move(loaded);
        this->loadedPath = path;
        clearPdfRasterCache();
        clearHistory();
        clearGeometryHistory();
        clearInteractiveGeometryState();
        this->activePdfTextSelection.reset();
        rebuildPageSnapshots();
        return true;
    } catch (const std::exception& e) {
        if (errorMessage) {
            *errorMessage = e.what();
        }
        return false;
    }
}

auto QtDocumentController::hasDocument() const -> bool { return static_cast<bool>(this->document); }

auto QtDocumentController::pageCount() const -> std::size_t {
    if (!this->document) {
        return 0U;
    }
    this->document->lock_shared();
    const auto count = this->document->getPageCount();
    this->document->unlock_shared();
    return count;
}

auto QtDocumentController::hasPdfBackgroundDocument() const -> bool {
    if (!this->document) {
        return false;
    }
    this->document->lock_shared();
    const bool hasPdf = this->document->getPdfPageCount() > 0U;
    this->document->unlock_shared();
    return hasPdf;
}

auto QtDocumentController::snapshotPages() const -> const std::vector<vn::view::render::PageRenderSnapshot>& {
    return this->pageSnapshots;
}

void QtDocumentController::preparePdfRasterCache(const std::vector<std::size_t>& visiblePageIndices) {
    if (!this->document || visiblePageIndices.empty() || this->pageSnapshots.empty()) {
        return;
    }

    std::unordered_set<std::size_t> wantedPageIndices;
    for (const auto pageIndex: visiblePageIndices) {
        const auto first = pageIndex >= static_cast<std::size_t>(this->pdfPreloadPagesBefore)
                                   ? pageIndex - static_cast<std::size_t>(this->pdfPreloadPagesBefore)
                                   : 0U;
        const auto last = std::min(this->pageSnapshots.size() - 1U,
                                   pageIndex + static_cast<std::size_t>(std::max(0, this->pdfPreloadPagesAfter)));
        for (std::size_t index = first; index <= last; ++index) {
            wantedPageIndices.insert(index);
        }
    }

    for (const auto pageIndex: wantedPageIndices) {
        if (pageIndex >= this->pageSnapshots.size()) {
            continue;
        }
        auto& snapshot = this->pageSnapshots[pageIndex];
        auto& background = snapshot.background;
        if (background.backgroundFormat != PageTypeFormat::Pdf) {
            continue;
        }
        background.rasterContent =
                cachedPdfRaster(background.pdfPageNumber, background.pageWidth, background.pageHeight);
    }

    if (this->pdfEagerPageCleanup) {
        std::unordered_set<std::size_t> wantedPdfPages;
        for (const auto pageIndex: wantedPageIndices) {
            if (pageIndex < this->pageSnapshots.size() &&
                this->pageSnapshots[pageIndex].background.backgroundFormat == PageTypeFormat::Pdf) {
                wantedPdfPages.insert(this->pageSnapshots[pageIndex].background.pdfPageNumber);
            }
        }
        this->pdfRasterCache.erase(
                std::remove_if(this->pdfRasterCache.begin(), this->pdfRasterCache.end(),
                               [&wantedPdfPages](const QtPdfRasterCacheEntry& entry) {
                                   return !wantedPdfPages.contains(entry.pdfPageNumber);
                               }),
                this->pdfRasterCache.end());
        for (auto& snapshot: this->pageSnapshots) {
            if (snapshot.background.backgroundFormat == PageTypeFormat::Pdf &&
                !wantedPdfPages.contains(snapshot.background.pdfPageNumber)) {
                snapshot.background.rasterContent = {};
            }
        }
    }

    prunePdfRasterCache();
}

void QtDocumentController::setPdfCacheOptions(int pageCacheSize, int preloadPagesBefore, int preloadPagesAfter,
                                              bool eagerCleanup, double pageRerenderThreshold) {
    this->pdfPageCacheSize = std::clamp(pageCacheSize, 1, 500);
    this->pdfPreloadPagesBefore = std::clamp(preloadPagesBefore, 0, 50);
    this->pdfPreloadPagesAfter = std::clamp(preloadPagesAfter, 0, 50);
    this->pdfEagerPageCleanup = eagerCleanup;
    this->pdfPageRerenderThreshold = std::clamp(pageRerenderThreshold, 0.0, 100.0);
    prunePdfRasterCache();
}

auto QtDocumentController::sourcePath() const -> const std::optional<std::filesystem::path>& {
    return this->loadedPath;
}

auto QtDocumentController::titleText() const -> std::string {
    if (this->loadedPath) {
        return this->loadedPath->filename().string();
    }
    return "Untitled Document";
}

auto QtDocumentController::hitTestGeometry(std::size_t pageIndex, double pageX, double pageY, double zoom,
                                                       double maxScreenDistance) const
        -> std::optional<QtGeometryHit> {
    return hitTestGeometry(pageIndex, pageX, pageY, zoom, maxScreenDistance, true, true, true);
}

auto QtDocumentController::hitTestGeometry(std::size_t pageIndex, double pageX, double pageY, double zoom,
                                           double maxScreenDistance, bool includeVertices, bool includeEdges) const
        -> std::optional<QtGeometryHit> {
    return hitTestGeometry(pageIndex, pageX, pageY, zoom, maxScreenDistance, includeVertices, includeEdges, false);
}

auto QtDocumentController::hitTestGeometry(std::size_t pageIndex, double pageX, double pageY, double zoom,
                                           double maxScreenDistance, bool includeVertices, bool includeEdges,
                                           bool includeFaces) const
        -> std::optional<QtGeometryHit> {
    if (pageIndex >= this->pageSnapshots.size()) {
        return std::nullopt;
    }

    std::optional<QtGeometryHit> bestHit;
    for (const auto& drawable: this->pageSnapshots[pageIndex].drawables) {
        const auto* geometry = std::get_if<vn::view::render::GeometryRenderModel>(&drawable);
        if (!geometry) {
            continue;
        }

        auto filteredGeometry = *geometry;
        if (!includeVertices) {
            filteredGeometry.vertices.clear();
        }
        if (!includeEdges) {
            filteredGeometry.edges.clear();
        }
        if (!includeFaces) {
            filteredGeometry.faces.clear();
        }
        auto hit = vn::view::render::hitTestGeometry(filteredGeometry, pageX, pageY, zoom, maxScreenDistance);
        if (!hit) {
            continue;
        }

        if (!bestHit || hit->screenDistance < bestHit->hit.screenDistance) {
            bestHit = QtGeometryHit{.pageIndex = pageIndex, .hit = *hit};
        }
    }

    return bestHit;
}

void QtDocumentController::setHoveredGeometry(std::optional<QtGeometryHit> hit) {
    this->hoveredGeometryHit = std::move(hit);
}

void QtDocumentController::setSelectedGeometry(std::optional<QtGeometryHit> hit, bool additive) {
    if (!additive || !hit || !this->selectedGeometryHit ||
        this->selectedGeometryHit->pageIndex != hit->pageIndex ||
        this->selectedGeometryHit->hit.objectId != hit->hit.objectId) {
        this->selectedGeometryHit = std::move(hit);
        this->selectedGeometryVertexIds.clear();
        this->selectedGeometryEdgeIds.clear();
        this->selectedGeometryFaceIds.clear();
        if (this->selectedGeometryHit) {
            if (this->selectedGeometryHit->hit.type == vn::view::render::GeometryHitType::Vertex &&
                this->selectedGeometryHit->hit.vertexId != vn::geom::InvalidVertexId) {
                this->selectedGeometryVertexIds.push_back(this->selectedGeometryHit->hit.vertexId);
            }
            if (this->selectedGeometryHit->hit.type == vn::view::render::GeometryHitType::Edge &&
                this->selectedGeometryHit->hit.edgeId != vn::geom::InvalidEdgeId) {
                this->selectedGeometryEdgeIds.push_back(this->selectedGeometryHit->hit.edgeId);
            }
            if (this->selectedGeometryHit->hit.type == vn::view::render::GeometryHitType::Face &&
                this->selectedGeometryHit->hit.faceId != vn::geom::InvalidFaceId) {
                this->selectedGeometryFaceIds.push_back(this->selectedGeometryHit->hit.faceId);
            }
        }
        return;
    }

    this->selectedGeometryHit = std::move(hit);
    if (this->selectedGeometryHit) {
        if (this->selectedGeometryHit->hit.type == vn::view::render::GeometryHitType::Vertex &&
            this->selectedGeometryHit->hit.vertexId != vn::geom::InvalidVertexId &&
            std::find(this->selectedGeometryVertexIds.begin(), this->selectedGeometryVertexIds.end(),
                      this->selectedGeometryHit->hit.vertexId) == this->selectedGeometryVertexIds.end()) {
            this->selectedGeometryVertexIds.push_back(this->selectedGeometryHit->hit.vertexId);
        }
        if (this->selectedGeometryHit->hit.type == vn::view::render::GeometryHitType::Edge &&
            this->selectedGeometryHit->hit.edgeId != vn::geom::InvalidEdgeId &&
            std::find(this->selectedGeometryEdgeIds.begin(), this->selectedGeometryEdgeIds.end(),
                      this->selectedGeometryHit->hit.edgeId) == this->selectedGeometryEdgeIds.end()) {
            this->selectedGeometryEdgeIds.push_back(this->selectedGeometryHit->hit.edgeId);
        }
        if (this->selectedGeometryHit->hit.type == vn::view::render::GeometryHitType::Face &&
            this->selectedGeometryHit->hit.faceId != vn::geom::InvalidFaceId &&
            std::find(this->selectedGeometryFaceIds.begin(), this->selectedGeometryFaceIds.end(),
                      this->selectedGeometryHit->hit.faceId) == this->selectedGeometryFaceIds.end()) {
            this->selectedGeometryFaceIds.push_back(this->selectedGeometryHit->hit.faceId);
        }
    }
}

void QtDocumentController::setSelectedGeometryObject(std::optional<QtGeometryHit> hit) {
    this->selectedGeometryHit = std::move(hit);
    this->selectedGeometryVertexIds.clear();
    this->selectedGeometryEdgeIds.clear();
    this->selectedGeometryFaceIds.clear();
}

void QtDocumentController::clearInteractiveGeometryState() {
    this->hoveredGeometryHit.reset();
    this->selectedGeometryHit.reset();
    this->selectedGeometryVertexIds.clear();
    this->selectedGeometryEdgeIds.clear();
    this->selectedGeometryFaceIds.clear();
    this->geometryDragState.reset();
    this->geometryDragLinkedObjects.clear();
    this->geometryTransformState.reset();
}

auto QtDocumentController::hoveredGeometry() const -> const std::optional<QtGeometryHit>& {
    return this->hoveredGeometryHit;
}

auto QtDocumentController::selectedGeometry() const -> const std::optional<QtGeometryHit>& {
    return this->selectedGeometryHit;
}

auto QtDocumentController::selectedVertexIds() const -> const std::vector<vn::geom::VertexId>& {
    return this->selectedGeometryVertexIds;
}

auto QtDocumentController::selectedEdgeIds() const -> const std::vector<vn::geom::EdgeId>& {
    return this->selectedGeometryEdgeIds;
}

auto QtDocumentController::selectedFaceIds() const -> const std::vector<vn::geom::FaceId>& {
    return this->selectedGeometryFaceIds;
}

auto QtDocumentController::selectedGeometryDepthRange() const -> std::optional<QtGeometryDepthRange> {
    if (!this->document || !this->selectedGeometryHit ||
        this->selectedGeometryHit->pageIndex >= this->document->getPageCount()) {
        return std::nullopt;
    }

    const auto page = this->document->getPage(this->selectedGeometryHit->pageIndex);
    if (!page) {
        return std::nullopt;
    }

    const vn::geom::GeometryObject* selectedObject = nullptr;
    for (const Layer* layer: page->getLayersView()) {
        if (!layer || !layer->isVisible()) {
            continue;
        }
        for (const Element* element: layer->getElementsView()) {
            const auto* geometry = dynamic_cast<const vn::geom::GeometryElement*>(element);
            if (geometry && geometry->geometry().objectId() == this->selectedGeometryHit->hit.objectId) {
                selectedObject = &geometry->geometry();
                break;
            }
        }
        if (selectedObject) {
            break;
        }
    }

    if (!selectedObject) {
        return std::nullopt;
    }

    const auto vertexIds = selectedGeometryTransformVertexIds(*selectedObject);
    if (vertexIds.empty()) {
        return std::nullopt;
    }

    QtGeometryDepthRange range{.minZ = std::numeric_limits<double>::max(),
                               .maxZ = std::numeric_limits<double>::lowest(),
                               .vertexCount = 0U};
    for (auto vertexId: vertexIds) {
        const auto* vertex = selectedObject->vertex(vertexId);
        if (!vertex) {
            continue;
        }
        range.minZ = std::min(range.minZ, vertex->modelPosition.z);
        range.maxZ = std::max(range.maxZ, vertex->modelPosition.z);
        ++range.vertexCount;
    }

    if (range.vertexCount == 0U) {
        return std::nullopt;
    }
    return range;
}

auto QtDocumentController::selectedGeometryFaceLoopStatus() const -> QtGeometryFaceLoopStatus {
    if (!this->document || !this->selectedGeometryHit ||
        this->selectedGeometryHit->pageIndex >= this->document->getPageCount()) {
        return {.kind = QtGeometryFaceLoopStatusKind::NoSelection,
                .message = "Select geometry edges before Fill"};
    }

    const auto page = this->document->getPage(this->selectedGeometryHit->pageIndex);
    if (!page) {
        return {.kind = QtGeometryFaceLoopStatusKind::NoSelection,
                .message = "Select geometry edges before Fill"};
    }

    const vn::geom::GeometryObject* selectedObject = nullptr;
    for (const Layer* layer: page->getLayersView()) {
        if (!layer || !layer->isVisible()) {
            continue;
        }
        for (const Element* element: layer->getElementsView()) {
            const auto* geometry = dynamic_cast<const vn::geom::GeometryElement*>(element);
            if (geometry && geometry->geometry().objectId() == this->selectedGeometryHit->hit.objectId) {
                selectedObject = &geometry->geometry();
                break;
            }
        }
        if (selectedObject) {
            break;
        }
    }
    if (!selectedObject) {
        return {.kind = QtGeometryFaceLoopStatusKind::NoSelection,
                .message = "Selected geometry no longer exists"};
    }

    const auto edgeIds = fillCandidateEdgeIds(*selectedObject, this->selectedGeometryEdgeIds);
    return evaluateClosedLineLoop(*selectedObject, edgeIds);
}

auto QtDocumentController::selectedGeometryFaceSplitDiagonals() const -> std::vector<QtGeometryFaceDiagonal> {
    std::vector<QtGeometryFaceDiagonal> diagonals;
    if (!this->document || !this->selectedGeometryHit ||
        this->selectedGeometryHit->pageIndex >= this->document->getPageCount()) {
        return diagonals;
    }

    const auto faceId = !this->selectedGeometryFaceIds.empty() ? this->selectedGeometryFaceIds.front()
                                                               : this->selectedGeometryHit->hit.faceId;
    if (faceId == vn::geom::InvalidFaceId) {
        return diagonals;
    }

    const auto page = this->document->getPage(this->selectedGeometryHit->pageIndex);
    if (!page) {
        return diagonals;
    }

    const vn::geom::Face* face = nullptr;
    for (const Layer* layer: page->getLayersView()) {
        if (!layer || !layer->isVisible()) {
            continue;
        }
        for (const Element* element: layer->getElementsView()) {
            const auto* geometry = dynamic_cast<const vn::geom::GeometryElement*>(element);
            if (!geometry || geometry->geometry().objectId() != this->selectedGeometryHit->hit.objectId) {
                continue;
            }
            face = geometry->geometry().face(faceId);
            break;
        }
        if (face) {
            break;
        }
    }
    if (!face || face->vertices.size() < 4U) {
        return diagonals;
    }

    const auto count = face->vertices.size();
    for (std::size_t lhs = 0U; lhs < count; ++lhs) {
        for (std::size_t rhs = lhs + 1U; rhs < count; ++rhs) {
            if (rhs == lhs + 1U || (lhs == 0U && rhs + 1U == count)) {
                continue;
            }
            diagonals.push_back(QtGeometryFaceDiagonal{
                    .lhsIndex = lhs,
                    .rhsIndex = rhs,
                    .lhs = face->vertices[lhs],
                    .rhs = face->vertices[rhs],
            });
        }
    }
    return diagonals;
}

auto QtDocumentController::beginGeometryVertexDrag(const QtGeometryHit& hit) -> bool {
    if (hit.hit.type != vn::view::render::GeometryHitType::Vertex) {
        return false;
    }

    this->lastGeometryDragStatus.clear();
    this->geometryDragLinkedObjects.clear();
    const bool preserveSelection = this->selectedGeometryHit && this->selectedGeometryHit->pageIndex == hit.pageIndex &&
                                   this->selectedGeometryHit->hit.objectId == hit.hit.objectId &&
                                   std::find(this->selectedGeometryVertexIds.begin(), this->selectedGeometryVertexIds.end(),
                                             hit.hit.vertexId) != this->selectedGeometryVertexIds.end();
    if (!preserveSelection) {
        setSelectedGeometry(hit);
    }
    this->geometryDragState = QtGeometryDragState{
            .pageIndex = hit.pageIndex,
            .objectId = hit.hit.objectId,
            .vertexId = hit.hit.vertexId,
            .vertexIds = this->selectedGeometryVertexIds.empty() ? std::vector<vn::geom::VertexId>{hit.hit.vertexId}
                                                                 : this->selectedGeometryVertexIds,
            .originalPosition = {hit.hit.point.x, hit.hit.point.y},
            .currentPosition = {hit.hit.point.x, hit.hit.point.y},
            .beforeGeometry = vn::geom::GeometryObject{},
            .snapPoint = {hit.hit.point.x, hit.hit.point.y},
    };

    bool initialized = false;
    this->document->lock();
    if (auto* geometry = findMutableGeometryElement(hit.pageIndex, hit.hit.objectId)) {
        this->geometryDragState->beforeGeometry = geometry->geometry();
        auto& beforeGeometry = this->geometryDragState->beforeGeometry;

        std::vector<vn::geom::Vec2> draggedPositions;
        draggedPositions.reserve(this->geometryDragState->vertexIds.size());
        for (auto vertexId: this->geometryDragState->vertexIds) {
            if (const auto* vertex = beforeGeometry.vertex(vertexId)) {
                if (!containsCoincidentPoint(draggedPositions, vertex->position)) {
                    draggedPositions.push_back(vertex->position);
                }
            }
        }
        if (draggedPositions.empty()) {
            draggedPositions.push_back({hit.hit.point.x, hit.hit.point.y});
        }

        for (const auto& vertex: beforeGeometry.vertices()) {
            if (!containsVertexId(this->geometryDragState->vertexIds, vertex.id) &&
                containsCoincidentPoint(draggedPositions, vertex.position)) {
                this->geometryDragState->vertexIds.push_back(vertex.id);
            }
        }

        this->geometryDragState->originalPositions.reserve(this->geometryDragState->vertexIds.size());
        this->geometryDragState->currentPositions.reserve(this->geometryDragState->vertexIds.size());
        for (auto vertexId: this->geometryDragState->vertexIds) {
            if (const auto* vertex = beforeGeometry.vertex(vertexId)) {
                this->geometryDragState->originalPositions.push_back(vertex->position);
                this->geometryDragState->currentPositions.push_back(vertex->position);
            }
        }
        initialized = !this->geometryDragState->originalPositions.empty();

        auto page = this->document->getPage(hit.pageIndex);
        if (page) {
            for (Layer* layer: page->getLayers()) {
                if (!layer || !layer->isVisible()) {
                    continue;
                }
                for (auto& element: layer->getElements()) {
                    auto* linkedGeometry = dynamic_cast<vn::geom::GeometryElement*>(element.get());
                    if (!linkedGeometry || linkedGeometry == geometry ||
                        linkedGeometry->geometry().objectId() == hit.hit.objectId) {
                        continue;
                    }

                    std::vector<vn::geom::VertexId> linkedVertexIds;
                    const auto& linkedObject = linkedGeometry->geometry();
                    for (const auto& vertex: linkedObject.vertices()) {
                        if (containsCoincidentPoint(draggedPositions, vertex.position)) {
                            linkedVertexIds.push_back(vertex.id);
                        }
                    }
                    if (linkedVertexIds.empty()) {
                        continue;
                    }

                    this->geometryDragLinkedObjects.push_back(QtGeometryDragLinkedObjectState{
                            .pageIndex = hit.pageIndex,
                            .objectId = linkedObject.objectId(),
                            .vertexIds = std::move(linkedVertexIds),
                            .beforeGeometry = linkedObject,
                    });
                }
            }
        }
    }
    this->document->unlock();
    if (!initialized) {
        this->geometryDragState.reset();
        this->geometryDragLinkedObjects.clear();
    }
    return initialized;
}

auto QtDocumentController::snapPagePoint(std::size_t pageIndex, double pageX, double pageY, double zoom,
                                         const QtSnapOptions& options,
                                         std::optional<vn::geom::ObjectId> ignoredObjectId) const
        -> QtSnapPointResult {
    vn::geom::Vec2 target{pageX, pageY};
    QtSnapPointResult result{.pagePoint = target, .snapKind = std::nullopt, .snapped = false};
    if (!this->document) {
        return result;
    }

    this->document->lock();
    auto page = this->document->getPage(pageIndex);
    if (!page) {
        this->document->unlock();
        return result;
    }

    vn::snap::SnapEngine engine;
    bool hasSnapProviders = false;
    if (options.geometryEnabled) {
        if (ignoredObjectId) {
            auto objects = vn::snap::collectGeometryObjects(page);
            objects.erase(std::remove_if(objects.begin(), objects.end(),
                                         [&](const vn::geom::GeometryObject* object) {
                                             return !object || object->objectId() == *ignoredObjectId;
                                         }),
                          objects.end());
            if (!objects.empty()) {
                engine.addProvider(std::make_shared<vn::snap::GeometrySnapProvider>(std::move(objects)));
                hasSnapProviders = true;
            }
        } else if (auto provider = geometrySnapProviderForPage(pageIndex, page)) {
            engine.addProvider(std::move(provider));
            hasSnapProviders = true;
        }
    }

    if (options.gridEnabled) {
        if (auto provider = gridSnapProviderFor(page->getBackgroundType().format, options.gridSize, options.gridTolerance)) {
            engine.addProvider(std::move(provider));
            hasSnapProviders = true;
        }
    }
    this->document->unlock();

    if (!hasSnapProviders) {
        return result;
    }

    const auto snapResult = engine.snap(vn::snap::SnapQuery{target, zoom, options.screenTolerance});
    if (!snapResult.snapped()) {
        return result;
    }

    result.pagePoint = snapResult.pagePoint;
    result.snapKind = snapResult.candidate ? std::optional<vn::snap::SnapKind>(snapResult.candidate->kind) : std::nullopt;
    result.snapCandidate = snapResult.candidate;
    result.snapped = true;
    return result;
}

auto QtDocumentController::updateGeometryVertexDrag(double pageX, double pageY, double zoom,
                                                               const QtSnapOptions& options) -> bool {
    if (!this->geometryDragState || !this->document) {
        return false;
    }

    auto snapped = snapPagePoint(this->geometryDragState->pageIndex, pageX, pageY, zoom, options,
                                 this->geometryDragState->objectId);
    const auto snappedToLinkedDragObject = [this, &snapped]() {
        if (!snapped.snapCandidate || snapped.snapCandidate->object == vn::geom::InvalidObjectId) {
            return false;
        }
        return std::ranges::any_of(this->geometryDragLinkedObjects, [&snapped](const auto& linked) {
            return linked.objectId == snapped.snapCandidate->object;
        });
    };
    if (snappedToLinkedDragObject()) {
        if (options.gridEnabled) {
            snapped = snapPagePoint(this->geometryDragState->pageIndex, pageX, pageY, zoom,
                                    {.geometryEnabled = false,
                                     .gridEnabled = true,
                                     .gridSize = options.gridSize,
                                     .gridTolerance = options.gridTolerance,
                                     .screenTolerance = options.screenTolerance},
                                    this->geometryDragState->objectId);
        } else {
            snapped = QtSnapPointResult{.pagePoint = vn::geom::Vec2{pageX, pageY},
                                        .snapKind = std::nullopt,
                                        .snapCandidate = std::nullopt,
                                        .snapped = false};
        }
    }
    vn::geom::Vec2 target = snapped.pagePoint;
    this->geometryDragState->snapKind = snapped.snapKind;
    this->geometryDragState->snapCandidate = snapped.snapCandidate;
    this->geometryDragState->snapPoint = snapped.pagePoint;

    this->document->lock();
    auto* geometry = findMutableGeometryElement(this->geometryDragState->pageIndex, this->geometryDragState->objectId);
    auto page = this->document->getPage(this->geometryDragState->pageIndex);
    if (!geometry || !page) {
        this->document->unlock();
        return false;
    }

    if (options.geometryEnabled) {
        std::optional<vn::snap::SnapCandidate> selfSnap;
        if (auto vertexSnap = snapToEditableSelfVertex(geometry->geometry(), this->geometryDragState->vertexIds, target,
                                                       zoom, options.screenTolerance)) {
            selfSnap = vertexSnap;
        }
        if (auto edgeSnap = snapToEditableSelfEdge(geometry->geometry(), this->geometryDragState->vertexIds, target,
                                                   zoom, options.screenTolerance);
            edgeSnap && (!selfSnap || edgeSnap->screenDistance < selfSnap->screenDistance)) {
            selfSnap = edgeSnap;
        }
        if (selfSnap && (!snapped.snapped || selfSnap->screenDistance < snapped.snapCandidate->screenDistance)) {
            target = selfSnap->pagePoint;
            this->geometryDragState->snapKind = selfSnap->kind;
            this->geometryDragState->snapCandidate = selfSnap;
            this->geometryDragState->snapPoint = selfSnap->pagePoint;
        }
    }

    const vn::geom::Vec2 delta{target.x - this->geometryDragState->originalPosition.x,
                               target.y - this->geometryDragState->originalPosition.y};
    bool changed = std::abs(delta.x) > 1e-6 || std::abs(delta.y) > 1e-6;
    const vn::geom::GeometryTransform2D transform{
            .pivot = this->geometryDragState->originalPosition,
            .translation = delta,
    };
    auto transformed = vn::geom::transformedGeometry(
            this->geometryDragState->beforeGeometry, this->geometryDragState->vertexIds, transform);
    if (this->geometryDragState->vertexIds.size() <= 1U) {
        changed = transformed.setVertexPosition(this->geometryDragState->vertexId, target) || changed;
    }
    geometry->replaceGeometry(std::move(transformed));
    if (!geometry->geometry().constraints().empty()) {
        const vn::constraints::GeometryConstraintSolver solver;
        changed = solver.apply(geometry->geometry()).changed || changed;
    }

    for (auto& linked: this->geometryDragLinkedObjects) {
        auto* linkedGeometry = findMutableGeometryElement(linked.pageIndex, linked.objectId);
        if (!linkedGeometry) {
            continue;
        }
        auto linkedTransformed = vn::geom::transformedGeometry(linked.beforeGeometry, linked.vertexIds, transform);
        linkedGeometry->replaceGeometry(std::move(linkedTransformed));
        if (!linkedGeometry->geometry().constraints().empty()) {
            const vn::constraints::GeometryConstraintSolver solver;
            changed = solver.apply(linkedGeometry->geometry()).changed || changed;
        }
    }

    if (const auto* vertex = geometry->geometry().vertex(this->geometryDragState->vertexId)) {
        this->geometryDragState->currentPosition = vertex->position;
        this->geometryDragState->changed = this->geometryDragState->changed ||
                                           vertex->position.x != this->geometryDragState->originalPosition.x ||
                                           vertex->position.y != this->geometryDragState->originalPosition.y;

        if (this->selectedGeometryHit) {
            this->selectedGeometryHit->hit.point = Point(vertex->position.x, vertex->position.y);
        }
        if (this->hoveredGeometryHit && this->hoveredGeometryHit->hit.objectId == this->geometryDragState->objectId &&
            this->hoveredGeometryHit->hit.vertexId == this->geometryDragState->vertexId) {
            this->hoveredGeometryHit->hit.point = Point(vertex->position.x, vertex->position.y);
        }
    }
    this->geometryDragState->currentPositions.clear();
    for (auto vertexId: this->geometryDragState->vertexIds) {
        if (const auto* vertex = geometry->geometry().vertex(vertexId)) {
            this->geometryDragState->currentPositions.push_back(vertex->position);
        }
    }
    this->document->unlock();

    if (changed) {
        rebuildPageSnapshots();
    }
    return changed;
}

auto QtDocumentController::endGeometryVertexDrag() -> bool {
    bool changed = this->geometryDragState && this->geometryDragState->changed;
    this->lastGeometryDragStatus.clear();
    if (this->document && this->geometryDragState) {
        this->document->lock();
        if (auto* geometry =
                    findMutableGeometryElement(this->geometryDragState->pageIndex, this->geometryDragState->objectId)) {
            std::vector<QtGeometryHistoryObjectState> linkedObjects;
            std::vector<QtGeometryHistoryRemovedElement> removedElements;
            const auto snapKind = this->geometryDragState->snapKind;
            const auto snapCandidate = this->geometryDragState->snapCandidate;
            bool welded = false;
            bool splitEdge = false;
            bool failedCrossObjectEdgeWeld = false;
            if (snapCandidate && snapCandidate->vertex != vn::geom::InvalidVertexId &&
                snapCandidate->object != vn::geom::InvalidObjectId &&
                snapKind == vn::snap::SnapKind::ExplicitVertex) {
                if (auto* targetGeometry =
                            findMutableGeometryElement(this->geometryDragState->pageIndex, snapCandidate->object)) {
                    if (snapCandidate->object == this->geometryDragState->objectId) {
                        if (geometry->geometry().mergeVertexInto(this->geometryDragState->vertexId,
                                                                 snapCandidate->vertex)) {
                            changed = true;
                            welded = true;
                        }
                    } else if (auto merged =
                                       mergedGeometryForVertexWeld(geometry->geometry(), targetGeometry->geometry(),
                                                                   snapCandidate->vertex,
                                                                   this->geometryDragState->vertexId);
                               merged) {
                        if (auto removedTarget =
                                    removeGeometryElement(this->geometryDragState->pageIndex, snapCandidate->object)) {
                            geometry->replaceGeometry(std::move(*merged));
                            changed = true;
                            welded = true;
                            removedElements.push_back(std::move(*removedTarget));
                        }
                    }
                }
            } else if (snapCandidate && snapCandidate->edge != vn::geom::InvalidEdgeId &&
                       snapCandidate->object != vn::geom::InvalidObjectId &&
                       (snapKind == vn::snap::SnapKind::EdgeProjection ||
                        snapKind == vn::snap::SnapKind::Midpoint)) {
                if (auto* targetGeometry =
                            findMutableGeometryElement(this->geometryDragState->pageIndex, snapCandidate->object)) {
                    if (snapCandidate->object == this->geometryDragState->objectId) {
                        if (targetGeometry->geometry().splitEdgeAtVertex(snapCandidate->edge,
                                                                         this->geometryDragState->vertexId)) {
                            changed = true;
                            splitEdge = true;
                        }
                    } else {
                        const auto beforeTarget = targetGeometry->geometry();
                        if (auto merged = mergedGeometryForEdgeWeld(geometry->geometry(), beforeTarget,
                                                                    snapCandidate->edge,
                                                                    this->geometryDragState->vertexId);
                            merged) {
                            if (auto removedTarget = removeGeometryElement(this->geometryDragState->pageIndex,
                                                                           snapCandidate->object)) {
                                geometry->replaceGeometry(std::move(*merged));
                                changed = true;
                                welded = true;
                                removedElements.push_back(std::move(*removedTarget));
                            }
                        } else {
                            failedCrossObjectEdgeWeld = true;
                        }
                    }
                }
            }

            if (!changed) {
                this->document->unlock();
                this->geometryDragState.reset();
                this->geometryDragLinkedObjects.clear();
                return false;
            }

            for (const auto& linked: this->geometryDragLinkedObjects) {
                if (auto* linkedGeometry = findMutableGeometryElement(linked.pageIndex, linked.objectId)) {
                    linkedObjects.push_back(QtGeometryHistoryObjectState{
                            .pageIndex = linked.pageIndex,
                            .objectId = linked.objectId,
                            .before = linked.beforeGeometry,
                            .after = linkedGeometry->geometry(),
                    });
                }
            }

            pushGeometryHistory({.pageIndex = this->geometryDragState->pageIndex,
                                 .objectId = this->geometryDragState->objectId,
                                 .before = this->geometryDragState->beforeGeometry,
                                 .after = geometry->geometry(),
                                 .linkedObjects = std::move(linkedObjects),
                                 .removedElements = std::move(removedElements),
                                 .text = this->geometryDragState->vertexIds.size() > 1U ? "Move geometry vertices"
                                                                                        : "Move geometry vertex"});
            if (welded) {
                this->lastGeometryDragStatus = "Geometry welded";
            } else if (splitEdge) {
                this->lastGeometryDragStatus = "Geometry edge split";
            } else if (failedCrossObjectEdgeWeld) {
                this->lastGeometryDragStatus = "Edge weld failed; moved vertex only";
            } else {
                this->lastGeometryDragStatus =
                        this->geometryDragState->vertexIds.size() > 1U ? "Moved geometry vertices"
                                                                       : "Moved geometry vertex";
            }
        }
        this->document->unlock();
    }
    this->geometryDragState.reset();
    this->geometryDragLinkedObjects.clear();
    if (changed) {
        rebuildPageSnapshots();
    }
    return changed;
}

auto QtDocumentController::activeGeometryDrag() const -> const std::optional<QtGeometryDragState>& {
    return this->geometryDragState;
}

auto QtDocumentController::lastGeometryDragMessage() const -> const std::string& {
    return this->lastGeometryDragStatus;
}

auto QtDocumentController::selectedGeometryTransformVertexIds(const vn::geom::GeometryObject& object) const
        -> std::vector<vn::geom::VertexId> {
    std::vector<vn::geom::VertexId> vertexIds;
    if (!this->selectedGeometryVertexIds.empty()) {
        vertexIds.reserve(this->selectedGeometryVertexIds.size());
        for (auto vertexId: this->selectedGeometryVertexIds) {
            if (object.vertex(vertexId) &&
                std::find(vertexIds.begin(), vertexIds.end(), vertexId) == vertexIds.end()) {
                vertexIds.push_back(vertexId);
            }
        }
        return vertexIds;
    }

    if (!this->selectedGeometryEdgeIds.empty()) {
        vertexIds.reserve(this->selectedGeometryEdgeIds.size() * 2U);
        const auto appendVertex = [&](vn::geom::VertexId vertexId) {
            if (object.vertex(vertexId) && std::find(vertexIds.begin(), vertexIds.end(), vertexId) == vertexIds.end()) {
                vertexIds.push_back(vertexId);
            }
        };
        for (auto edgeId: this->selectedGeometryEdgeIds) {
            const auto* edge = object.edge(edgeId);
            if (!edge) {
                continue;
            }
            appendVertex(edge->start);
            appendVertex(edge->end);
            for (auto controlId: edge->controls) {
                appendVertex(controlId);
            }
        }
        return vertexIds;
    }

    if (!this->selectedGeometryFaceIds.empty()) {
        const auto appendVertex = [&](vn::geom::VertexId vertexId) {
            if (object.vertex(vertexId) && std::find(vertexIds.begin(), vertexIds.end(), vertexId) == vertexIds.end()) {
                vertexIds.push_back(vertexId);
            }
        };
        for (auto faceId: this->selectedGeometryFaceIds) {
            const auto* face = object.face(faceId);
            if (!face) {
                continue;
            }
            for (auto vertexId: face->vertices) {
                appendVertex(vertexId);
            }
        }
        return vertexIds;
    }

    vertexIds.reserve(object.vertices().size());
    for (const auto& vertex: object.vertices()) {
        vertexIds.push_back(vertex.id);
    }
    return vertexIds;
}

auto QtDocumentController::beginSelectedGeometryTransform() -> bool {
    if (!this->selectedGeometryHit || !this->document) {
        return false;
    }

    this->document->lock();
    auto* geometry =
            findMutableGeometryElement(this->selectedGeometryHit->pageIndex, this->selectedGeometryHit->hit.objectId);
    if (!geometry) {
        this->document->unlock();
        return false;
    }

    auto vertexIds = selectedGeometryTransformVertexIds(geometry->geometry());
    if (vertexIds.empty()) {
        this->document->unlock();
        return false;
    }

    vn::geom::Vec2 center{};
    std::size_t count = 0U;
    for (auto vertexId: vertexIds) {
        if (const auto* vertex = geometry->geometry().vertex(vertexId)) {
            center.x += vertex->position.x;
            center.y += vertex->position.y;
            ++count;
        }
    }
    if (count == 0U) {
        this->document->unlock();
        return false;
    }
    center.x /= static_cast<double>(count);
    center.y /= static_cast<double>(count);

    const bool transformedWholeObject = vertexIds.size() == geometry->geometry().vertices().size();
    const QtGeometryTransformSelectionKind selectionKind =
            this->selectedGeometryEdgeIds.empty()
                    ? (transformedWholeObject ? QtGeometryTransformSelectionKind::Object
                                              : QtGeometryTransformSelectionKind::Vertices)
                    : QtGeometryTransformSelectionKind::Edges;
    this->geometryTransformState = QtGeometryTransformState{
            .pageIndex = this->selectedGeometryHit->pageIndex,
            .objectId = this->selectedGeometryHit->hit.objectId,
            .vertexIds = std::move(vertexIds),
            .beforeGeometry = geometry->geometry(),
            .currentGeometry = geometry->geometry(),
            .center = center,
            .transformedWholeObject = transformedWholeObject,
            .selectionKind = selectionKind,
    };
    this->document->unlock();
    return true;
}

auto QtDocumentController::updateSelectedGeometryTransform(double dx, double dy, double degrees, double scaleX,
                                                           double scaleY) -> bool {
    if (!this->geometryTransformState || !this->document) {
        return false;
    }

    auto& state = *this->geometryTransformState;
    const bool changed = std::abs(dx) > 1e-6 || std::abs(dy) > 1e-6 || std::abs(degrees) > 1e-6 ||
                         std::abs(scaleX - 1.0) > 1e-6 || std::abs(scaleY - 1.0) > 1e-6;

    this->document->lock();
    auto* geometry = findMutableGeometryElement(state.pageIndex, state.objectId);
    if (!geometry) {
        this->document->unlock();
        this->geometryTransformState.reset();
        return false;
    }

    const double radians = degrees * PI / 180.0;
    auto transformed = vn::geom::transformedGeometry(
            state.beforeGeometry, state.vertexIds,
            vn::geom::GeometryTransform2D{
                    .pivot = state.center,
                    .translation = vn::geom::Vec2{.x = dx, .y = dy},
                    .rotationRadians = radians,
                    .scaleX = scaleX,
                    .scaleY = scaleY,
            });

    geometry->replaceGeometry(transformed);
    state.currentGeometry = geometry->geometry();
    state.currentDx = dx;
    state.currentDy = dy;
    state.currentDegrees = degrees;
    state.currentScaleX = scaleX;
    state.currentScaleY = scaleY;
    state.changed = changed;

    if (this->selectedGeometryHit) {
        if (this->selectedGeometryHit->hit.vertexId != vn::geom::InvalidVertexId) {
            if (const auto* vertex = geometry->geometry().vertex(this->selectedGeometryHit->hit.vertexId)) {
                this->selectedGeometryHit->hit.point = Point(vertex->position.x, vertex->position.y);
            }
        } else {
            this->selectedGeometryHit->hit.point = Point(state.center.x + dx, state.center.y + dy);
        }
    }
    this->document->unlock();

    rebuildPageSnapshots();
    return changed;
}

auto QtDocumentController::endSelectedGeometryTransform() -> bool {
    if (!this->geometryTransformState) {
        return false;
    }

    auto state = std::move(*this->geometryTransformState);
    this->geometryTransformState.reset();
    if (!state.changed) {
        return false;
    }

    const bool rotated = std::abs(state.currentDegrees) > 1e-6;
    const bool translated = std::abs(state.currentDx) > 1e-6 || std::abs(state.currentDy) > 1e-6;
    const bool scaled = std::abs(state.currentScaleX - 1.0) > 1e-6 || std::abs(state.currentScaleY - 1.0) > 1e-6;
    std::string text = "Transform geometry";
    const auto selectionLabel = [&state](std::string_view objectText, std::string_view edgeText,
                                         std::string_view edgesText, std::string_view vertexText,
                                         std::string_view verticesText) -> std::string {
        switch (state.selectionKind) {
            case QtGeometryTransformSelectionKind::Object:
                return std::string(objectText);
            case QtGeometryTransformSelectionKind::Edges:
                return std::string(state.vertexIds.size() > 2U ? edgesText : edgeText);
            case QtGeometryTransformSelectionKind::Vertices:
                return std::string(state.vertexIds.size() > 1U ? verticesText : vertexText);
        }
        return std::string(objectText);
    };
    if (scaled && !rotated && !translated) {
        text = selectionLabel("Scale geometry object", "Scale geometry edge", "Scale geometry edges",
                              "Scale geometry vertex", "Scale geometry vertices");
    } else if (rotated && !translated && !scaled) {
        text = selectionLabel("Rotate geometry object", "Rotate geometry edge", "Rotate geometry edges",
                              "Rotate geometry vertex", "Rotate geometry vertices");
    } else if (!rotated && translated && !scaled) {
        text = selectionLabel("Move geometry object", "Move geometry edge", "Move geometry edges",
                              "Move geometry vertex", "Move geometry vertices");
    }

    pushGeometryHistory({.pageIndex = state.pageIndex,
                         .objectId = state.objectId,
                         .before = std::move(state.beforeGeometry),
                         .after = std::move(state.currentGeometry),
                         .text = std::move(text)});
    return true;
}

void QtDocumentController::cancelSelectedGeometryTransform() {
    if (!this->geometryTransformState || !this->document) {
        this->geometryTransformState.reset();
        return;
    }

    auto state = std::move(*this->geometryTransformState);
    this->geometryTransformState.reset();
    this->document->lock();
    if (auto* geometry = findMutableGeometryElement(state.pageIndex, state.objectId)) {
        geometry->replaceGeometry(std::move(state.beforeGeometry));
    }
    this->document->unlock();
    rebuildPageSnapshots();
}

auto QtDocumentController::activeGeometryTransform() const -> const std::optional<QtGeometryTransformState>& {
    return this->geometryTransformState;
}

auto QtDocumentController::translateSelectedVertices(double dx, double dy) -> bool {
    if (!this->selectedGeometryHit || !this->document || (dx == 0.0 && dy == 0.0)) {
        return false;
    }
    if (!beginSelectedGeometryTransform()) {
        return false;
    }
    if (!updateSelectedGeometryTransform(dx, dy, 0.0)) {
        cancelSelectedGeometryTransform();
        return false;
    }
    return endSelectedGeometryTransform();
}

auto QtDocumentController::rotateSelectedGeometry(double degrees) -> bool {
    if (!this->selectedGeometryHit || !this->document || degrees == 0.0) {
        return false;
    }
    if (!beginSelectedGeometryTransform()) {
        return false;
    }
    if (!updateSelectedGeometryTransform(0.0, 0.0, degrees)) {
        cancelSelectedGeometryTransform();
        return false;
    }
    return endSelectedGeometryTransform();
}

auto QtDocumentController::scaleSelectedGeometry(double scaleX, double scaleY) -> bool {
    if (!this->selectedGeometryHit || !this->document || (scaleX == 1.0 && scaleY == 1.0) || scaleX <= 0.0 ||
        scaleY <= 0.0) {
        return false;
    }
    if (!beginSelectedGeometryTransform()) {
        return false;
    }
    if (!updateSelectedGeometryTransform(0.0, 0.0, 0.0, scaleX, scaleY)) {
        cancelSelectedGeometryTransform();
        return false;
    }
    return endSelectedGeometryTransform();
}

auto QtDocumentController::projectSelectedGeometry3D(const vn::geom::ProjectionCamera& camera) -> bool {
    if (!this->selectedGeometryHit || !this->document) {
        return false;
    }

    const auto pageIndex = this->selectedGeometryHit->pageIndex;
    const auto objectId = this->selectedGeometryHit->hit.objectId;
    std::optional<vn::geom::GeometryObject> beforeGeometry;
    std::optional<vn::geom::GeometryObject> afterGeometry;
    bool changed = false;

    this->document->lock();
    auto* geometry = findMutableGeometryElement(pageIndex, objectId);
    if (!geometry) {
        this->document->unlock();
        return false;
    }

    beforeGeometry = geometry->geometry();
    auto after = *beforeGeometry;
    const auto vertexIds = allGeometryVertexIds(after);
    normalizePageSpaceModelForProjection(after, vertexIds, camera);
    changed = after.applyProjection(camera);
    if (changed) {
        geometry->replaceGeometry(std::move(after));
        afterGeometry = geometry->geometry();
    }
    this->document->unlock();

    if (!changed || !afterGeometry) {
        return false;
    }

    pushGeometryHistory({.pageIndex = pageIndex,
                         .objectId = objectId,
                         .before = std::move(*beforeGeometry),
                         .after = std::move(*afterGeometry),
                         .text = "Project geometry 3D view"});
    rebuildPageSnapshots();
    return true;
}

auto QtDocumentController::nudgeSelectedGeometryZ(double delta, const vn::geom::ProjectionCamera& camera) -> bool {
    if (!this->selectedGeometryHit || !this->document || delta == 0.0) {
        return false;
    }

    const auto pageIndex = this->selectedGeometryHit->pageIndex;
    const auto objectId = this->selectedGeometryHit->hit.objectId;
    std::optional<vn::geom::GeometryObject> beforeGeometry;
    std::optional<vn::geom::GeometryObject> afterGeometry;
    bool changed = false;

    this->document->lock();
    auto* geometry = findMutableGeometryElement(pageIndex, objectId);
    if (!geometry) {
        this->document->unlock();
        return false;
    }

    beforeGeometry = geometry->geometry();
    auto after = *beforeGeometry;
    auto vertexIds = selectedGeometryTransformVertexIds(after);
    normalizePageSpaceModelForProjection(after, allGeometryVertexIds(after), camera);
    for (auto vertexId: vertexIds) {
        if (auto* vertex = after.vertex(vertexId)) {
            changed = after.setVertexZ(vertexId, vertex->modelPosition.z + delta) || changed;
        }
    }
    changed = after.applyProjection(camera) || changed;
    if (changed) {
        geometry->replaceGeometry(std::move(after));
        afterGeometry = geometry->geometry();
    }
    this->document->unlock();

    if (!changed || !afterGeometry) {
        return false;
    }

    pushGeometryHistory({.pageIndex = pageIndex,
                         .objectId = objectId,
                         .before = std::move(*beforeGeometry),
                         .after = std::move(*afterGeometry),
                         .text = delta > 0.0 ? "Push geometry vertices in Z" : "Pull geometry vertices in Z"});
    rebuildPageSnapshots();
    return true;
}

auto QtDocumentController::setSelectedGeometryZ(double z, const vn::geom::ProjectionCamera& camera) -> bool {
    if (!this->selectedGeometryHit || !this->document) {
        return false;
    }

    const auto pageIndex = this->selectedGeometryHit->pageIndex;
    const auto objectId = this->selectedGeometryHit->hit.objectId;
    std::optional<vn::geom::GeometryObject> beforeGeometry;
    std::optional<vn::geom::GeometryObject> afterGeometry;
    bool changed = false;

    this->document->lock();
    auto* geometry = findMutableGeometryElement(pageIndex, objectId);
    if (!geometry) {
        this->document->unlock();
        return false;
    }

    beforeGeometry = geometry->geometry();
    auto after = *beforeGeometry;
    auto vertexIds = selectedGeometryTransformVertexIds(after);
    normalizePageSpaceModelForProjection(after, allGeometryVertexIds(after), camera);
    for (auto vertexId: vertexIds) {
        changed = after.setVertexZ(vertexId, z) || changed;
    }
    changed = after.applyProjection(camera) || changed;
    if (changed) {
        geometry->replaceGeometry(std::move(after));
        afterGeometry = geometry->geometry();
    }
    this->document->unlock();

    if (!changed || !afterGeometry) {
        return false;
    }

    pushGeometryHistory({.pageIndex = pageIndex,
                         .objectId = objectId,
                         .before = std::move(*beforeGeometry),
                         .after = std::move(*afterGeometry),
                         .text = "Set geometry Z depth"});
    rebuildPageSnapshots();
    return true;
}

auto QtDocumentController::deleteSelectedGeometry() -> bool {
    if (!this->selectedGeometryHit || !this->document) {
        return false;
    }

    bool changed = false;
    bool removesPrimaryObject = false;
    std::optional<vn::geom::GeometryObject> beforeGeometry;
    std::optional<vn::geom::GeometryObject> afterGeometry;
    std::vector<QtGeometryHistoryRemovedElement> removedElements;
    std::string actionText = "Edit geometry topology";
    this->document->lock();
    auto* geometry = findMutableGeometryElement(this->selectedGeometryHit->pageIndex, this->selectedGeometryHit->hit.objectId);
    if (!geometry) {
        this->document->unlock();
        return false;
    }
    beforeGeometry = geometry->geometry();

    if (this->selectedGeometryHit->hit.type == vn::view::render::GeometryHitType::Vertex &&
        this->selectedGeometryHit->hit.vertexId != vn::geom::InvalidVertexId) {
        std::vector<vn::geom::VertexId> vertexIds = this->selectedGeometryVertexIds;
        if (vertexIds.empty()) {
            vertexIds.push_back(this->selectedGeometryHit->hit.vertexId);
        }
        for (auto vertexId: vertexIds) {
            changed = geometry->removeVertex(vertexId) || changed;
        }
        actionText = vertexIds.size() > 1U ? "Delete geometry vertices" : "Delete geometry vertex";
    } else if (this->selectedGeometryHit->hit.type == vn::view::render::GeometryHitType::Edge &&
               this->selectedGeometryHit->hit.edgeId != vn::geom::InvalidEdgeId) {
        std::vector<vn::geom::EdgeId> edgeIds = this->selectedGeometryEdgeIds;
        if (edgeIds.empty()) {
            edgeIds.push_back(this->selectedGeometryHit->hit.edgeId);
        }
        for (auto edgeId: edgeIds) {
            changed = geometry->removeEdge(edgeId) || changed;
        }
        actionText = edgeIds.size() > 1U ? "Delete geometry edges" : "Delete geometry edge";
    } else if (this->selectedGeometryHit->hit.type == vn::view::render::GeometryHitType::Face &&
               this->selectedGeometryHit->hit.faceId != vn::geom::InvalidFaceId) {
        std::vector<vn::geom::FaceId> faceIds = this->selectedGeometryFaceIds;
        if (faceIds.empty()) {
            faceIds.push_back(this->selectedGeometryHit->hit.faceId);
        }
        for (auto faceId: faceIds) {
            changed = geometry->geometry().removeFace(faceId) || changed;
        }
        actionText = faceIds.size() > 1U ? "Delete geometry faces" : "Delete geometry face";
    } else if (this->selectedGeometryVertexIds.empty() && this->selectedGeometryEdgeIds.empty() &&
               this->selectedGeometryFaceIds.empty()) {
        if (auto removed = removeGeometryElement(this->selectedGeometryHit->pageIndex, this->selectedGeometryHit->hit.objectId)) {
            removedElements.push_back(std::move(*removed));
            changed = true;
            removesPrimaryObject = true;
            actionText = "Delete geometry object";
        }
    }
    if (changed && !removesPrimaryObject) {
        afterGeometry = geometry->geometry();
    }
    this->document->unlock();

    if (changed) {
        pushGeometryHistory({.pageIndex = this->selectedGeometryHit->pageIndex,
                             .objectId = this->selectedGeometryHit->hit.objectId,
                             .before = std::move(*beforeGeometry),
                             .after = afterGeometry ? std::move(*afterGeometry) : *beforeGeometry,
                             .removedElements = std::move(removedElements),
                             .removesPrimaryObject = removesPrimaryObject,
                             .text = std::move(actionText)});
        clearInteractiveGeometryState();
        rebuildPageSnapshots();
    }

    return changed;
}

auto QtDocumentController::fillSelectedGeometryFace(int fillOpacity) -> bool {
    if (!this->selectedGeometryHit || !this->document) {
        return false;
    }

    const auto pageIndex = this->selectedGeometryHit->pageIndex;
    const auto objectId = this->selectedGeometryHit->hit.objectId;
    std::optional<vn::geom::GeometryObject> beforeGeometry;
    std::optional<vn::geom::GeometryObject> afterGeometry;
    bool changed = false;

    this->document->lock();
    auto* geometry = findMutableGeometryElement(pageIndex, objectId);
    if (!geometry) {
        this->document->unlock();
        return false;
    }

    beforeGeometry = geometry->geometry();
    auto after = *beforeGeometry;
    const auto edgeIds = fillCandidateEdgeIds(after, this->selectedGeometryEdgeIds);

    auto loopStatus = evaluateClosedLineLoop(after, edgeIds);
    if (loopStatus.kind == QtGeometryFaceLoopStatusKind::Ready) {
        try {
            after.addFace(loopStatus.loop, std::clamp(fillOpacity, 0, 255));
            changed = true;
        } catch (const std::invalid_argument&) {
            changed = false;
        }
    }

    if (changed) {
        geometry->replaceGeometry(std::move(after));
        afterGeometry = geometry->geometry();
    }
    this->document->unlock();

    if (!changed || !afterGeometry) {
        return false;
    }

    pushGeometryHistory({.pageIndex = pageIndex,
                         .objectId = objectId,
                         .before = std::move(*beforeGeometry),
                         .after = std::move(*afterGeometry),
                         .text = "Fill geometry face"});
    rebuildPageSnapshots();
    return true;
}

auto QtDocumentController::deleteSelectedGeometryFace() -> bool {
    if (!this->selectedGeometryHit || !this->document) {
        return false;
    }

    const auto pageIndex = this->selectedGeometryHit->pageIndex;
    const auto objectId = this->selectedGeometryHit->hit.objectId;
    std::vector<vn::geom::FaceId> faceIds = this->selectedGeometryFaceIds;
    if (faceIds.empty() && this->selectedGeometryHit->hit.type == vn::view::render::GeometryHitType::Face &&
        this->selectedGeometryHit->hit.faceId != vn::geom::InvalidFaceId) {
        faceIds.push_back(this->selectedGeometryHit->hit.faceId);
    }
    if (faceIds.empty()) {
        return false;
    }

    std::optional<vn::geom::GeometryObject> beforeGeometry;
    std::optional<vn::geom::GeometryObject> afterGeometry;
    bool changed = false;

    this->document->lock();
    auto* geometry = findMutableGeometryElement(pageIndex, objectId);
    if (!geometry) {
        this->document->unlock();
        return false;
    }

    beforeGeometry = geometry->geometry();
    auto after = *beforeGeometry;
    for (auto faceId: faceIds) {
        changed = after.removeFace(faceId) || changed;
    }
    if (changed) {
        geometry->replaceGeometry(std::move(after));
        afterGeometry = geometry->geometry();
    }
    this->document->unlock();

    if (!changed || !afterGeometry) {
        return false;
    }

    pushGeometryHistory({.pageIndex = pageIndex,
                         .objectId = objectId,
                         .before = std::move(*beforeGeometry),
                         .after = std::move(*afterGeometry),
                         .text = faceIds.size() > 1U ? "Delete geometry faces" : "Delete geometry face"});
    this->selectedGeometryFaceIds.clear();
    rebuildPageSnapshots();
    return true;
}

auto QtDocumentController::splitSelectedGeometryFace() -> bool {
    const auto diagonals = selectedGeometryFaceSplitDiagonals();
    if (diagonals.empty()) {
        return false;
    }
    return splitSelectedGeometryFace(diagonals.front().lhsIndex, diagonals.front().rhsIndex);
}

auto QtDocumentController::splitSelectedGeometryFace(std::size_t lhsIndex, std::size_t rhsIndex) -> bool {
    if (!this->selectedGeometryHit || !this->document) {
        return false;
    }

    const auto faceId = !this->selectedGeometryFaceIds.empty() ? this->selectedGeometryFaceIds.front()
                                                               : this->selectedGeometryHit->hit.faceId;
    if (faceId == vn::geom::InvalidFaceId) {
        return false;
    }

    const auto pageIndex = this->selectedGeometryHit->pageIndex;
    const auto objectId = this->selectedGeometryHit->hit.objectId;
    std::optional<vn::geom::GeometryObject> beforeGeometry;
    std::optional<vn::geom::GeometryObject> afterGeometry;
    bool changed = false;

    this->document->lock();
    auto* geometry = findMutableGeometryElement(pageIndex, objectId);
    if (!geometry) {
        this->document->unlock();
        return false;
    }

    beforeGeometry = geometry->geometry();
    auto after = *beforeGeometry;
    changed = splitFaceByIndices(after, faceId, lhsIndex, rhsIndex);
    if (changed) {
        geometry->replaceGeometry(std::move(after));
        afterGeometry = geometry->geometry();
    }
    this->document->unlock();

    if (!changed || !afterGeometry) {
        return false;
    }

    pushGeometryHistory({.pageIndex = pageIndex,
                         .objectId = objectId,
                         .before = std::move(*beforeGeometry),
                         .after = std::move(*afterGeometry),
                         .text = "Split geometry face"});
    rebuildPageSnapshots();
    return true;
}

auto QtDocumentController::triangulateSelectedGeometryFace() -> bool {
    if (!this->selectedGeometryHit || !this->document) {
        return false;
    }

    const auto faceId = !this->selectedGeometryFaceIds.empty() ? this->selectedGeometryFaceIds.front()
                                                               : this->selectedGeometryHit->hit.faceId;
    if (faceId == vn::geom::InvalidFaceId) {
        return false;
    }

    const auto pageIndex = this->selectedGeometryHit->pageIndex;
    const auto objectId = this->selectedGeometryHit->hit.objectId;
    std::optional<vn::geom::GeometryObject> beforeGeometry;
    std::optional<vn::geom::GeometryObject> afterGeometry;
    bool changed = false;

    this->document->lock();
    auto* geometry = findMutableGeometryElement(pageIndex, objectId);
    if (!geometry) {
        this->document->unlock();
        return false;
    }

    beforeGeometry = geometry->geometry();
    auto after = *beforeGeometry;
    changed = triangulateFace(after, faceId);
    if (changed) {
        geometry->replaceGeometry(std::move(after));
        afterGeometry = geometry->geometry();
    }
    this->document->unlock();

    if (!changed || !afterGeometry) {
        return false;
    }

    pushGeometryHistory({.pageIndex = pageIndex,
                         .objectId = objectId,
                         .before = std::move(*beforeGeometry),
                         .after = std::move(*afterGeometry),
                         .text = "Triangulate geometry face"});
    rebuildPageSnapshots();
    return true;
}

auto QtDocumentController::detachSelectedGeometry() -> bool {
    if (!this->selectedGeometryHit || !this->document) {
        return false;
    }

    const auto pageIndex = this->selectedGeometryHit->pageIndex;
    const auto objectId = this->selectedGeometryHit->hit.objectId;
    std::optional<vn::geom::GeometryObject> beforeGeometry;
    std::optional<vn::geom::GeometryObject> afterGeometry;
    std::vector<vn::geom::VertexId> affectedVertexIds;
    std::vector<vn::geom::EdgeId> affectedEdgeIds;
    std::string actionText = "Detach geometry";
    bool changed = false;

    this->document->lock();
    auto* geometry = findMutableGeometryElement(pageIndex, objectId);
    auto page = pageIndex < this->document->getPageCount() ? this->document->getPage(pageIndex) : nullptr;
    if (!geometry || !page) {
        this->document->unlock();
        return false;
    }

    beforeGeometry = geometry->geometry();
    auto after = *beforeGeometry;
    const auto before = *beforeGeometry;

    const auto hasCoincidentPeer = [&](vn::geom::VertexId vertexId, vn::geom::Vec2 position) {
        for (const auto& vertex: before.vertices()) {
            if (vertex.id != vertexId && coincidentPoint(vertex.position, position)) {
                return true;
            }
        }

        for (Layer* layer: page->getLayers()) {
            if (!layer || !layer->isVisible()) {
                continue;
            }
            for (const auto& element: layer->getElements()) {
                const auto* otherGeometry = dynamic_cast<const vn::geom::GeometryElement*>(element.get());
                if (!otherGeometry || otherGeometry->geometry().objectId() == objectId) {
                    continue;
                }
                for (const auto& vertex: otherGeometry->geometry().vertices()) {
                    if (coincidentPoint(vertex.position, position)) {
                        return true;
                    }
                }
            }
        }
        return false;
    };

    std::vector<vn::geom::EdgeId> edgeIds = this->selectedGeometryEdgeIds;
    if (edgeIds.empty() && this->selectedGeometryHit->hit.type == vn::view::render::GeometryHitType::Edge &&
        this->selectedGeometryHit->hit.edgeId != vn::geom::InvalidEdgeId) {
        edgeIds.push_back(this->selectedGeometryHit->hit.edgeId);
    }

    std::vector<vn::geom::VertexId> vertexIds = this->selectedGeometryVertexIds;
    if (vertexIds.empty() && this->selectedGeometryHit->hit.type == vn::view::render::GeometryHitType::Vertex &&
        this->selectedGeometryHit->hit.vertexId != vn::geom::InvalidVertexId) {
        vertexIds.push_back(this->selectedGeometryHit->hit.vertexId);
    }

    if (!edgeIds.empty()) {
        std::unordered_set<vn::geom::EdgeId> selectedEdgeSet;
        for (auto edgeId: edgeIds) {
            if (after.edge(edgeId)) {
                selectedEdgeSet.insert(edgeId);
            }
        }

        std::unordered_map<vn::geom::VertexId, vn::geom::VertexId> duplicateVertexIds;
        const auto duplicateSelectedEdgeVertex = [&](vn::geom::VertexId vertexId) -> std::optional<vn::geom::VertexId> {
            if (const auto existing = duplicateVertexIds.find(vertexId); existing != duplicateVertexIds.end()) {
                return existing->second;
            }

            const auto* vertex = after.vertex(vertexId);
            if (!vertex) {
                return std::nullopt;
            }
            const auto position = vertex->position;
            const auto flags = vertex->flags;
            const auto duplicate = after.addVertex({position.x + TopologyDetachOffset.x,
                                                    position.y + TopologyDetachOffset.y},
                                                   flags);
            duplicateVertexIds.emplace(vertexId, duplicate);
            appendUnique(affectedVertexIds, vertexId);
            appendUnique(affectedVertexIds, duplicate);
            return duplicate;
        };

        for (auto edgeId: edgeIds) {
            const auto* beforeEdge = before.edge(edgeId);
            if (!beforeEdge || !selectedEdgeSet.contains(edgeId)) {
                continue;
            }

            for (auto vertexId: edgeVertexReferences(*beforeEdge)) {
                const auto* beforeVertex = before.vertex(vertexId);
                if (!beforeVertex) {
                    continue;
                }

                bool sharedWithUnselectedEdge = false;
                for (const auto& edge: before.edges()) {
                    if (!selectedEdgeSet.contains(edge.id) && edgeReferencesVertex(edge, vertexId)) {
                        sharedWithUnselectedEdge = true;
                        break;
                    }
                }

                if (!sharedWithUnselectedEdge && !hasCoincidentPeer(vertexId, beforeVertex->position)) {
                    continue;
                }

                const auto duplicate = duplicateSelectedEdgeVertex(vertexId);
                auto* edge = after.edge(edgeId);
                if (!duplicate || !edge) {
                    continue;
                }

                replaceEdgeVertexReference(*edge, vertexId, *duplicate);
                appendUnique(affectedEdgeIds, edgeId);
                changed = true;
            }
        }

        if (changed) {
            actionText = selectedEdgeSet.size() > 1U ? "Detach geometry edges" : "Detach geometry edge";
        }
    } else if (!vertexIds.empty()) {
        for (auto vertexId: vertexIds) {
            const auto* vertex = after.vertex(vertexId);
            if (!vertex) {
                continue;
            }

            const auto position = vertex->position;
            const auto flags = vertex->flags;
            const auto incidentEdges = incidentEdgeIds(after, vertexId);
            if (incidentEdges.size() > 1U) {
                const auto duplicate = after.addVertex(position, flags);
                for (std::size_t index = 1U; index < incidentEdges.size(); ++index) {
                    if (auto* edge = after.edge(incidentEdges[index])) {
                        replaceEdgeVertexReference(*edge, vertexId, duplicate);
                        appendUnique(affectedEdgeIds, incidentEdges[index]);
                    }
                }
                (void)after.setVertexPosition(vertexId, {position.x + TopologyDetachOffset.x,
                                                         position.y + TopologyDetachOffset.y});
                appendUnique(affectedVertexIds, vertexId);
                appendUnique(affectedVertexIds, duplicate);
                changed = true;
            } else if (hasCoincidentPeer(vertexId, position)) {
                (void)after.setVertexPosition(vertexId, {position.x + TopologyDetachOffset.x,
                                                         position.y + TopologyDetachOffset.y});
                appendUnique(affectedVertexIds, vertexId);
                for (auto edgeId: incidentEdges) {
                    appendUnique(affectedEdgeIds, edgeId);
                }
                changed = true;
            }
        }

        if (changed) {
            actionText = vertexIds.size() > 1U ? "Detach geometry vertices" : "Detach geometry vertex";
        }
    }

    if (changed) {
        removeConstraintsReferencing(after, affectedVertexIds, affectedEdgeIds);
        geometry->replaceGeometry(std::move(after));
        afterGeometry = geometry->geometry();
        if (this->selectedGeometryHit->hit.type == vn::view::render::GeometryHitType::Vertex &&
            this->selectedGeometryHit->hit.vertexId != vn::geom::InvalidVertexId) {
            if (const auto* vertex = afterGeometry->vertex(this->selectedGeometryHit->hit.vertexId)) {
                this->selectedGeometryHit->hit.point = Point(vertex->position.x, vertex->position.y);
            }
        } else if (this->selectedGeometryHit->hit.type == vn::view::render::GeometryHitType::Edge &&
                   this->selectedGeometryHit->hit.edgeId != vn::geom::InvalidEdgeId) {
            if (const auto* edge = afterGeometry->edge(this->selectedGeometryHit->hit.edgeId)) {
                const auto* start = afterGeometry->vertex(edge->start);
                const auto* end = afterGeometry->vertex(edge->end);
                if (start && end) {
                    this->selectedGeometryHit->hit.point =
                            Point((start->position.x + end->position.x) / 2.0,
                                  (start->position.y + end->position.y) / 2.0);
                }
            }
        }
        this->hoveredGeometryHit = this->selectedGeometryHit;
    }
    this->document->unlock();

    if (!changed) {
        return false;
    }

    pushGeometryHistory({.pageIndex = pageIndex,
                         .objectId = objectId,
                         .before = std::move(*beforeGeometry),
                         .after = std::move(*afterGeometry),
                         .text = std::move(actionText)});
    rebuildPageSnapshots();
    return true;
}

auto QtDocumentController::weldSelectedGeometry() -> bool {
    if (!this->selectedGeometryHit || !this->document) {
        return false;
    }

    std::vector<vn::geom::VertexId> vertexIds = this->selectedGeometryVertexIds;
    if (vertexIds.empty() && this->selectedGeometryHit->hit.type == vn::view::render::GeometryHitType::Vertex &&
        this->selectedGeometryHit->hit.vertexId != vn::geom::InvalidVertexId) {
        vertexIds.push_back(this->selectedGeometryHit->hit.vertexId);
    }
    if (vertexIds.empty()) {
        return false;
    }

    struct ExternalWeldTarget {
        vn::geom::ObjectId objectId = vn::geom::InvalidObjectId;
        vn::geom::VertexId targetVertexId = vn::geom::InvalidVertexId;
        vn::geom::VertexId weldVertexId = vn::geom::InvalidVertexId;
        vn::geom::GeometryObject geometry;
    };

    const auto pageIndex = this->selectedGeometryHit->pageIndex;
    const auto objectId = this->selectedGeometryHit->hit.objectId;
    std::optional<vn::geom::GeometryObject> beforeGeometry;
    std::optional<vn::geom::GeometryObject> afterGeometry;
    std::vector<QtGeometryHistoryRemovedElement> removedElements;
    bool changed = false;

    this->document->lock();
    auto* geometry = findMutableGeometryElement(pageIndex, objectId);
    auto page = pageIndex < this->document->getPageCount() ? this->document->getPage(pageIndex) : nullptr;
    if (!geometry || !page) {
        this->document->unlock();
        return false;
    }

    beforeGeometry = geometry->geometry();
    auto after = *beforeGeometry;
    const auto targetVertexId = vertexIds.front();
    if (!after.vertex(targetVertexId)) {
        this->document->unlock();
        return false;
    }

    for (std::size_t index = 1U; index < vertexIds.size(); ++index) {
        changed = after.mergeVertexInto(vertexIds[index], targetVertexId) || changed;
    }

    std::vector<ExternalWeldTarget> externalTargets;
    std::unordered_set<vn::geom::ObjectId> collectedObjects;
    for (auto weldVertexId: vertexIds) {
        const auto* weldVertex = after.vertex(weldVertexId);
        if (!weldVertex) {
            continue;
        }
        for (Layer* layer: page->getLayers()) {
            if (!layer || !layer->isVisible()) {
                continue;
            }
            for (const auto& element: layer->getElements()) {
                const auto* otherGeometry = dynamic_cast<const vn::geom::GeometryElement*>(element.get());
                if (!otherGeometry || otherGeometry->geometry().objectId() == objectId ||
                    collectedObjects.contains(otherGeometry->geometry().objectId())) {
                    continue;
                }
                for (const auto& vertex: otherGeometry->geometry().vertices()) {
                    if (!coincidentPoint(vertex.position, weldVertex->position)) {
                        continue;
                    }
                    externalTargets.push_back(ExternalWeldTarget{
                            .objectId = otherGeometry->geometry().objectId(),
                            .targetVertexId = vertex.id,
                            .weldVertexId = weldVertexId,
                            .geometry = otherGeometry->geometry(),
                    });
                    collectedObjects.insert(otherGeometry->geometry().objectId());
                    break;
                }
            }
        }
    }

    for (const auto& target: externalTargets) {
        auto merged = mergedGeometryForVertexWeld(after, target.geometry, target.targetVertexId, target.weldVertexId);
        if (!merged) {
            continue;
        }
        auto removed = removeGeometryElement(pageIndex, target.objectId);
        if (!removed) {
            continue;
        }
        after = std::move(*merged);
        removedElements.push_back(std::move(*removed));
        changed = true;
    }

    if (changed) {
        geometry->replaceGeometry(std::move(after));
        afterGeometry = geometry->geometry();
        if (this->selectedGeometryHit->hit.vertexId != vn::geom::InvalidVertexId) {
            if (const auto* vertex = afterGeometry->vertex(this->selectedGeometryHit->hit.vertexId)) {
                this->selectedGeometryHit->hit.point = Point(vertex->position.x, vertex->position.y);
            }
        }
        this->hoveredGeometryHit = this->selectedGeometryHit;
    }
    this->document->unlock();

    if (!changed) {
        return false;
    }

    pushGeometryHistory({.pageIndex = pageIndex,
                         .objectId = objectId,
                         .before = std::move(*beforeGeometry),
                         .after = std::move(*afterGeometry),
                         .removedElements = std::move(removedElements),
                         .text = vertexIds.size() > 1U ? "Weld geometry vertices" : "Weld geometry vertex"});
    rebuildPageSnapshots();
    return true;
}

auto QtDocumentController::insertVertexOnSelectedEdge() -> bool {
    if (!this->selectedGeometryHit || !this->document ||
        this->selectedGeometryHit->hit.type != vn::view::render::GeometryHitType::Edge ||
        this->selectedGeometryHit->hit.edgeId == vn::geom::InvalidEdgeId) {
        return false;
    }

    bool changed = false;
    std::optional<vn::geom::GeometryObject> beforeGeometry;
    std::optional<vn::geom::GeometryObject> afterGeometry;
    std::optional<vn::geom::VertexId> insertedVertexId;
    vn::geom::Vec2 insertedPoint{this->selectedGeometryHit->hit.point.x, this->selectedGeometryHit->hit.point.y};
    this->document->lock();
    auto* geometry = findMutableGeometryElement(this->selectedGeometryHit->pageIndex, this->selectedGeometryHit->hit.objectId);
    if (!geometry) {
        this->document->unlock();
        return false;
    }

    beforeGeometry = geometry->geometry();
    insertedVertexId = geometry->insertVertexOnEdge(this->selectedGeometryHit->hit.edgeId, insertedPoint);
    changed = insertedVertexId.has_value();
    if (changed) {
        afterGeometry = geometry->geometry();
        if (const auto* vertex = geometry->geometry().vertex(*insertedVertexId)) {
            insertedPoint = vertex->position;
        }
    }
    this->document->unlock();

    if (changed) {
        pushGeometryHistory({.pageIndex = this->selectedGeometryHit->pageIndex,
                             .objectId = this->selectedGeometryHit->hit.objectId,
                             .before = std::move(*beforeGeometry),
                             .after = std::move(*afterGeometry),
                             .text = "Insert geometry vertex"});
        vn::view::render::GeometryHitResult insertedHit;
        insertedHit.type = vn::view::render::GeometryHitType::Vertex;
        insertedHit.objectId = this->selectedGeometryHit->hit.objectId;
        insertedHit.vertexId = *insertedVertexId;
        insertedHit.edgeId = vn::geom::InvalidEdgeId;
        insertedHit.point = Point(insertedPoint.x, insertedPoint.y);
        insertedHit.snapKind.reset();
        insertedHit.screenDistance = 0.0;
        this->selectedGeometryHit = QtGeometryHit{.pageIndex = this->selectedGeometryHit->pageIndex,
                                                              .hit = std::move(insertedHit)};
        this->hoveredGeometryHit = this->selectedGeometryHit;
        rebuildPageSnapshots();
    }

    return changed;
}

auto QtDocumentController::canUndoGeometryEdit() const -> bool {
    return !this->geometryUndoHistory.empty() ||
           (!this->undoHistory.empty() && std::holds_alternative<QtGeometryHistoryEntry>(this->undoHistory.back().data));
}

auto QtDocumentController::canRedoGeometryEdit() const -> bool {
    return !this->geometryRedoHistory.empty() ||
           (!this->redoHistory.empty() && std::holds_alternative<QtGeometryHistoryEntry>(this->redoHistory.back().data));
}

auto QtDocumentController::undoGeometryEditText() const -> std::string {
    if (!this->geometryUndoHistory.empty()) {
        return this->geometryUndoHistory.back().text;
    }
    if (!this->undoHistory.empty()) {
        if (const auto* entry = std::get_if<QtGeometryHistoryEntry>(&this->undoHistory.back().data)) {
            return entry->text;
        }
    }
    return {};
}

auto QtDocumentController::redoGeometryEditText() const -> std::string {
    if (!this->geometryRedoHistory.empty()) {
        return this->geometryRedoHistory.back().text;
    }
    if (!this->redoHistory.empty()) {
        if (const auto* entry = std::get_if<QtGeometryHistoryEntry>(&this->redoHistory.back().data)) {
            return entry->text;
        }
    }
    return {};
}

auto QtDocumentController::undoGeometryEdit() -> bool {
    if (!this->undoHistory.empty() && std::holds_alternative<QtGeometryHistoryEntry>(this->undoHistory.back().data)) {
        return undo();
    }
    if (this->geometryUndoHistory.empty()) {
        return false;
    }

    auto entry = std::move(this->geometryUndoHistory.back());
    this->geometryUndoHistory.pop_back();
    const bool changed = applyGeometryHistoryEntry(entry, false);
    if (changed) {
        this->geometryRedoHistory.push_back(std::move(entry));
    } else {
        this->geometryUndoHistory.push_back(std::move(entry));
    }
    return changed;
}

auto QtDocumentController::redoGeometryEdit() -> bool {
    if (!this->redoHistory.empty() && std::holds_alternative<QtGeometryHistoryEntry>(this->redoHistory.back().data)) {
        return redo();
    }
    if (this->geometryRedoHistory.empty()) {
        return false;
    }

    auto entry = std::move(this->geometryRedoHistory.back());
    this->geometryRedoHistory.pop_back();
    const bool changed = applyGeometryHistoryEntry(entry, true);
    if (changed) {
        this->geometryUndoHistory.push_back(std::move(entry));
    } else {
        this->geometryRedoHistory.push_back(std::move(entry));
    }
    return changed;
}

auto QtDocumentController::isPdfPath(const std::filesystem::path& path) -> bool {
    return normalizeExtension(path) == ".pdf";
}

auto QtDocumentController::normalizeExtension(const std::filesystem::path& path) -> std::string {
    auto extension = path.extension().string();
    std::transform(extension.begin(), extension.end(), extension.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return extension;
}

void QtDocumentController::rebuildPageSnapshots() {
    this->pageSnapshots.clear();
    invalidateGeometrySnapCache();
    if (!this->document) {
        return;
    }
    this->pageSnapshots = vn::view::render::buildPageRenderSnapshots(
            *this->document, {.renderPdfBackgrounds = false});
    this->geometrySnapProviderCache.resize(this->pageSnapshots.size());
}

void QtDocumentController::invalidateGeometrySnapCache() {
    this->geometrySnapProviderCache.clear();
}

auto QtDocumentController::geometrySnapProviderForPage(std::size_t pageIndex, const PageRef& page) const
        -> std::shared_ptr<vn::snap::GeometrySnapProvider> {
    if (!page) {
        return nullptr;
    }
    if (this->geometrySnapProviderCache.size() < this->pageSnapshots.size()) {
        this->geometrySnapProviderCache.resize(this->pageSnapshots.size());
    }
    if (pageIndex >= this->geometrySnapProviderCache.size()) {
        return nullptr;
    }

    auto& provider = this->geometrySnapProviderCache[pageIndex];
    if (!provider) {
        auto objects = vn::snap::collectGeometryObjects(page);
        if (!objects.empty()) {
            provider = std::make_shared<vn::snap::GeometrySnapProvider>(std::move(objects));
        }
    }
    return provider;
}

auto QtDocumentController::cachedPdfRaster(std::size_t pdfPageNumber, double pageWidth, double pageHeight)
        -> vn::util::RasterImageData {
    const auto percentChange = [](double oldValue, double newValue) {
        const double average = (std::abs(oldValue) + std::abs(newValue)) / 2.0;
        return average <= std::numeric_limits<double>::epsilon() ? 0.0 : std::abs(oldValue - newValue) * 100.0 / average;
    };
    const auto sameSize = [this, pageWidth, pageHeight, percentChange](const QtPdfRasterCacheEntry& entry) {
        if (std::abs(entry.pageWidth - pageWidth) < 0.5 && std::abs(entry.pageHeight - pageHeight) < 0.5) {
            return true;
        }
        return std::max(percentChange(entry.pageWidth, pageWidth), percentChange(entry.pageHeight, pageHeight)) <=
               this->pdfPageRerenderThreshold;
    };
    for (auto& entry: this->pdfRasterCache) {
        if (entry.pdfPageNumber == pdfPageNumber && sameSize(entry)) {
            entry.lastUsed = ++this->pdfRasterUseCounter;
            return entry.raster;
        }
    }

    auto raster = vn::view::render::createPdfPagePreviewRaster(*this->document, pdfPageNumber, pageWidth, pageHeight);
    if (raster.empty()) {
        return raster;
    }

    this->pdfRasterCache.push_back(QtPdfRasterCacheEntry{.pdfPageNumber = pdfPageNumber,
                                                         .pageWidth = pageWidth,
                                                         .pageHeight = pageHeight,
                                                         .lastUsed = ++this->pdfRasterUseCounter,
                                                         .raster = raster});
    prunePdfRasterCache();
    return raster;
}

void QtDocumentController::prunePdfRasterCache() {
    const auto maxSize = static_cast<std::size_t>(std::clamp(this->pdfPageCacheSize, 1, 500));
    if (this->pdfRasterCache.size() <= maxSize) {
        return;
    }
    std::ranges::sort(this->pdfRasterCache, [](const auto& lhs, const auto& rhs) {
        return lhs.lastUsed > rhs.lastUsed;
    });
    this->pdfRasterCache.resize(maxSize);
}

void QtDocumentController::clearPdfRasterCache() {
    this->pdfRasterCache.clear();
    this->pdfRasterUseCounter = 0U;
}

void QtDocumentController::clearGeometryHistory() {
    this->geometryUndoHistory.clear();
    this->geometryRedoHistory.clear();
}

void QtDocumentController::pushGeometryHistory(QtGeometryHistoryEntry entry) {
    pushHistory(QtHistoryEntry{.data = std::move(entry)});
}

auto QtDocumentController::applyGeometryHistoryEntry(QtGeometryHistoryEntry& entry, bool useAfterState)
        -> bool {
    if (!this->document) {
        return false;
    }

    this->document->lock();
    auto* geometry = findMutableGeometryElement(entry.pageIndex, entry.objectId);
    if (!geometry && !entry.removesPrimaryObject) {
        this->document->unlock();
        return false;
    }

    if (geometry && !entry.removesPrimaryObject) {
        geometry->replaceGeometry(useAfterState ? entry.after : entry.before);
    }
    for (const auto& linked: entry.linkedObjects) {
        auto* linkedGeometry = findMutableGeometryElement(linked.pageIndex, linked.objectId);
        if (!linkedGeometry) {
            this->document->unlock();
            return false;
        }
        linkedGeometry->replaceGeometry(useAfterState ? linked.after : linked.before);
    }
    if (useAfterState) {
        for (auto& removed: entry.removedElements) {
            if (removed.removed.e) {
                continue;
            }
            if (removed.pageIndex >= this->document->getPageCount() || !removed.element) {
                this->document->unlock();
                return false;
            }
            auto page = this->document->getPage(removed.pageIndex);
            if (!page) {
                this->document->unlock();
                return false;
            }
            auto& layers = page->getLayers();
            if (removed.layerIndex >= layers.size() || !layers[removed.layerIndex]) {
                this->document->unlock();
                return false;
            }
            auto detached = layers[removed.layerIndex]->removeElement(removed.element);
            if (!detached.e) {
                this->document->unlock();
                return false;
            }
            removed.element = detached.e.get();
            removed.removed = std::move(detached);
        }
    } else {
        std::ranges::sort(entry.removedElements, [](const auto& lhs, const auto& rhs) {
            if (lhs.pageIndex != rhs.pageIndex) {
                return lhs.pageIndex < rhs.pageIndex;
            }
            if (lhs.layerIndex != rhs.layerIndex) {
                return lhs.layerIndex < rhs.layerIndex;
            }
            return lhs.removed.pos < rhs.removed.pos;
        });
        for (auto& removed: entry.removedElements) {
            if (!removed.removed.e) {
                continue;
            }
            if (removed.pageIndex >= this->document->getPageCount()) {
                this->document->unlock();
                return false;
            }
            auto page = this->document->getPage(removed.pageIndex);
            if (!page) {
                this->document->unlock();
                return false;
            }
            auto& layers = page->getLayers();
            if (removed.layerIndex >= layers.size() || !layers[removed.layerIndex]) {
                this->document->unlock();
                return false;
            }
            removed.element = removed.removed.e.get();
            layers[removed.layerIndex]->insertElement(std::move(removed.removed.e), removed.removed.pos);
        }
    }
    this->document->unlock();

    clearInteractiveGeometryState();
    rebuildPageSnapshots();
    return true;
}

auto QtDocumentController::findMutableGeometryElement(std::size_t pageIndex, vn::geom::ObjectId objectId)
        -> vn::geom::GeometryElement* {
    if (!this->document || pageIndex >= this->document->getPageCount()) {
        return nullptr;
    }

    auto page = this->document->getPage(pageIndex);
    if (!page) {
        return nullptr;
    }

    for (Layer* layer: page->getLayers()) {
        if (!layer || !layer->isVisible()) {
            continue;
        }

        for (auto& element: layer->getElements()) {
            auto* geometry = dynamic_cast<vn::geom::GeometryElement*>(element.get());
            if (geometry && geometry->geometry().objectId() == objectId) {
                return geometry;
            }
        }
    }

    return nullptr;
}

auto QtDocumentController::removeGeometryElement(std::size_t pageIndex, vn::geom::ObjectId objectId)
        -> std::optional<QtGeometryHistoryRemovedElement> {
    if (!this->document || pageIndex >= this->document->getPageCount()) {
        return std::nullopt;
    }

    auto page = this->document->getPage(pageIndex);
    if (!page) {
        return std::nullopt;
    }

    auto& layers = page->getLayers();
    for (std::size_t layerIndex = 0U; layerIndex < layers.size(); ++layerIndex) {
        auto* layer = layers[layerIndex];
        if (!layer) {
            continue;
        }
        for (const auto& element: layer->getElements()) {
            const auto* geometry = dynamic_cast<const vn::geom::GeometryElement*>(element.get());
            if (!geometry || geometry->geometry().objectId() != objectId) {
                continue;
            }

            auto removed = layer->removeElement(geometry);
            if (!removed.e) {
                return std::nullopt;
            }
            const auto* removedPtr = removed.e.get();
            return QtGeometryHistoryRemovedElement{
                    .pageIndex = pageIndex,
                    .layerIndex = layerIndex,
                    .removed = std::move(removed),
                    .element = removedPtr,
            };
        }
    }

    return std::nullopt;
}

auto QtDocumentController::gridSnapProviderFor(PageTypeFormat format, double gridSize, double gridTolerance)
        -> std::shared_ptr<const vn::snap::ISnapProvider> {
    switch (format) {
        case PageTypeFormat::Graph:
        case PageTypeFormat::Dotted:
        case PageTypeFormat::IsoDotted:
        case PageTypeFormat::IsoGraph:
            return std::make_shared<vn::snap::GridSnapProvider>(gridSize, gridSize, std::max(1.0, gridTolerance));
        default:
            return std::make_shared<vn::snap::GridSnapProvider>(gridSize, gridSize, std::max(1.0, gridTolerance));
    }
}

// ---------------------------------------------------------------------------
// Stroke input
// ---------------------------------------------------------------------------

auto QtDocumentController::beginStroke(std::size_t pageIndex, double x, double y, double pressure, Color color,
                                       double width, StrokeTool::Value toolType, bool pressureSensitive,
                                       const std::string& lineStyle, int fill) -> bool {
    if (!this->document || pageIndex >= this->document->getPageCount()) {
        return false;
    }

    auto stroke = std::make_unique<Stroke>();
    stroke->setToolType(StrokeTool(toolType));
    stroke->setColor(color);
    stroke->setWidth(width);

    if (toolType == StrokeTool::HIGHLIGHTER) {
        stroke->setFill(128);
    } else if (fill > 0) {
        stroke->setFill(fill);
    }

    if (lineStyle != "plain" && !lineStyle.empty() && toolType == StrokeTool::PEN) {
        stroke->setLineStyle(StrokeStyle::parseStyle(lineStyle));
    }

    const bool hasPressure = pressureSensitive && pressure > 0.0 && toolType == StrokeTool::PEN;
    if (hasPressure) {
        stroke->addPoint(Point(x, y, pressure * width));
    } else {
        stroke->addPoint(Point(x, y));
    }

    this->currentStroke = QtActiveStroke{.pageIndex = pageIndex, .stroke = std::move(stroke), .hasPressure = hasPressure};
    return true;
}

auto QtDocumentController::updateStroke(double x, double y, double pressure) -> bool {
    if (!this->currentStroke) {
        return false;
    }

    auto& stroke = this->currentStroke->stroke;
    const auto pointCount = stroke->getPointCount();
    if (pointCount == 0) {
        return false;
    }

    if (pressure == 0.0) {
        // Some devices emit zero-pressure moves when lifting — ignore them
        return true;
    }

    const auto lastPoint = stroke->getPoint(pointCount - 1);
    Point newPoint;
    if (this->currentStroke->hasPressure) {
        newPoint = Point(x, y, pressure * stroke->getWidth());
    } else {
        newPoint = Point(x, y);
    }

    constexpr double MIN_DISTANCE = 0.3;
    if (newPoint.lineLengthTo(lastPoint) < MIN_DISTANCE) {
        return true;
    }

    stroke->addPoint(newPoint);
    return true;
}

auto QtDocumentController::finalizeStroke(bool recognizeShape, double recognizerMinSize, bool snapRecognizedToGrid)
        -> bool {
    if (!this->currentStroke) {
        return false;
    }

    auto& stroke = this->currentStroke->stroke;

    // A stroke with only one point needs a duplicate to be visible
    if (stroke->getPointCount() == 1) {
        const Point pt = stroke->getPoint(0);
        stroke->addPoint(pt);
    }

    if (stroke->getPointCount() < 2) {
        this->currentStroke.reset();
        return false;
    }

    stroke->freeUnusedPointItems();
    const std::size_t pageIndex = this->currentStroke->pageIndex;

    if (recognizeShape) {
        ShapeRecognizer recognizer;
        if (auto recognized = recognizer.recognizePatterns(stroke.get(), recognizerMinSize)) {
            recognized->setColor(stroke->getColor());
            recognized->setWidth(stroke->hasPressure() ? stroke->getAvgPressure() : stroke->getWidth());

            if (snapRecognizedToGrid) {
                const auto oldSnappedBounds = recognized->getSnappedBounds();
                Point topLeft(oldSnappedBounds.x, oldSnappedBounds.y);
                Point topLeftSnapped = Point(std::round(topLeft.x / 28.0) * 28.0, std::round(topLeft.y / 28.0) * 28.0);

                recognized->move(topLeftSnapped.x - topLeft.x, topLeftSnapped.y - topLeft.y);
                const auto snappedBounds = recognized->getSnappedBounds();
                Point bottomRight(snappedBounds.x + snappedBounds.width, snappedBounds.y + snappedBounds.height);
                Point bottomRightSnapped = Point(std::round(bottomRight.x / 28.0) * 28.0,
                                                 std::round(bottomRight.y / 28.0) * 28.0);

                const double fx = std::abs(snappedBounds.width) > std::numeric_limits<double>::epsilon()
                                          ? (bottomRightSnapped.x - topLeftSnapped.x) / snappedBounds.width
                                          : 1.0;
                const double fy = std::abs(snappedBounds.height) > std::numeric_limits<double>::epsilon()
                                          ? (bottomRightSnapped.y - topLeftSnapped.y) / snappedBounds.height
                                          : 1.0;
                recognized->scale(topLeftSnapped.x, topLeftSnapped.y, fx, fy, 0.0, false);
            }

            stroke = std::move(recognized);
        }
    }

    this->document->lock();
    if (pageIndex >= this->document->getPageCount()) {
        this->document->unlock();
        this->currentStroke.reset();
        return false;
    }

    auto page = this->document->getPage(pageIndex);
    auto* layer = page ? page->getSelectedLayer() : nullptr;
    if (!layer) {
        this->document->unlock();
        this->currentStroke.reset();
        return false;
    }

    const Element* elementPtr = stroke.get();
    layer->addElement(std::move(stroke));
    this->document->unlock();

    pushHistory(QtHistoryEntry{QtStrokeHistoryEntry{.pageIndex = pageIndex, .element = elementPtr, .text = "Draw stroke"}});
    rebuildPageSnapshots();
    this->currentStroke.reset();
    return true;
}

auto QtDocumentController::cancelStroke() -> void { this->currentStroke.reset(); }

auto QtDocumentController::activeStroke() const -> const QtActiveStroke* {
    return this->currentStroke ? &*this->currentStroke : nullptr;
}

// ---------------------------------------------------------------------------
// Text editing
// ---------------------------------------------------------------------------

auto QtDocumentController::insertTextElement(std::size_t pageIndex, std::unique_ptr<Text> text) -> const Element* {
    if (!this->document || !text || pageIndex >= this->document->getPageCount()) {
        return nullptr;
    }
    this->document->lock();
    auto page = this->document->getPage(pageIndex);
    auto* layer = page ? page->getSelectedLayer() : nullptr;
    if (!layer) {
        this->document->unlock();
        return nullptr;
    }

    const auto* ptr = text.get();
    layer->addElement(std::move(text));

    // Push to undo history
    QtTextHistoryEntry entry;
    entry.pageIndex = pageIndex;
    entry.element = ptr;
    entry.isNew = true;
    entry.text = "Insert text";
    pushHistory(QtHistoryEntry{std::move(entry)});

    this->document->unlock();
    rebuildPageSnapshots();
    return ptr;
}

auto QtDocumentController::hitTestTextElement(std::size_t pageIndex, double pageX, double pageY,
                                              double maxDistance) const -> Text* {
    if (!this->document || pageIndex >= this->document->getPageCount()) {
        return nullptr;
    }
    auto page = this->document->getPage(pageIndex);
    auto* layer = page ? page->getSelectedLayer() : nullptr;
    if (!layer) {
        return nullptr;
    }

    Text* best = nullptr;
    double bestDist = maxDistance;

    for (auto& ep: layer->getElements()) {
        if (!ep || ep->getType() != ELEMENT_TEXT) {
            continue;
        }
        auto* t = dynamic_cast<Text*>(ep.get());
        if (!t) {
            continue;
        }
        const double dist = t->distanceTo(pageX, pageY);
        if (dist < bestDist) {
            bestDist = dist;
            best = t;
        }
    }
    return best;
}

// ---------------------------------------------------------------------------
// Document save
// ---------------------------------------------------------------------------

auto QtDocumentController::saveDocument(const std::filesystem::path& path, std::string* errorMessage) -> bool {
    if (!this->document) {
        if (errorMessage) {
            *errorMessage = "No document to save.";
        }
        return false;
    }

    SaveHandler handler;
    this->document->lock();
    handler.prepareSave(this->document.get(), path);
    this->document->unlock();

    handler.saveTo(path);

    const auto& err = handler.getErrorMessage();
    if (!err.empty()) {
        if (errorMessage) {
            *errorMessage = err;
        }
        return false;
    }
    this->loadedPath = path;
    return true;
}

auto QtDocumentController::documentPtr() const -> const Document* { return this->document.get(); }

// ---------------------------------------------------------------------------
// Image insertion
// ---------------------------------------------------------------------------

auto QtDocumentController::insertImage(std::size_t pageIndex, double x, double y, const std::string& imageData,
                                       double width, double height) -> const Element* {
    if (!this->document || pageIndex >= this->document->getPageCount()) {
        return nullptr;
    }
    this->document->lock();
    auto page = this->document->getPage(pageIndex);
    auto* layer = page ? page->getSelectedLayer() : nullptr;
    if (!layer) {
        this->document->unlock();
        return nullptr;
    }

    auto img = std::make_unique<Image>();
    img->setX(x);
    img->setY(y);
    img->setWidth(width);
    img->setHeight(height);
    img->setImage(std::string(imageData));

    const auto* ptr = img.get();
    layer->addElement(std::move(img));

    // Push to undo history (reuse stroke-style undo: remove on undo, re-insert on redo)
    QtStrokeHistoryEntry entry;
    entry.pageIndex = pageIndex;
    entry.element = ptr;
    entry.text = "Insert image";
    pushHistory(QtHistoryEntry{std::move(entry)});

    this->document->unlock();
    rebuildPageSnapshots();
    return ptr;
}

auto QtDocumentController::insertElement(std::size_t pageIndex, ElementPtr element, std::string historyText)
        -> const Element* {
    if (!this->document || !element || pageIndex >= this->document->getPageCount()) {
        return nullptr;
    }

    this->document->lock();
    auto page = this->document->getPage(pageIndex);
    auto* layer = page ? page->getSelectedLayer() : nullptr;
    if (!layer) {
        this->document->unlock();
        return nullptr;
    }

    const auto* ptr = element.get();
    layer->addElement(std::move(element));
    pushHistory(QtHistoryEntry{
            QtStrokeHistoryEntry{.pageIndex = pageIndex, .element = ptr, .text = std::move(historyText)}});
    this->document->unlock();
    rebuildPageSnapshots();
    return ptr;
}

// ---------------------------------------------------------------------------
// Text search
// ---------------------------------------------------------------------------

auto QtDocumentController::findTextInDocument(const std::string& query) const
        -> std::vector<TextSearchResult> {
    std::vector<TextSearchResult> results;
    if (!this->document || query.empty()) {
        return results;
    }

    for (std::size_t pi = 0; pi < this->document->getPageCount(); ++pi) {
        auto page = this->document->getPage(pi);
        if (!page) {
            continue;
        }
        for (auto* layer: page->getLayers()) {
            if (!layer) {
                continue;
            }
            for (const auto& elem: layer->getElements()) {
                if (!elem || elem->getType() != ELEMENT_TEXT) {
                    continue;
                }
                auto* t = dynamic_cast<const Text*>(elem.get());
                if (!t) {
                    continue;
                }
                const auto& text = t->getText();
                // Case-insensitive substring search
                std::string lowerText = text;
                std::string lowerQuery = query;
                std::transform(lowerText.begin(), lowerText.end(), lowerText.begin(),
                               [](unsigned char c) { return std::tolower(c); });
                std::transform(lowerQuery.begin(), lowerQuery.end(), lowerQuery.begin(),
                               [](unsigned char c) { return std::tolower(c); });
                if (lowerText.find(lowerQuery) != std::string::npos) {
                    results.push_back({.pageIndex = pi, .textElement = t, .matchContext = text});
                }
            }
        }
    }
    return results;
}

// ---------------------------------------------------------------------------
// Geometry constraints
// ---------------------------------------------------------------------------

auto QtDocumentController::applyConstraint(vn::geom::ConstraintKind kind) -> bool {
    if (!this->selectedGeometryHit) {
        return false;
    }

    auto* geometry = findMutableGeometryElement(this->selectedGeometryHit->pageIndex,
                                                this->selectedGeometryHit->hit.objectId);
    if (!geometry) {
        return false;
    }

    auto before = geometry->geometry();
    auto after = before;

    try {
        switch (kind) {
            case vn::geom::ConstraintKind::Coincident:
                if (this->selectedGeometryVertexIds.size() < 2U) {
                    return false;
                }
                after.addConstraint(kind, this->selectedGeometryVertexIds);
                break;
            case vn::geom::ConstraintKind::Horizontal:
            case vn::geom::ConstraintKind::Vertical:
                if (this->selectedGeometryVertexIds.size() != 2U) {
                    return false;
                }
                after.addConstraint(kind, {this->selectedGeometryVertexIds[0], this->selectedGeometryVertexIds[1]});
                break;
            case vn::geom::ConstraintKind::FixedLength: {
                if (this->selectedGeometryVertexIds.size() != 2U) {
                    return false;
                }
                const auto* v0 = before.vertex(this->selectedGeometryVertexIds[0]);
                const auto* v1 = before.vertex(this->selectedGeometryVertexIds[1]);
                if (!v0 || !v1) {
                    return false;
                }
                const double length =
                        std::hypot(v1->position.x - v0->position.x, v1->position.y - v0->position.y);
                if (length <= 0.0) {
                    return false;
                }
                after.addConstraint(kind, {this->selectedGeometryVertexIds[0], this->selectedGeometryVertexIds[1]}, {},
                                    length);
                break;
            }
            case vn::geom::ConstraintKind::Parallel:
            case vn::geom::ConstraintKind::Perpendicular:
                if (this->selectedGeometryEdgeIds.size() != 2U) {
                    return false;
                }
                for (const auto edgeId: this->selectedGeometryEdgeIds) {
                    const auto* edge = before.edge(edgeId);
                    if (!edge || !isLineLikeEdge(*edge)) {
                        return false;
                    }
                }
                after.addConstraint(kind, {}, {this->selectedGeometryEdgeIds[0], this->selectedGeometryEdgeIds[1]});
                break;
            case vn::geom::ConstraintKind::Radius: {
                if (this->selectedGeometryEdgeIds.size() != 1U) {
                    return false;
                }
                const auto* edge = before.edge(this->selectedGeometryEdgeIds.front());
                if (!edge || (edge->kind != vn::geom::EdgeKind::Arc &&
                              edge->kind != vn::geom::EdgeKind::ConstructionCircle) ||
                    edge->controls.empty()) {
                    return false;
                }
                const auto* center = before.vertex(edge->controls.front());
                const auto* start = before.vertex(edge->start);
                if (!center || !start) {
                    return false;
                }
                const double radius =
                        std::hypot(start->position.x - center->position.x, start->position.y - center->position.y);
                if (radius <= 0.0) {
                    return false;
                }
                after.addConstraint(kind, {}, {this->selectedGeometryEdgeIds.front()}, radius);
                break;
            }
            case vn::geom::ConstraintKind::EqualLength:
                if (this->selectedGeometryEdgeIds.size() != 2U) {
                    return false;
                }
                for (const auto edgeId: this->selectedGeometryEdgeIds) {
                    const auto* edge = before.edge(edgeId);
                    if (!edge || !isLineLikeEdge(*edge)) {
                        return false;
                    }
                }
                after.addConstraint(kind, {}, {this->selectedGeometryEdgeIds[0], this->selectedGeometryEdgeIds[1]});
                break;
            case vn::geom::ConstraintKind::FixedAngle: {
                if (this->selectedGeometryEdgeIds.size() != 1U) {
                    return false;
                }
                const auto* edge = before.edge(this->selectedGeometryEdgeIds.front());
                if (!edge || !isLineLikeEdge(*edge)) {
                    return false;
                }
                const auto angle = edgeAngle(before, *edge);
                if (!angle) {
                    return false;
                }
                after.addConstraint(kind, {}, {this->selectedGeometryEdgeIds.front()}, *angle);
                break;
            }
            case vn::geom::ConstraintKind::OnEdge: {
                if (this->selectedGeometryVertexIds.size() != 1U || this->selectedGeometryEdgeIds.size() != 1U) {
                    return false;
                }
                const auto* edge = before.edge(this->selectedGeometryEdgeIds.front());
                if (!edge || !isOnEdgeSupportedEdge(*edge) ||
                    edgeReferencesVertex(*edge, this->selectedGeometryVertexIds.front())) {
                    return false;
                }
                after.addConstraint(kind, {this->selectedGeometryVertexIds.front()},
                                    {this->selectedGeometryEdgeIds.front()});
                break;
            }
        }
    } catch (const std::invalid_argument&) {
        return false;
    }

    // Run the constraint solver
    vn::constraints::GeometryConstraintSolver solver;
    (void)solver.apply(after);

    geometry->replaceGeometry(after);
    pushGeometryHistory({.pageIndex = this->selectedGeometryHit->pageIndex,
                         .objectId = this->selectedGeometryHit->hit.objectId,
                         .before = std::move(before),
                         .after = geometry->geometry(),
                         .text = "Apply constraint"});
    rebuildPageSnapshots();
    return true;
}

auto QtDocumentController::deleteSelectedConstraints() -> bool {
    if (!this->selectedGeometryHit) {
        return false;
    }

    auto* geometry = findMutableGeometryElement(this->selectedGeometryHit->pageIndex,
                                                this->selectedGeometryHit->hit.objectId);
    if (!geometry) {
        return false;
    }

    if (this->selectedGeometryVertexIds.empty() && this->selectedGeometryEdgeIds.empty()) {
        return false;
    }

    const auto before = geometry->geometry();
    auto after = before;
    std::vector<vn::geom::ConstraintId> removedIds;

    for (const auto& constraint: before.constraints()) {
        bool intersects = false;
        for (const auto& vid: constraint.vertices) {
            if (std::find(this->selectedGeometryVertexIds.begin(), this->selectedGeometryVertexIds.end(), vid) !=
                this->selectedGeometryVertexIds.end()) {
                intersects = true;
                break;
            }
        }
        if (!intersects) {
            for (const auto& eid: constraint.edges) {
                if (std::find(this->selectedGeometryEdgeIds.begin(), this->selectedGeometryEdgeIds.end(), eid) !=
                    this->selectedGeometryEdgeIds.end()) {
                    intersects = true;
                    break;
                }
            }
        }
        if (intersects) {
            removedIds.push_back(constraint.id);
        }
    }

    if (removedIds.empty()) {
        return false;
    }

    for (const auto id: removedIds) {
        (void)after.removeConstraint(id);
    }

    geometry->replaceGeometry(after);
    pushGeometryHistory({.pageIndex = this->selectedGeometryHit->pageIndex,
                         .objectId = this->selectedGeometryHit->hit.objectId,
                         .before = std::move(before),
                         .after = geometry->geometry(),
                         .text = "Delete constraint"});
    rebuildPageSnapshots();
    return true;
}

auto QtDocumentController::selectedFixedLengthConstraint() -> std::optional<vn::geom::Constraint> {
    if (!this->selectedGeometryHit) {
        return std::nullopt;
    }
    const auto* geometry = findMutableGeometryElement(this->selectedGeometryHit->pageIndex,
                                                     this->selectedGeometryHit->hit.objectId);
    if (!geometry) {
        return std::nullopt;
    }

    for (const auto& constraint: geometry->geometry().constraints()) {
        if (constraint.kind != vn::geom::ConstraintKind::FixedLength &&
            constraint.kind != vn::geom::ConstraintKind::Radius) {
            continue;
        }
        // Check if any selected vertex/edge is part of this constraint
        for (const auto& vid: constraint.vertices) {
            if (std::find(this->selectedGeometryVertexIds.begin(), this->selectedGeometryVertexIds.end(), vid) !=
                this->selectedGeometryVertexIds.end()) {
                return constraint;
            }
        }
        for (const auto& eid: constraint.edges) {
            if (std::find(this->selectedGeometryEdgeIds.begin(), this->selectedGeometryEdgeIds.end(), eid) !=
                this->selectedGeometryEdgeIds.end()) {
                return constraint;
            }
        }
    }
    return std::nullopt;
}

auto QtDocumentController::updateFixedLengthConstraint(double value) -> bool {
    if (!this->selectedGeometryHit || value <= 0.0) {
        return false;
    }

    auto* geometry = findMutableGeometryElement(this->selectedGeometryHit->pageIndex,
                                                this->selectedGeometryHit->hit.objectId);
    if (!geometry) {
        return false;
    }

    auto constraint = selectedFixedLengthConstraint();
    if (!constraint) {
        return false;
    }

    auto before = geometry->geometry();
    auto after = before;

    auto updatedConstraint = *constraint;
    updatedConstraint.value = value;
    (void)after.replaceConstraint(updatedConstraint);

    vn::constraints::GeometryConstraintSolver solver;
    (void)solver.apply(after);

    geometry->replaceGeometry(after);
    pushGeometryHistory({.pageIndex = this->selectedGeometryHit->pageIndex,
                         .objectId = this->selectedGeometryHit->hit.objectId,
                         .before = std::move(before),
                         .after = geometry->geometry(),
                         .text = "Edit constraint value"});
    rebuildPageSnapshots();
    return true;
}
