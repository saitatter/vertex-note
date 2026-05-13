#include <array>
#include <filesystem>
#include <stdexcept>
#include <string_view>
#include <vector>

#include <config-test.h>
#include <gtest/gtest.h>

#include "QtDocumentController.h"
#include "QtToolState.h"
#include "vertexnote/geometry/SurfaceMesh.h"
#include "view/render/Renderers.h"

namespace {

auto testFile(const char8_t* relativePath) -> std::filesystem::path { return std::filesystem::path(GET_TESTFILE(relativePath)); }

auto sourceFile(const char8_t* relativePath) -> std::filesystem::path {
    return std::filesystem::path(std::u8string(PROJECT_SOURCE_DIR)) / std::filesystem::path(relativePath);
}

auto strokeCount(const vn::view::render::PageRenderSnapshot& snapshot) -> std::size_t {
    std::size_t count = 0;
    for (const auto& drawable: snapshot.drawables) {
        if (std::holds_alternative<vn::view::render::StrokeRenderModel>(drawable)) {
            ++count;
        }
    }
    return count;
}

auto geometryCount(const vn::view::render::PageRenderSnapshot& snapshot) -> std::size_t {
    std::size_t count = 0;
    for (const auto& drawable: snapshot.drawables) {
        if (std::holds_alternative<vn::view::render::GeometryRenderModel>(drawable)) {
            ++count;
        }
    }
    return count;
}

auto geometries(const vn::view::render::PageRenderSnapshot& snapshot)
        -> std::vector<vn::view::render::GeometryRenderModel> {
    std::vector<vn::view::render::GeometryRenderModel> result;
    for (const auto& drawable: snapshot.drawables) {
        if (const auto* geometry = std::get_if<vn::view::render::GeometryRenderModel>(&drawable)) {
            result.push_back(*geometry);
        }
    }
    return result;
}

auto lastStroke(const vn::view::render::PageRenderSnapshot& snapshot)
        -> const vn::view::render::StrokeRenderModel& {
    for (auto it = snapshot.drawables.rbegin(); it != snapshot.drawables.rend(); ++it) {
        if (auto* stroke = std::get_if<vn::view::render::StrokeRenderModel>(&*it)) {
            return *stroke;
        }
    }
    throw std::runtime_error("Expected a stroke drawable");
}

auto lastGeometry(const vn::view::render::PageRenderSnapshot& snapshot)
        -> const vn::view::render::GeometryRenderModel& {
    for (auto it = snapshot.drawables.rbegin(); it != snapshot.drawables.rend(); ++it) {
        if (auto* geometry = std::get_if<vn::view::render::GeometryRenderModel>(&*it)) {
            return *geometry;
        }
    }
    throw std::runtime_error("Expected a geometry drawable");
}

auto strokes(const vn::view::render::PageRenderSnapshot& snapshot) -> std::vector<vn::view::render::StrokeRenderModel> {
    std::vector<vn::view::render::StrokeRenderModel> result;
    for (const auto& drawable: snapshot.drawables) {
        if (const auto* stroke = std::get_if<vn::view::render::StrokeRenderModel>(&drawable)) {
            result.push_back(*stroke);
        }
    }
    return result;
}

}  // namespace

TEST(VertexNoteQtDocumentControllerShapeTools, createsLegacyStrokeShapesForQtShell) {
    QtDocumentController controller;
    constexpr std::size_t PageIndex = 0U;
    const Color shapeColor{0x3366ccffU};

    ASSERT_NE(nullptr, controller.createEllipse(PageIndex, 10.0, 20.0, 110.0, 80.0, shapeColor, 2.0, "plain", 96));
    ASSERT_NE(nullptr,
              controller.createArrow(PageIndex, 20.0, 40.0, 160.0, 120.0, shapeColor, 3.0, "dash", false));
    ASSERT_NE(nullptr, controller.createArrow(PageIndex, 40.0, 50.0, 180.0, 150.0, shapeColor, 3.5, "plain", true));
    ASSERT_NE(nullptr, controller.createCoordinateSystem(PageIndex, 25.0, 25.0, 140.0, 120.0, shapeColor, 1.5, "dot"));
    ASSERT_NE(nullptr, controller.createSpline(PageIndex, {{30.0, 60.0}, {70.0, 30.0}, {120.0, 80.0}, {170.0, 55.0}},
                                               shapeColor, 2.25, "dashdot"));

    const auto& pages = controller.snapshotPages();
    ASSERT_EQ(1U, pages.size());
    EXPECT_EQ(5U, strokeCount(pages.front()));

    const auto& splineStroke = lastStroke(pages.front());
    EXPECT_EQ(shapeColor, splineStroke.color);
    EXPECT_DOUBLE_EQ(2.25, splineStroke.width);
    EXPECT_GT(splineStroke.points.size(), 4U);
}

TEST(VertexNoteQtDocumentControllerShapeTools, separatesStrokeLineFromVertexEdge) {
    QtDocumentController controller;
    constexpr std::size_t PageIndex = 0U;

    ASSERT_NE(nullptr, controller.createLine(PageIndex, 10.0, 20.0, 90.0, 20.0, Colors::black, 2.0));
    ASSERT_EQ(1U, strokeCount(controller.snapshotPages().front()));
    EXPECT_EQ(0U, geometryCount(controller.snapshotPages().front()));

    ASSERT_NE(nullptr, controller.createEdge(PageIndex, 10.0, 50.0, 90.0, 50.0, Colors::black, 2.0));
    EXPECT_EQ(1U, strokeCount(controller.snapshotPages().front()));
    EXPECT_EQ(1U, geometryCount(controller.snapshotPages().front()));
}

TEST(VertexNoteQtDocumentControllerShapeTools, snapsPagePointsToGridOnPlainPages) {
    QtDocumentController controller;

    const auto snapped = controller.snapPagePoint(
            0U, 12.0, 19.0, 1.0,
            {.geometryEnabled = false, .gridEnabled = true, .gridSize = 10.0, .gridTolerance = 1.0});

    ASSERT_TRUE(snapped.snapped);
    ASSERT_TRUE(snapped.snapKind.has_value());
    EXPECT_EQ(vn::snap::SnapKind::Grid, *snapped.snapKind);
    EXPECT_DOUBLE_EQ(10.0, snapped.pagePoint.x);
    EXPECT_DOUBLE_EQ(20.0, snapped.pagePoint.y);
}

