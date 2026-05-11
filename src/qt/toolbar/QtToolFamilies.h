/*
 * VertexNote
 *
 * Qt tool family command catalogs.
 */

#pragma once

#include <span>
#include <string_view>

#include "QtToolState.h"

class QAction;
class QtCommandHost;

struct ToolActionSpec {
    std::string_view commandId;
    QtToolType tool;
};

[[nodiscard]] auto selectionToolSpecs() -> std::span<const ToolActionSpec>;
[[nodiscard]] auto strokeDrawingToolSpecs() -> std::span<const ToolActionSpec>;
[[nodiscard]] auto vertexDrawingToolSpecs() -> std::span<const ToolActionSpec>;
[[nodiscard]] auto laserToolSpecs() -> std::span<const ToolActionSpec>;
[[nodiscard]] auto pdfToolSpecs() -> std::span<const ToolActionSpec>;
[[nodiscard]] auto findActionForTool(QtCommandHost* host, std::span<const ToolActionSpec> specs,
                                     QtToolType activeTool) -> QAction*;
