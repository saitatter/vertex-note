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
    // The legacy GTK ToolHandler was removed with the GTK shell. Qt applies
    // button/tool mappings through its pointer input profile instead, while
    // this class remains as a settings-file compatibility container.
    (void) toolHandler;
    (void) button;
}

void ButtonConfig::applyConfigToToolbarTool(ToolHandler* toolHandler) const {
    // See initButton(): toolbar tool state is owned by the Qt toolbar/profile
    // layer now, not by the removed ToolHandler bridge.
    (void) toolHandler;
}

auto ButtonConfig::applyNoChangeSettings(ToolHandler* toolHandler, Button button) const -> bool {
    // See initButton(): no active caller should depend on the removed GTK
    // ToolHandler semantics.
    (void) toolHandler;
    (void) button;
    return false;
}