TEST(VertexNoteQtDocumentControllerShapeTools, snapsPagePointsToGeometryVertices) {
    QtDocumentController controller;
    ASSERT_NE(nullptr, controller.createEdge(0U, 50.0, 50.0, 120.0, 50.0, Colors::black, 1.0));

    const auto snapped = controller.snapPagePoint(
            0U, 52.0, 49.0, 1.0,
            {.geometryEnabled = true, .gridEnabled = false, .gridSize = 10.0, .gridTolerance = 1.0});

    ASSERT_TRUE(snapped.snapped);
    ASSERT_TRUE(snapped.snapKind.has_value());
    EXPECT_EQ(vn::snap::SnapKind::ExplicitVertex, *snapped.snapKind);
    EXPECT_DOUBLE_EQ(50.0, snapped.pagePoint.x);
    EXPECT_DOUBLE_EQ(50.0, snapped.pagePoint.y);
}

TEST(VertexNoteQtDocumentControllerShapeTools, surfaceMeshValidatesGeometryTopology) {
    vn::geom::GeometryObject object(42U);
    const auto a = object.addVertex({10.0, 10.0});
    const auto b = object.addVertex({50.0, 10.0});
    ASSERT_NE(vn::geom::InvalidEdgeId, object.addLine(a, b));

    const auto mesh = vn::geom::SurfaceMesh::fromGeometryObject(object);
    EXPECT_EQ(42U, mesh.objectId);
    EXPECT_TRUE(mesh.validate().valid);
    EXPECT_TRUE(vn::geom::validateGeometryTopology(object).valid);
}

TEST(VertexNoteQtDocumentControllerShapeTools, classifiesSnapFamiliesForDrawingTools) {
    constexpr std::array strokeTools = {
            QtToolType::DrawLine,
            QtToolType::DrawRectangle,
            QtToolType::DrawEllipse,
            QtToolType::DrawArrow,
            QtToolType::DrawDoubleArrow,
            QtToolType::DrawCoordinateSystem,
            QtToolType::DrawSpline,
            QtToolType::ShapeRecognizer,
    };
    constexpr std::array vertexTools = {
            QtToolType::DrawEdge,
            QtToolType::DrawCircle,
            QtToolType::DrawArc,
            QtToolType::DrawPolyline,
            QtToolType::DrawConstructionLine,
            QtToolType::DrawConstructionCircle,
    };

    QtToolState state;
    for (const auto tool: strokeTools) {
        state.activeTool = tool;
        EXPECT_TRUE(state.isShapeDrawingTool());
        EXPECT_TRUE(state.isStrokeDrawingTool());
        EXPECT_FALSE(state.isVertexDrawingTool());
    }
    for (const auto tool: vertexTools) {
        state.activeTool = tool;
        EXPECT_TRUE(state.isShapeDrawingTool());
        EXPECT_FALSE(state.isStrokeDrawingTool());
        EXPECT_TRUE(state.isVertexDrawingTool());
    }
}

TEST(VertexNoteQtDocumentControllerShapeTools, translatesSelectedVerticesWithUndoRedo) {
    QtDocumentController controller;
    ASSERT_NE(nullptr, controller.createEdge(0U, 50.0, 50.0, 120.0, 50.0, Colors::black, 1.0));

    auto hit = controller.hitTestGeometry(0U, 50.0, 50.0, 1.0, 8.0);
    ASSERT_TRUE(hit.has_value());
    ASSERT_EQ(vn::view::render::GeometryHitType::Vertex, hit->hit.type);
    controller.setSelectedGeometry(*hit);

    ASSERT_TRUE(controller.translateSelectedVertices(7.0, -3.0));
    const auto& translated = lastGeometry(controller.snapshotPages().front());
    ASSERT_FALSE(translated.vertices.empty());
    EXPECT_DOUBLE_EQ(57.0, translated.vertices.front().position.x);
    EXPECT_DOUBLE_EQ(47.0, translated.vertices.front().position.y);

    ASSERT_TRUE(controller.undoGeometryEdit());
    const auto& undone = lastGeometry(controller.snapshotPages().front());
    EXPECT_DOUBLE_EQ(50.0, undone.vertices.front().position.x);
    EXPECT_DOUBLE_EQ(50.0, undone.vertices.front().position.y);

    ASSERT_TRUE(controller.redoGeometryEdit());
    const auto& redone = lastGeometry(controller.snapshotPages().front());
    EXPECT_DOUBLE_EQ(57.0, redone.vertices.front().position.x);
    EXPECT_DOUBLE_EQ(47.0, redone.vertices.front().position.y);
}

TEST(VertexNoteQtDocumentControllerShapeTools, translatingSelectedEdgeMovesOnlyEdgeVerticesAndKeepsMeshAttached) {
    QtDocumentController controller;
    ASSERT_NE(nullptr,
              controller.createPolyline(0U, {{10.0, 10.0}, {50.0, 10.0}, {90.0, 10.0}}, Colors::black, 1.0));

    auto hit = controller.hitTestGeometry(0U, 30.0, 10.0, 1.0, 8.0);
    ASSERT_TRUE(hit.has_value());
    ASSERT_EQ(vn::view::render::GeometryHitType::Edge, hit->hit.type);
    controller.setSelectedGeometry(*hit);

    ASSERT_TRUE(controller.translateSelectedVertices(0.0, 20.0));
    auto translated = geometries(controller.snapshotPages().front());
    ASSERT_EQ(1U, translated.size());
    ASSERT_EQ(3U, translated.front().vertices.size());
    ASSERT_EQ(2U, translated.front().edges.size());

    EXPECT_DOUBLE_EQ(10.0, translated.front().edges[0].start.x);
    EXPECT_DOUBLE_EQ(30.0, translated.front().edges[0].start.y);
    EXPECT_DOUBLE_EQ(50.0, translated.front().edges[0].end.x);
    EXPECT_DOUBLE_EQ(30.0, translated.front().edges[0].end.y);
    EXPECT_DOUBLE_EQ(50.0, translated.front().edges[1].start.x);
    EXPECT_DOUBLE_EQ(30.0, translated.front().edges[1].start.y);
    EXPECT_DOUBLE_EQ(90.0, translated.front().edges[1].end.x);
    EXPECT_DOUBLE_EQ(10.0, translated.front().edges[1].end.y);

    ASSERT_TRUE(controller.undo());
    auto undone = geometries(controller.snapshotPages().front());
    ASSERT_EQ(1U, undone.size());
    EXPECT_DOUBLE_EQ(10.0, undone.front().edges[0].start.y);
    EXPECT_DOUBLE_EQ(10.0, undone.front().edges[0].end.y);
    EXPECT_DOUBLE_EQ(10.0, undone.front().edges[1].start.y);
    EXPECT_DOUBLE_EQ(10.0, undone.front().edges[1].end.y);
}

