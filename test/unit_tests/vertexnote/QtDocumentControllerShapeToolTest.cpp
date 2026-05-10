#include <array>
#include <stdexcept>

#include <gtest/gtest.h>

#include "qt/QtDocumentController.h"
#include "view/render/Renderers.h"

namespace {

auto strokeCount(const vn::view::render::PageRenderSnapshot& snapshot) -> std::size_t {
    std::size_t count = 0;
    for (const auto& drawable: snapshot.drawables) {
        if (std::holds_alternative<vn::view::render::StrokeRenderModel>(drawable)) {
            ++count;
        }
    }
    return count;
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
