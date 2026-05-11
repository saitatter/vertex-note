#include "ButtonConfig.h"

ButtonConfig::ButtonConfig(ToolType action, Color color, ToolSize size, DrawingType drawingType, EraserType eraserMode,
                           StrokeType strokeType):
        action(action),
        color(color),
        size(size),
        eraserMode(eraserMode),
        drawingType(drawingType),
        strokeType(strokeType),
        disableDrawing(false) {}

ButtonConfig::~ButtonConfig() = default;

auto ButtonConfig::getDisableDrawing() const -> bool { return this->disableDrawing; }

auto ButtonConfig::getDrawingType() const -> DrawingType { return this->drawingType; }

auto ButtonConfig::getAction() const -> ToolType { return this->action; }

void ButtonConfig::initButton(ToolHandler* toolHandler, Button button) const {
    (void) toolHandler;
    (void) button;
}

void ButtonConfig::applyConfigToToolbarTool(ToolHandler* toolHandler) const {
    (void) toolHandler;
}

auto ButtonConfig::applyNoChangeSettings(ToolHandler* toolHandler, Button button) const -> bool {
    (void) toolHandler;
    (void) button;
    return false;
}