TEST(VertexNoteQtDocumentControllerShapeTools, rotatesSelectedGeometryObjectWithUndoRedo) {
    QtDocumentController controller;
    ASSERT_NE(nullptr, controller.createEdge(0U, 10.0, 20.0, 30.0, 20.0, Colors::black, 1.0));

    auto hit = controller.hitTestGeometry(0U, 20.0, 20.0, 1.0, 8.0);
    ASSERT_TRUE(hit.has_value());
    ASSERT_EQ(vn::view::render::GeometryHitType::Edge, hit->hit.type);
    controller.setSelectedGeometry(*hit);

    ASSERT_TRUE(controller.rotateSelectedGeometry(90.0));
    const auto& rotated = lastGeometry(controller.snapshotPages().front());
    ASSERT_EQ(2U, rotated.vertices.size());
    EXPECT_NEAR(20.0, rotated.vertices[0].position.x, 0.0001);
    EXPECT_NEAR(10.0, rotated.vertices[0].position.y, 0.0001);
    EXPECT_NEAR(20.0, rotated.vertices[1].position.x, 0.0001);
    EXPECT_NEAR(30.0, rotated.vertices[1].position.y, 0.0001);

    ASSERT_TRUE(controller.undoGeometryEdit());
    const auto& undone = lastGeometry(controller.snapshotPages().front());
    EXPECT_NEAR(10.0, undone.vertices[0].position.x, 0.0001);
    EXPECT_NEAR(20.0, undone.vertices[0].position.y, 0.0001);
    EXPECT_NEAR(30.0, undone.vertices[1].position.x, 0.0001);
    EXPECT_NEAR(20.0, undone.vertices[1].position.y, 0.0001);

    ASSERT_TRUE(controller.redoGeometryEdit());
    const auto& redone = lastGeometry(controller.snapshotPages().front());
    EXPECT_NEAR(20.0, redone.vertices[0].position.x, 0.0001);
    EXPECT_NEAR(10.0, redone.vertices[0].position.y, 0.0001);
    EXPECT_NEAR(20.0, redone.vertices[1].position.x, 0.0001);
    EXPECT_NEAR(30.0, redone.vertices[1].position.y, 0.0001);
}

TEST(VertexNoteQtDocumentControllerShapeTools, transformStatePreservesGeometryTopology) {
    QtDocumentController controller;
    ASSERT_NE(nullptr,
              controller.createPolyline(0U, {{10.0, 10.0}, {50.0, 10.0}, {90.0, 10.0}}, Colors::black, 1.0));

    auto hit = controller.hitTestGeometry(0U, 50.0, 10.0, 1.0, 8.0);
    ASSERT_TRUE(hit.has_value());
    ASSERT_EQ(vn::view::render::GeometryHitType::Vertex, hit->hit.type);
    controller.setSelectedGeometry(*hit);
    ASSERT_TRUE(controller.beginSelectedGeometryTransform());
    ASSERT_TRUE(controller.updateSelectedGeometryTransform(5.0, 10.0, 35.0));
    ASSERT_TRUE(controller.endSelectedGeometryTransform());

    auto transformed = geometries(controller.snapshotPages().front());
    ASSERT_EQ(1U, transformed.size());
    ASSERT_EQ(3U, transformed.front().vertices.size());
    ASSERT_EQ(2U, transformed.front().edges.size());
    EXPECT_DOUBLE_EQ(transformed.front().vertices[0].position.x, transformed.front().edges[0].start.x);
    EXPECT_DOUBLE_EQ(transformed.front().vertices[0].position.y, transformed.front().edges[0].start.y);
    EXPECT_DOUBLE_EQ(transformed.front().vertices[1].position.x, transformed.front().edges[0].end.x);
    EXPECT_DOUBLE_EQ(transformed.front().vertices[1].position.y, transformed.front().edges[0].end.y);
    EXPECT_DOUBLE_EQ(transformed.front().vertices[1].position.x, transformed.front().edges[1].start.x);
    EXPECT_DOUBLE_EQ(transformed.front().vertices[1].position.y, transformed.front().edges[1].start.y);
    EXPECT_DOUBLE_EQ(transformed.front().vertices[2].position.x, transformed.front().edges[1].end.x);
    EXPECT_DOUBLE_EQ(transformed.front().vertices[2].position.y, transformed.front().edges[1].end.y);
}

