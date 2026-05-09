# Qt Migration Bootstrap

This document tracks the executable slices of the Qt migration.

## Current Scope

### Platform-neutral abstractions (shared core)

- `src/core/ui/common/` — shell interfaces: `IAppShell`, `ICommandHost`, `ICanvasHost`,
  `IClipboardService`, `IDialogService`, `IRecentFilesService`,
  `IUpdatePresentationService`, `IPluginUiBridge`.
- `src/core/ui/input/` — Qt-independent input event shape (`PointerEvent`,
  `KeyboardEvent`, `TouchEvent`) and `IInputEventSink`.
- `src/core/view/render/` — backend-neutral render contracts:
  - Data models: `PageBackgroundRenderModel`, `StrokeRenderModel`,
    `TextRenderModel`, `ImageRenderModel`, `GeometryRenderModel`.
  - Renderer interfaces: `BackgroundRenderer`, `StrokeRenderer`,
    `TextRenderer`, `ImageRenderer`, `GeometryRenderer`, `OverlayRenderer`.
  - Model factories that convert core `Document` objects → render models.
  - `GeometryHitTest` for vertex/edge intersection detection.
  - `PageRenderSnapshotFactory` builds `PageRenderSnapshot` vectors from
    a `Document`.
  - `CairoRenderContext` and `QtPainterRenderContext` backend wrappers.
  - GTK page-type previews now consume the same background render model
    seam through a Cairo preview renderer.

### Qt shell (`src/qt/`)

`ENABLE_QT_SHELL` adds an optional Qt Widgets bootstrap target.

- **App bootstrap**: `QApplication`, `QMainWindow`, full `IAppShell`
  implementation (`QtAppShell`).
- **Command host**: `QtCommandHost` — ~20 bootstrap commands wired to
  `QAction`/`QMenu`.
- **Services**: `QtClipboardService`, `QtDialogService`,
  `QtRecentFilesService` (working), `QtUpdatePresentationService` and
  `QtPluginUiBridge` (stubs).
- **Canvas**: `QtCanvas` — viewport rendering with translated mouse, pen,
  wheel, key, and touch input; pan / zoom / fit-page interactions.
- **Input adapter**: `QtInputAdapter` translates Qt events → neutral
  `vn::ui::input` events.
- **Document controller**: `QtDocumentController` — loads real `.xopp`,
  `.xoj`, `.xopt`, `.pdf` documents through the shared core `Document`
  model; page snapshot caching; `.vnsession` open/save flow.
- **Preview renderers**: `QtPreviewBackgroundRenderer`,
  `QtPreviewStrokeRenderer`, `QtPreviewTextRenderer`,
  `QtPreviewImageRenderer`, `QtPreviewGeometryRenderer`.
- **Geometry editing** (VertexNote-specific):
  - Hover and select geometry vertices/edges.
  - Drag vertices with grid and geometry snapping.
  - Insert vertex on edge, delete selected geometry.
  - Local undo/redo history for geometry edits.
- **Tool system** (`QtToolState`):
  - Tool types: Hand, Pen, Eraser, Highlighter, Text, SelectRect.
  - Tool commands with keyboard shortcuts (H, P, E, G) and toolbar buttons.
  - Per-tool state: color, width, pressure sensitivity.
- **Stroke drawing**:
  - Pressure-sensitive input from mouse and tablet (Wacom) devices.
  - Pen and Highlighter tools create `Stroke` elements on the active layer.
  - Active stroke rendered live through shared `StrokeRenderer` pipeline.
  - Minimum distance filtering between points.
- **Whole-stroke eraser**:
  - Eraser tool deletes strokes that intersect the eraser path.
  - Uses `Stroke::intersects()` for precise hit testing.
  - All strokes erased in one drag are batched into a single undo entry.
- **Element selection** (SelectRect tool):
  - Click-to-select with distance-based hit testing.
  - Rubber-band rectangle selection for multi-select.
  - Shift+click additive/toggle selection.
  - Drag selected elements to move them.
  - Selection overlay with dashed bounding box and corner handles.
- **Shared page rendering** (`PageContentRenderer`):
  - Dispatches all drawable types through abstract renderer interfaces.
  - Both committed strokes and in-progress active strokes render through
    the same `StrokeRenderer` pipeline.
- **Unified undo/redo**:
  - Supports geometry edits, stroke creation, stroke erasure, and
    element moves.
  - Stroke undo removes from layer, redo re-inserts at original position.
  - Erase undo re-inserts all removed strokes at original z-order.
  - Move undo/redo applies inverse/forward delta to element positions.

## Build

The Qt shell is intentionally opt-in.

```powershell
pacman -S mingw-w64-x86_64-qt6-base
powershell -ExecutionPolicy Bypass -File scripts/mingw64-dev.ps1 build-qt
```

Current binary name:

- `vertex-note-qt-shell`

Run it with:

```powershell
powershell -ExecutionPolicy Bypass -File scripts/mingw64-dev.ps1 run-qt
```

## Intentional Limits

- The shipping GTK application remains the primary shell for now.
- The Qt target opens real core documents and supports drawing and erasing,
  but does not host the GTK `Control` class (too tightly coupled). Instead,
  tool management and undo/redo are implemented Qt-natively using the same
  core model objects (`Stroke`, `Layer`, `Document`).
- Preview renderers cover pressure-sensitive strokes, all background
  patterns (ruled, graph, dotted, staves, etc.), multi-line text with
  correct Pango-equivalent sizing, and images.
- No segment eraser — only whole-stroke deletion.
- No layer management, sidebar, or tool palette in the Qt shell yet.

## Completed Slices

### Phase 2 — Production rendering & selection ✓

1. ✓ Pressure-sensitive variable-width stroke rendering with cap styles.
2. ✓ All background patterns (ruled, graph, dotted, staves, iso, etc.)
   with page colour support.
3. ✓ Full `TextRenderer` with `QTextDocument` multi-line layout and
   Pango-equivalent pixel sizing.
4. ✓ Element selection system: click-to-select, rubber-band rectangle
   multi-select, drag-to-move with undo/redo, selection overlay.
5. ✓ Shared `PageContentRenderer` routes drawables through abstract
   renderer interfaces; active stroke preview uses the same pipeline.

### Phase 3 — Feature parity & polish (partial) ✓

6. ✓ Layer panel dock widget with visibility toggles, add/remove/reorder.
7. ✓ Page background dialog with colour picker and pattern dropdown.
9. ✓ Export: PDF via `QPdfWriter`, PNG via `QImage` at configurable scale.
10. ✓ Page sidebar dock with rendered thumbnails and click-to-scroll navigation.

## Next Slices

### Phase 3 — Feature parity & polish

6. ✅ Layer panel UI and layer operations.
7. ✅ Page templates, background colour/pattern dialogs.
8. PDF annotation overlay (pen/text on PDF pages).
9. ✅ Export (PDF, PNG) via QPdfWriter / QImage.
10. ✅ Sidebar (page thumbnails with rendered previews, click-to-scroll).
11. Plugin system, fullscreen/presentation modes.
12. Toolbar with tool palette, colour picker, font selector, floating
    toolbox.
