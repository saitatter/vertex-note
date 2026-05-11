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
    LaserPointerPen,
    LaserPointerHighlighter,
    Setsquare,
    Compass,
    Eraser,
    Highlighter,
    Text,
    PdfTextLinear,
    PdfTextRect,
    SelectRect,
    SelectRegion,
    SelectMultiLayerRect,
    SelectMultiLayerRegion,
    SelectObject,
    VerticalSpace,
    DrawLine,
    DrawRectangle,
    DrawCircle,
    DrawEllipse,
    DrawArrow,
    DrawDoubleArrow,
    DrawCoordinateSystem,
    DrawSpline,
    ShapeRecognizer,
    DrawArc,
    DrawPolyline,
    DrawConstructionLine,
    DrawConstructionCircle,
};
enum class QtEraserMode { Standard, Whiteout, DeleteStroke, Segment };

struct QtToolState {
    QtToolType activeTool = QtToolType::Pen;
    Color penColor{0x3333ccffU};
    Color highlighterColor{0xffff0080U};
    double penWidth = 1.41;
    double highlighterWidth = 8.50;
    double eraserWidth = 8.50;
    bool pressureSensitive = true;
    QtEraserMode eraserMode = QtEraserMode::Standard;
    std::string penLineStyle = "plain";
    bool fillEnabled = false;
    int fillOpacity = 128;
    bool highlighterFillEnabled = false;
    int pdfTextMarkerOpacity = 60;
    std::string fontName = "Sans";
    double fontSize = 12.0;

    [[nodiscard]] auto activeToolName() const -> std::string {
        switch (activeTool) {
            case QtToolType::Hand: return "Hand";
            case QtToolType::Pen: return "Pen";
            case QtToolType::LaserPointerPen: return "Laser Pointer Pen";
            case QtToolType::LaserPointerHighlighter: return "Laser Pointer Highlighter";
            case QtToolType::Setsquare: return "Setsquare";
            case QtToolType::Compass: return "Compass";
            case QtToolType::Eraser: return "Eraser";
            case QtToolType::Highlighter: return "Highlighter";
            case QtToolType::Text: return "Text";
            case QtToolType::PdfTextLinear: return "Select Linear PDF Text";
            case QtToolType::PdfTextRect: return "Select Area PDF Text";
            case QtToolType::SelectRect: return "Select Rectangle";
            case QtToolType::SelectRegion: return "Select Region";
            case QtToolType::SelectMultiLayerRect: return "Select Multi-Layer Rect";
            case QtToolType::SelectMultiLayerRegion: return "Select Multi-Layer Region";
            case QtToolType::SelectObject: return "Select Object";
            case QtToolType::VerticalSpace: return "Vertical Space";
            case QtToolType::DrawLine: return "Line";
            case QtToolType::DrawRectangle: return "Rectangle";
            case QtToolType::DrawCircle: return "Circle";
            case QtToolType::DrawEllipse: return "Ellipse";
            case QtToolType::DrawArrow: return "Arrow";
            case QtToolType::DrawDoubleArrow: return "Double Arrow";
            case QtToolType::DrawCoordinateSystem: return "Coordinate System";
            case QtToolType::DrawSpline: return "Spline";
            case QtToolType::ShapeRecognizer: return "Shape Recognizer";
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
            case QtToolType::DrawEllipse:
            case QtToolType::DrawArrow:
            case QtToolType::DrawDoubleArrow:
            case QtToolType::DrawCoordinateSystem:
            case QtToolType::DrawSpline:
            case QtToolType::ShapeRecognizer:
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
