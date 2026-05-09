/*
 * VertexNote
 *
 * Qt shell tool state tracking.
 */

#pragma once

#include <string>

#include "util/Color.h"

enum class QtToolType {
    Hand,
    Pen,
    Eraser,
    Highlighter,
    Text,
    SelectRect,
    DrawLine,
    DrawRectangle,
    DrawCircle,
    DrawArc,
    DrawPolyline,
    DrawConstructionLine,
    DrawConstructionCircle,
};
enum class QtEraserMode { WholeStroke, Segment };

struct QtToolState {
    QtToolType activeTool = QtToolType::Hand;
    Color penColor{0x3333ccffU};
    Color highlighterColor{0xffff0080U};
    double penWidth = 1.41;
    double highlighterWidth = 8.50;
    double eraserWidth = 8.50;
    bool pressureSensitive = true;
    QtEraserMode eraserMode = QtEraserMode::WholeStroke;

    [[nodiscard]] auto activeToolName() const -> std::string {
        switch (activeTool) {
            case QtToolType::Hand: return "Hand";
            case QtToolType::Pen: return "Pen";
            case QtToolType::Eraser: return "Eraser";
            case QtToolType::Highlighter: return "Highlighter";
            case QtToolType::Text: return "Text";
            case QtToolType::SelectRect: return "Select";
            case QtToolType::DrawLine: return "Line";
            case QtToolType::DrawRectangle: return "Rectangle";
            case QtToolType::DrawCircle: return "Circle";
            case QtToolType::DrawArc: return "Arc";
            case QtToolType::DrawPolyline: return "Polyline";
            case QtToolType::DrawConstructionLine: return "Construction Line";
            case QtToolType::DrawConstructionCircle: return "Construction Circle";
        }
        return "Unknown";
    }

    [[nodiscard]] auto isShapeDrawingTool() const -> bool {
        switch (activeTool) {
            case QtToolType::DrawLine:
            case QtToolType::DrawRectangle:
            case QtToolType::DrawCircle:
            case QtToolType::DrawArc:
            case QtToolType::DrawPolyline:
            case QtToolType::DrawConstructionLine:
            case QtToolType::DrawConstructionCircle:
                return true;
            default:
                return false;
        }
    }
};
