/*
 * VertexNote
 *
 * Qt tool family command catalogs.
 */

#include "QtToolFamilies.h"

#include "QtCommandHost.h"

#include <array>

#include <QAction>

namespace {

constexpr std::array<ToolActionSpec, 5> SELECTION_TOOL_SPECS = {{
        {"tool.select", QtToolType::SelectRect},
        {"tool.select-region", QtToolType::SelectRegion},
        {"tool.select-multilayer-rect", QtToolType::SelectMultiLayerRect},
        {"tool.select-multilayer-region", QtToolType::SelectMultiLayerRegion},
        {"tool.select-object", QtToolType::SelectObject},
}};

constexpr std::array<ToolActionSpec, 8> STROKE_DRAWING_TOOL_SPECS = {{
        {"tool.draw-line", QtToolType::DrawLine},
        {"tool.draw-rectangle", QtToolType::DrawRectangle},
        {"tool.draw-ellipse", QtToolType::DrawEllipse},
        {"tool.draw-arrow", QtToolType::DrawArrow},
        {"tool.draw-double-arrow", QtToolType::DrawDoubleArrow},
        {"tool.draw-coordinate-system", QtToolType::DrawCoordinateSystem},
        {"tool.draw-spline", QtToolType::DrawSpline},
        {"tool.draw-shape-recognizer", QtToolType::ShapeRecognizer},
}};

constexpr std::array<ToolActionSpec, 5> VERTEX_DRAWING_TOOL_SPECS = {{
        {"tool.draw-circle", QtToolType::DrawCircle},
        {"tool.draw-arc", QtToolType::DrawArc},
        {"tool.draw-polyline", QtToolType::DrawPolyline},
        {"tool.draw-construction-line", QtToolType::DrawConstructionLine},
        {"tool.draw-construction-circle", QtToolType::DrawConstructionCircle},
}};

constexpr std::array<ToolActionSpec, 2> LASER_TOOL_SPECS = {{
        {"tool.laser-pointer-pen", QtToolType::LaserPointerPen},
        {"tool.laser-pointer-highlighter", QtToolType::LaserPointerHighlighter},
}};

constexpr std::array<ToolActionSpec, 2> PDF_TOOL_SPECS = {{
        {"tool.select-pdf-text-linear", QtToolType::PdfTextLinear},
        {"tool.select-pdf-text-rect", QtToolType::PdfTextRect},
}};

}  // namespace

auto selectionToolSpecs() -> std::span<const ToolActionSpec> { return SELECTION_TOOL_SPECS; }

auto strokeDrawingToolSpecs() -> std::span<const ToolActionSpec> { return STROKE_DRAWING_TOOL_SPECS; }

auto vertexDrawingToolSpecs() -> std::span<const ToolActionSpec> { return VERTEX_DRAWING_TOOL_SPECS; }

auto laserToolSpecs() -> std::span<const ToolActionSpec> { return LASER_TOOL_SPECS; }

auto pdfToolSpecs() -> std::span<const ToolActionSpec> { return PDF_TOOL_SPECS; }

auto findActionForTool(QtCommandHost* host, std::span<const ToolActionSpec> specs, QtToolType activeTool) -> QAction* {
    if (!host || specs.empty()) {
        return nullptr;
    }

    for (const auto& spec: specs) {
        if (spec.tool == activeTool) {
            if (auto* action = host->actionForCommand(spec.commandId)) {
                return action;
            }
        }
    }
    return host->actionForCommand(specs.front().commandId);
}