TEST(VertexNoteQtDocumentControllerShapeTools, directVertexDragKeepsConnectedEdgesAttachedAndUndoable) {
    QtDocumentController controller;
    ASSERT_NE(nullptr,
              controller.createPolyline(0U, {{10.0, 10.0}, {50.0, 10.0}, {90.0, 10.0}}, Colors::black, 1.0));

    auto hit = controller.hitTestGeometry(0U, 50.0, 10.0, 1.0, 8.0);
    ASSERT_TRUE(hit.has_value());
    ASSERT_EQ(vn::view::render::GeometryHitType::Vertex, hit->hit.type);
    ASSERT_TRUE(controller.beginGeometryVertexDrag(*hit));
    ASSERT_TRUE(controller.updateGeometryVertexDrag(
            50.0, 40.0, 1.0,
            {.geometryEnabled = false, .gridEnabled = false, .gridSize = 10.0, .gridTolerance = 1.0,
             .screenTolerance = 18.0}));

    auto preview = geometries(controller.snapshotPages().front());
    ASSERT_EQ(1U, preview.size());
    ASSERT_EQ(2U, preview.front().edges.size());
    EXPECT_DOUBLE_EQ(50.0, preview.front().edges[0].end.x);
    EXPECT_DOUBLE_EQ(40.0, preview.front().edges[0].end.y);
    EXPECT_DOUBLE_EQ(50.0, preview.front().edges[1].start.x);
    EXPECT_DOUBLE_EQ(40.0, preview.front().edges[1].start.y);

    ASSERT_TRUE(controller.endGeometryVertexDrag());
    ASSERT_TRUE(controller.canUndo());
    ASSERT_TRUE(controller.undo());
    auto undone = geometries(controller.snapshotPages().front());
    ASSERT_EQ(1U, undone.size());
    EXPECT_DOUBLE_EQ(50.0, undone.front().edges[0].end.x);
    EXPECT_DOUBLE_EQ(10.0, undone.front().edges[0].end.y);
    EXPECT_DOUBLE_EQ(50.0, undone.front().edges[1].start.x);
    EXPECT_DOUBLE_EQ(10.0, undone.front().edges[1].start.y);

    ASSERT_TRUE(controller.canRedo());
    ASSERT_TRUE(controller.redo());
    auto redone = geometries(controller.snapshotPages().front());
    ASSERT_EQ(1U, redone.size());
    EXPECT_DOUBLE_EQ(50.0, redone.front().edges[0].end.x);
    EXPECT_DOUBLE_EQ(40.0, redone.front().edges[0].end.y);
    EXPECT_DOUBLE_EQ(50.0, redone.front().edges[1].start.x);
    EXPECT_DOUBLE_EQ(40.0, redone.front().edges[1].start.y);
}

TEST(VertexNoteQtDocumentControllerShapeTools, draggingVertexOntoOwnEdgeSplitsEdgeWithoutDuplicatingVertex) {
    QtDocumentController controller;
    ASSERT_NE(nullptr, controller.createRectangle(0U, 10.0, 10.0, 90.0, 90.0, Colors::black, 1.0));

    auto hit = controller.hitTestGeometry(0U, 10.0, 90.0, 1.0, 8.0);
    ASSERT_TRUE(hit.has_value());
    ASSERT_EQ(vn::view::render::GeometryHitType::Vertex, hit->hit.type);
    const auto draggedVertex = hit->hit.vertexId;

    ASSERT_TRUE(controller.beginGeometryVertexDrag(*hit));
    ASSERT_TRUE(controller.updateGeometryVertexDrag(
            50.0, 12.0, 1.0,
            {.geometryEnabled = true, .gridEnabled = false, .gridSize = 10.0, .gridTolerance = 1.0,
             .screenTolerance = 18.0}));
    ASSERT_TRUE(controller.endGeometryVertexDrag());

    auto attached = geometries(controller.snapshotPages().front());
    ASSERT_EQ(1U, attached.size());
    EXPECT_EQ(4U, attached.front().vertices.size());
    EXPECT_EQ(5U, attached.front().edges.size());

    const auto movedVertex =
            std::ranges::find(attached.front().vertices, draggedVertex, &vn::view::render::GeometryVertexRenderModel::id);
    ASSERT_NE(attached.front().vertices.end(), movedVertex);
    EXPECT_DOUBLE_EQ(50.0, movedVertex->position.x);
    EXPECT_DOUBLE_EQ(10.0, movedVertex->position.y);

    const auto samePoint = [](const Point& lhs, const Point& rhs) {
        return lhs.x == rhs.x && lhs.y == rhs.y;
    };
    const auto incidentSplitEdges = std::ranges::count_if(attached.front().edges, [&](const auto& edge) {
        return (samePoint(edge.start, movedVertex->position) && edge.end.y == 10.0) ||
               (samePoint(edge.end, movedVertex->position) && edge.start.y == 10.0);
    });
    EXPECT_GE(incidentSplitEdges, 2);

    ASSERT_TRUE(controller.undo());
    auto undone = geometries(controller.snapshotPages().front());
    ASSERT_EQ(1U, undone.size());
    EXPECT_EQ(4U, undone.front().vertices.size());
    EXPECT_EQ(4U, undone.front().edges.size());
    const auto undoneVertex =
            std::ranges::find(undone.front().vertices, draggedVertex, &vn::view::render::GeometryVertexRenderModel::id);
    ASSERT_NE(undone.front().vertices.end(), undoneVertex);
    EXPECT_DOUBLE_EQ(10.0, undoneVertex->position.x);
    EXPECT_DOUBLE_EQ(90.0, undoneVertex->position.y);

    ASSERT_TRUE(controller.redo());
    auto redone = geometries(controller.snapshotPages().front());
    ASSERT_EQ(1U, redone.size());
    EXPECT_EQ(4U, redone.front().vertices.size());
    EXPECT_EQ(5U, redone.front().edges.size());
}

TEST(VertexNoteQtDocumentControllerShapeTools, draggingVertexOntoOwnVertexWeldsTopologyWithUndoRedo) {
    QtDocumentController controller;
    ASSERT_NE(nullptr,
              controller.createPolyline(0U, {{10.0, 10.0}, {50.0, 10.0}, {90.0, 10.0}}, Colors::black, 1.0));

    auto hit = controller.hitTestGeometry(0U, 10.0, 10.0, 1.0, 8.0);
    ASSERT_TRUE(hit.has_value());
    ASSERT_EQ(vn::view::render::GeometryHitType::Vertex, hit->hit.type);

    ASSERT_TRUE(controller.beginGeometryVertexDrag(*hit));
    ASSERT_TRUE(controller.updateGeometryVertexDrag(
            49.0, 10.0, 1.0,
            {.geometryEnabled = true, .gridEnabled = false, .gridSize = 10.0, .gridTolerance = 1.0,
             .screenTolerance = 18.0}));
    ASSERT_TRUE(controller.endGeometryVertexDrag());

    auto welded = geometries(controller.snapshotPages().front());
    ASSERT_EQ(1U, welded.size());
    EXPECT_EQ(2U, welded.front().vertices.size());
    EXPECT_EQ(1U, welded.front().edges.size());
    EXPECT_DOUBLE_EQ(50.0, welded.front().edges.front().start.x);
    EXPECT_DOUBLE_EQ(10.0, welded.front().edges.front().start.y);
    EXPECT_DOUBLE_EQ(90.0, welded.front().edges.front().end.x);
    EXPECT_DOUBLE_EQ(10.0, welded.front().edges.front().end.y);

    ASSERT_TRUE(controller.undo());
    auto undone = geometries(controller.snapshotPages().front());
    ASSERT_EQ(1U, undone.size());
    EXPECT_EQ(3U, undone.front().vertices.size());
    EXPECT_EQ(2U, undone.front().edges.size());

    ASSERT_TRUE(controller.redo());
    auto redone = geometries(controller.snapshotPages().front());
    ASSERT_EQ(1U, redone.size());
    EXPECT_EQ(2U, redone.front().vertices.size());
    EXPECT_EQ(1U, redone.front().edges.size());
}

