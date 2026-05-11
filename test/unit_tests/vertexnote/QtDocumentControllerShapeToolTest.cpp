#include <array>
#include <filesystem>
#include <stdexcept>
#include <string_view>

#include <config-test.h>
#include <gtest/gtest.h>

#include "qt/QtDocumentController.h"
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
