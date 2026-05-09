/*
 * VertexNote
 *
 * Qt shell tool state tracking.
 */

#pragma once

#include <string>

#include "util/Color.h"

enum class QtToolType { Hand, Pen, Eraser, Highlighter, Text, SelectRect };

struct QtToolState {
    QtToolType activeTool = QtToolType::Hand;
    Color penColor{0x3333ccffU};
    Color highlighterColor{0xffff0080U};
    double penWidth = 1.41;
    double highlighterWidth = 8.50;
    double eraserWidth = 8.50;
    bool pressureSensitive = true;

    [[nodiscard]] auto activeToolName() const -> std::string {
        switch (activeTool) {
            case QtToolType::Hand: return "Hand";
            case QtToolType::Pen: return "Pen";
            case QtToolType::Eraser: return "Eraser";
            case QtToolType::Highlighter: return "Highlighter";
            case QtToolType::Text: return "Text";
            case QtToolType::SelectRect: return "Select";
        }
        return "Unknown";
    }
};