TEST(VertexNoteQtDocumentControllerShapeTools, draggingVertexOntoOtherObjectEdgeWeldsIntoOneMeshWithUndoRedo) {
    QtDocumentController controller;
    ASSERT_NE(nullptr, controller.createEdge(0U, 10.0, 10.0, 110.0, 10.0, Colors::black, 1.0));
    ASSERT_NE(nullptr, controller.createEdge(0U, 40.0, 50.0, 80.0, 50.0, Colors::black, 1.0));

    auto hit = controller.hitTestGeometry(0U, 40.0, 50.0, 1.0, 8.0);
    ASSERT_TRUE(hit.has_value());
    ASSERT_EQ(vn::view::render::GeometryHitType::Vertex, hit->hit.type);
    ASSERT_TRUE(controller.beginGeometryVertexDrag(*hit));
    ASSERT_TRUE(controller.updateGeometryVertexDrag(
            60.0, 12.0, 1.0,
            {.geometryEnabled = true, .gridEnabled = false, .gridSize = 10.0, .gridTolerance = 1.0,
             .screenTolerance = 18.0}));
    ASSERT_TRUE(controller.endGeometryVertexDrag());

    auto after = geometries(controller.snapshotPages().front());
    ASSERT_EQ(1U, after.size());
    EXPECT_EQ(4U, after.front().vertices.size());
    EXPECT_EQ(3U, after.front().edges.size());
    const auto weldedVertex =
            std::ranges::find_if(after.front().vertices, [](const auto& vertex) {
                return vertex.position.x == 60.0 && vertex.position.y == 10.0;
            });
    ASSERT_NE(after.front().vertices.end(), weldedVertex);
    const auto incidentEdges = std::ranges::count_if(after.front().edges, [&](const auto& edge) {
        return (edge.start.x == 60.0 && edge.start.y == 10.0) || (edge.end.x == 60.0 && edge.end.y == 10.0);
    });
    EXPECT_GE(incidentEdges, 3);

    ASSERT_TRUE(controller.undoGeometryEdit());
    auto undone = geometries(controller.snapshotPages().front());
    ASSERT_EQ(2U, undone.size());
    EXPECT_EQ(2U, undone[0].vertices.size());
    EXPECT_EQ(1U, undone[0].edges.size());
    EXPECT_DOUBLE_EQ(40.0, undone[1].vertices.front().position.x);
    EXPECT_DOUBLE_EQ(50.0, undone[1].vertices.front().position.y);

    ASSERT_TRUE(controller.redoGeometryEdit());
    auto redone = geometries(controller.snapshotPages().front());
    ASSERT_EQ(1U, redone.size());
    EXPECT_EQ(4U, redone.front().vertices.size());
    EXPECT_EQ(3U, redone.front().edges.size());
}

TEST(VertexNoteQtDocumentControllerShapeTools, draggingVertexOntoOtherObjectVertexWeldsIntoOneMeshWithUndoRedo) {
    QtDocumentController controller;
    ASSERT_NE(nullptr, controller.createEdge(0U, 10.0, 10.0, 110.0, 10.0, Colors::black, 1.0));
    ASSERT_NE(nullptr, controller.createEdge(0U, 40.0, 50.0, 80.0, 50.0, Colors::black, 1.0));

    auto hit = controller.hitTestGeometry(0U, 40.0, 50.0, 1.0, 8.0);
    ASSERT_TRUE(hit.has_value());
    ASSERT_EQ(vn::view::render::GeometryHitType::Vertex, hit->hit.type);
    ASSERT_TRUE(controller.beginGeometryVertexDrag(*hit));
    ASSERT_TRUE(controller.updateGeometryVertexDrag(
            11.0, 10.0, 1.0,
            {.geometryEnabled = true, .gridEnabled = false, .gridSize = 10.0, .gridTolerance = 1.0,
             .screenTolerance = 18.0}));
    ASSERT_TRUE(controller.endGeometryVertexDrag());

    auto welded = geometries(controller.snapshotPages().front());
    ASSERT_EQ(1U, welded.size());
    EXPECT_EQ(3U, welded.front().vertices.size());
    EXPECT_EQ(2U, welded.front().edges.size());
    const auto incidentEdges = std::ranges::count_if(welded.front().edges, [](const auto& edge) {
        return (edge.start.x == 10.0 && edge.start.y == 10.0) || (edge.end.x == 10.0 && edge.end.y == 10.0);
    });
    EXPECT_GE(incidentEdges, 2);

    ASSERT_TRUE(controller.undo());
    auto undone = geometries(controller.snapshotPages().front());
    ASSERT_EQ(2U, undone.size());
    EXPECT_EQ(2U, undone[0].vertices.size());
    EXPECT_EQ(2U, undone[1].vertices.size());

    ASSERT_TRUE(controller.redo());
    auto redone = geometries(controller.snapshotPages().front());
    ASSERT_EQ(1U, redone.size());
    EXPECT_EQ(3U, redone.front().vertices.size());
    EXPECT_EQ(2U, redone.front().edges.size());
}

TEST(VertexNoteQtDocumentControllerShapeTools, rectangleSelectionTargetsGeometryVerticesInsteadOfWholeShape) {
    QtDocumentController controller;
    ASSERT_NE(nullptr,
              controller.createPolyline(0U, {{10.0, 10.0}, {50.0, 10.0}, {90.0, 10.0}}, Colors::black, 1.0));

    ASSERT_TRUE(controller.selectGeometryVerticesInRect(0U, 45.0, 5.0, 10.0, 10.0));
    ASSERT_TRUE(controller.selectedGeometry());
    EXPECT_EQ(vn::view::render::GeometryHitType::Vertex, controller.selectedGeometry()->hit.type);
    ASSERT_EQ(1U, controller.selectedVertexIds().size());
    EXPECT_FALSE(controller.elementSelection());

    ASSERT_TRUE(controller.selectGeometryVerticesInRect(0U, 0.0, 0.0, 100.0, 20.0));
    EXPECT_EQ(3U, controller.selectedVertexIds().size());
}

TEST(VertexNoteQtDocumentControllerShapeTools, rectangleSelectionCanTargetGeometryEdges) {
    QtDocumentController controller;
    ASSERT_NE(nullptr,
              controller.createPolyline(0U, {{10.0, 10.0}, {50.0, 10.0}, {90.0, 10.0}}, Colors::black, 1.0));

    ASSERT_TRUE(controller.selectGeometryEdgesInRect(0U, 5.0, 5.0, 30.0, 10.0));
    ASSERT_TRUE(controller.selectedGeometry());
    EXPECT_EQ(vn::view::render::GeometryHitType::Edge, controller.selectedGeometry()->hit.type);
    EXPECT_TRUE(controller.selectedVertexIds().empty());
    ASSERT_EQ(1U, controller.selectedEdgeIds().size());
    EXPECT_FALSE(controller.elementSelection());

    ASSERT_TRUE(controller.selectGeometryEdgesInRect(0U, 0.0, 0.0, 100.0, 20.0));
    EXPECT_EQ(2U, controller.selectedEdgeIds().size());
}

TEST(VertexNoteQtDocumentControllerShapeTools, rectangleSelectionCanTargetGeometryObject) {
    QtDocumentController controller;
    ASSERT_NE(nullptr,
              controller.createPolyline(0U, {{10.0, 10.0}, {50.0, 10.0}, {90.0, 10.0}}, Colors::black, 1.0));

    ASSERT_TRUE(controller.selectGeometryObjectInRect(0U, 45.0, 5.0, 10.0, 10.0));
    ASSERT_TRUE(controller.selectedGeometry());
    EXPECT_TRUE(controller.selectedVertexIds().empty());
    EXPECT_TRUE(controller.selectedEdgeIds().empty());
    EXPECT_FALSE(controller.elementSelection());

    ASSERT_TRUE(controller.translateSelectedVertices(0.0, 10.0));
    auto translated = geometries(controller.snapshotPages().front());
    ASSERT_EQ(1U, translated.size());
    EXPECT_DOUBLE_EQ(20.0, translated.front().vertices[0].position.y);
    EXPECT_DOUBLE_EQ(20.0, translated.front().vertices[1].position.y);
    EXPECT_DOUBLE_EQ(20.0, translated.front().vertices[2].position.y);
}

TEST(VertexNoteQtDocumentControllerShapeTools, shapeCreationParticipatesInUnifiedUndoRedo) {
    QtDocumentController controller;
    constexpr std::size_t PageIndex = 0U;

    ASSERT_NE(nullptr, controller.createEllipse(PageIndex, 10.0, 10.0, 50.0, 40.0, Colors::black, 1.0, "plain", -1));
    ASSERT_EQ(1U, strokeCount(controller.snapshotPages().front()));
    ASSERT_TRUE(controller.canUndo());

    EXPECT_TRUE(controller.undo());
    EXPECT_EQ(0U, strokeCount(controller.snapshotPages().front()));
    ASSERT_TRUE(controller.canRedo());

    EXPECT_TRUE(controller.redo());
    EXPECT_EQ(1U, strokeCount(controller.snapshotPages().front()));
}

TEST(VertexNoteQtDocumentControllerShapeTools, verticalSpaceMovesElementsWithUndoRedo) {
    QtDocumentController controller;
    constexpr std::size_t PageIndex = 0U;

    ASSERT_TRUE(controller.beginStroke(PageIndex, 20.0, 20.0, 0.5, Colors::black, 1.0, StrokeTool::PEN, false));
    ASSERT_TRUE(controller.updateStroke(120.0, 20.0, 0.5));
    ASSERT_TRUE(controller.finalizeStroke());
    ASSERT_TRUE(controller.beginStroke(PageIndex, 20.0, 100.0, 0.5, Colors::black, 1.0, StrokeTool::PEN, false));
    ASSERT_TRUE(controller.updateStroke(120.0, 100.0, 0.5));
    ASSERT_TRUE(controller.finalizeStroke());

    ASSERT_TRUE(controller.beginVerticalSpace(PageIndex, 50.0, false));
    EXPECT_TRUE(controller.updateVerticalSpace(75.0));
    EXPECT_TRUE(controller.endVerticalSpace());

    auto moved = strokes(controller.snapshotPages().front());
    ASSERT_EQ(2U, moved.size());
    EXPECT_DOUBLE_EQ(20.0, moved[0].points.front().y);
    EXPECT_DOUBLE_EQ(125.0, moved[1].points.front().y);

    ASSERT_TRUE(controller.canUndo());
    EXPECT_TRUE(controller.undo());
    moved = strokes(controller.snapshotPages().front());
    ASSERT_EQ(2U, moved.size());
    EXPECT_DOUBLE_EQ(20.0, moved[0].points.front().y);
    EXPECT_DOUBLE_EQ(100.0, moved[1].points.front().y);

    ASSERT_TRUE(controller.canRedo());
    EXPECT_TRUE(controller.redo());
    moved = strokes(controller.snapshotPages().front());
    ASSERT_EQ(2U, moved.size());
    EXPECT_DOUBLE_EQ(20.0, moved[0].points.front().y);
    EXPECT_DOUBLE_EQ(125.0, moved[1].points.front().y);
}

TEST(VertexNoteQtDocumentControllerShapeTools, scalesSelectionWithUndoRedo) {
    QtDocumentController controller;
    constexpr std::size_t PageIndex = 0U;

    ASSERT_TRUE(controller.beginStroke(PageIndex, 20.0, 20.0, 0.5, Colors::black, 2.0, StrokeTool::PEN, false));
    ASSERT_TRUE(controller.updateStroke(120.0, 20.0, 0.5));
    ASSERT_TRUE(controller.finalizeStroke());

    controller.selectElementAt(PageIndex, 70.0, 20.0, 20.0);
    ASSERT_TRUE(controller.elementSelection());
    ASSERT_TRUE(controller.beginScaleSelection(20.0, 20.0, 120.0, 20.0, true, false, true));
    EXPECT_TRUE(controller.updateScaleSelection(220.0, 20.0));
    EXPECT_TRUE(controller.endScaleSelection());

    auto scaled = strokes(controller.snapshotPages().front());
    ASSERT_EQ(1U, scaled.size());
    ASSERT_EQ(2U, scaled.front().points.size());
    EXPECT_DOUBLE_EQ(20.0, scaled.front().points[0].x);
    EXPECT_DOUBLE_EQ(220.0, scaled.front().points[1].x);
    EXPECT_DOUBLE_EQ(2.0, scaled.front().width);

    ASSERT_TRUE(controller.canUndo());
    EXPECT_TRUE(controller.undo());
    scaled = strokes(controller.snapshotPages().front());
    ASSERT_EQ(1U, scaled.size());
    EXPECT_DOUBLE_EQ(120.0, scaled.front().points[1].x);
    EXPECT_DOUBLE_EQ(2.0, scaled.front().width);

    ASSERT_TRUE(controller.canRedo());
    EXPECT_TRUE(controller.redo());
    scaled = strokes(controller.snapshotPages().front());
    ASSERT_EQ(1U, scaled.size());
    EXPECT_DOUBLE_EQ(220.0, scaled.front().points[1].x);
    EXPECT_DOUBLE_EQ(2.0, scaled.front().width);
}

TEST(VertexNoteQtDocumentControllerShapeTools, pdfTextMarkersCreateHighlighterStrokesWithUndoRedo) {
    QtDocumentController controller;
    constexpr std::size_t PageIndex = 0U;
    const Color markerColor{0xfff06a00U};

    const int inserted = controller.createPdfTextMarkerStrokes(
            PageIndex, {PdfRectangle(10.0, 20.0, 110.0, 32.0), PdfRectangle(15.0, 40.0, 95.0, 50.0)},
            QtPdfTextMarkerKind::Highlight, 60, markerColor);

    ASSERT_EQ(2, inserted);
    ASSERT_EQ(2U, strokeCount(controller.snapshotPages().front()));

    const auto& markerStroke = lastStroke(controller.snapshotPages().front());
    EXPECT_TRUE(markerStroke.highlighter);
    EXPECT_EQ(markerColor, markerStroke.color);
    EXPECT_EQ(60, markerStroke.fill);
    EXPECT_EQ(static_cast<int>(StrokeCapStyle::BUTT), markerStroke.capStyle);
    EXPECT_DOUBLE_EQ(10.0, markerStroke.width);
    ASSERT_EQ(2U, markerStroke.points.size());
    EXPECT_DOUBLE_EQ(15.0, markerStroke.points[0].x);
    EXPECT_DOUBLE_EQ(45.0, markerStroke.points[0].y);
    EXPECT_DOUBLE_EQ(95.0, markerStroke.points[1].x);
    EXPECT_DOUBLE_EQ(45.0, markerStroke.points[1].y);

    ASSERT_TRUE(controller.canUndo());
    EXPECT_TRUE(controller.undo());
    EXPECT_EQ(0U, strokeCount(controller.snapshotPages().front()));

    ASSERT_TRUE(controller.canRedo());
    EXPECT_TRUE(controller.redo());
    EXPECT_EQ(2U, strokeCount(controller.snapshotPages().front()));
}

TEST(VertexNoteQtDocumentControllerShapeTools, loadsAttachedAndExternalPdfDocumentsAsBackgroundPages) {
    for (const bool attachToDocument: {false, true}) {
        SCOPED_TRACE(attachToDocument ? "attached PDF" : "external PDF");
        QtDocumentController controller;
        std::string error;

        ASSERT_TRUE(controller.loadPdfAsDocument(testFile(u8"cjk/测试.pdf"), attachToDocument, &error)) << error;
        ASSERT_EQ(2U, controller.pageCount());

        const auto& pages = controller.snapshotPages();
        ASSERT_EQ(2U, pages.size());
        EXPECT_EQ(PageTypeFormat::Pdf, pages[0].background.backgroundFormat);
        EXPECT_EQ(0U, pages[0].background.pdfPageNumber);
        EXPECT_EQ(PageTypeFormat::Pdf, pages[1].background.backgroundFormat);
        EXPECT_EQ(1U, pages[1].background.pdfPageNumber);
    }
}

TEST(VertexNoteQtDocumentControllerShapeTools, preparesPdfBackgroundRastersThroughQtCache) {
    QtDocumentController controller;
    std::string error;

    ASSERT_TRUE(controller.loadPdfAsDocument(testFile(u8"cjk/测试.pdf"), false, &error)) << error;
    ASSERT_EQ(2U, controller.snapshotPages().size());
    EXPECT_TRUE(controller.snapshotPages()[0].background.rasterContent.empty());

    controller.setPdfCacheOptions(1, 0, 0, true);
    controller.preparePdfRasterCache({0U});
    EXPECT_FALSE(controller.snapshotPages()[0].background.rasterContent.empty());
    EXPECT_TRUE(controller.snapshotPages()[1].background.rasterContent.empty());

    controller.preparePdfRasterCache({1U});
    EXPECT_TRUE(controller.snapshotPages()[0].background.rasterContent.empty());
    EXPECT_FALSE(controller.snapshotPages()[1].background.rasterContent.empty());
}

TEST(VertexNoteQtDocumentControllerShapeTools, appendNewPdfPagesAddsMissingBackgroundPages) {
    QtDocumentController controller;
    std::string error;

    ASSERT_TRUE(controller.loadPdfAsDocument(testFile(u8"cjk/测试.pdf"), false, &error)) << error;
    ASSERT_EQ(2U, controller.pageCount());

    controller.deletePage(1U);
    ASSERT_EQ(1U, controller.pageCount());

    EXPECT_EQ(1, controller.appendNewPdfPages());
    ASSERT_EQ(2U, controller.pageCount());
    const auto& pages = controller.snapshotPages();
    ASSERT_EQ(2U, pages.size());
    EXPECT_EQ(PageTypeFormat::Pdf, pages[1].background.backgroundFormat);
    EXPECT_EQ(1U, pages[1].background.pdfPageNumber);

    EXPECT_EQ(0, controller.appendNewPdfPages());
}

TEST(VertexNoteQtDocumentControllerShapeTools, selectsPdfTextForAttachedAndExternalPdfDocuments) {
    for (const bool attachToDocument: {false, true}) {
        SCOPED_TRACE(attachToDocument ? "attached PDF" : "external PDF");
        QtDocumentController controller;
        std::string error;

        ASSERT_TRUE(controller.loadPdfAsDocument(sourceFile(u8"development/documentation/README-Eraser-and-padded-box.pdf"),
                                                attachToDocument, &error))
                << error;
        ASSERT_TRUE(controller.beginPdfTextSelection(0U, 0.0, 0.0, PdfPageSelectionStyle::Area));
        ASSERT_TRUE(controller.updatePdfTextSelection(10000.0, 10000.0));

        const auto selectedText = controller.finalizePdfTextSelection();
        EXPECT_NE(std::string::npos, selectedText.find("Eraser"));
        ASSERT_TRUE(controller.pdfTextSelection().has_value());
        EXPECT_TRUE(controller.pdfTextSelection()->finalized);
        EXPECT_FALSE(controller.pdfTextSelection()->previewRects.empty());
    }
}

TEST(VertexNoteQtDocumentControllerShapeTools, createsLegacyInstrumentStrokesForQtShell) {
    QtDocumentController controller;
    constexpr std::size_t PageIndex = 0U;

    ASSERT_NE(nullptr,
              controller.createSetsquareStroke(PageIndex, {{32.0, 28.0}, {128.0, 28.0}}, Colors::xopp_royalblue, 2.0,
                                               "plain"));
    ASSERT_NE(nullptr, controller.createCompassStroke(PageIndex,
                                                      {{160.0, 120.0}, {170.0, 110.0}, {180.0, 104.0},
                                                       {192.0, 102.0}, {205.0, 104.0}, {216.0, 110.0}},
                                                      Colors::red, 1.5, "dash"));

    const auto& page = controller.snapshotPages().front();
    EXPECT_EQ(2U, strokeCount(page));
    const auto& compassStroke = lastStroke(page);
    EXPECT_EQ(Colors::red, compassStroke.color);
    EXPECT_DOUBLE_EQ(1.5, compassStroke.width);
    EXPECT_GE(compassStroke.points.size(), 6U);
}

TEST(VertexNoteQtDocumentControllerShapeTools, shapeRecognizerStrokeFinalizesThroughQtPath) {
    QtDocumentController controller;
    ASSERT_TRUE(controller.beginStroke(0U, 20.0, 20.0, 0.5, Colors::black, 2.0, StrokeTool::PEN, false));

    static constexpr std::array<std::pair<double, double>, 8> RectangleLikePoints = {{
            {120.0, 20.0},
            {120.0, 70.0},
            {120.0, 120.0},
            {70.0, 120.0},
            {20.0, 120.0},
            {20.0, 70.0},
            {20.0, 20.0},
            {120.0, 20.0},
    }};

    for (const auto& [x, y]: RectangleLikePoints) {
        ASSERT_TRUE(controller.updateStroke(x, y, 0.5));
    }

    ASSERT_TRUE(controller.finalizeStroke(true, 20.0, false));
    ASSERT_EQ(1U, strokeCount(controller.snapshotPages().front()));
    EXPECT_TRUE(controller.canUndo());
}

TEST(VertexNoteQtDocumentControllerShapeTools, movesSelectionBetweenLayersWithUndoRedo) {
    QtDocumentController controller;
    constexpr std::size_t PageIndex = 0U;

    controller.addLayer(PageIndex);
    controller.selectLayer(PageIndex, 0U);
    ASSERT_NE(nullptr, controller.createLine(PageIndex, 20.0, 20.0, 120.0, 20.0, Colors::black, 2.0));

    controller.selectElementAt(PageIndex, 60.0, 20.0, 16.0);
    ASSERT_TRUE(controller.canMoveSelectionToAdjacentLayer(+1));
    ASSERT_TRUE(controller.moveSelectionToAdjacentLayer(+1));

    auto infos = controller.layerInfos(PageIndex);
    ASSERT_EQ(2U, infos.size());
    EXPECT_EQ(0U, infos[0].elementCount);
    EXPECT_EQ(1U, infos[1].elementCount);

    ASSERT_TRUE(controller.undo());
    infos = controller.layerInfos(PageIndex);
    EXPECT_EQ(1U, infos[0].elementCount);
    EXPECT_EQ(0U, infos[1].elementCount);

    ASSERT_TRUE(controller.redo());
    infos = controller.layerInfos(PageIndex);
    EXPECT_EQ(0U, infos[0].elementCount);
    EXPECT_EQ(1U, infos[1].elementCount);
}

TEST(VertexNoteQtDocumentControllerShapeTools, resizesPageWithUndoRedo) {
    QtDocumentController controller;
    constexpr std::size_t PageIndex = 0U;

    const auto& before = controller.snapshotPages().front();
    EXPECT_TRUE(controller.canResizePage(PageIndex));
    ASSERT_TRUE(controller.resizePage(PageIndex, 842.0, 595.0));
    const auto& resized = controller.snapshotPages().front();
    EXPECT_DOUBLE_EQ(842.0, resized.width);
    EXPECT_DOUBLE_EQ(595.0, resized.height);

    ASSERT_TRUE(controller.undo());
    const auto& undone = controller.snapshotPages().front();
    EXPECT_DOUBLE_EQ(before.width, undone.width);
    EXPECT_DOUBLE_EQ(before.height, undone.height);

    ASSERT_TRUE(controller.redo());
    const auto& redone = controller.snapshotPages().front();
    EXPECT_DOUBLE_EQ(842.0, redone.width);
    EXPECT_DOUBLE_EQ(595.0, redone.height);
}
