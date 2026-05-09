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
  - Active stroke rendered live during drawing via `QPainterPath`.
  - Minimum distance filtering between points.
- **Whole-stroke eraser**:
  - Eraser tool deletes strokes that intersect the eraser path.
  - Uses `Stroke::intersects()` for precise hit testing.
  - All strokes erased in one drag are batched into a single undo entry.
- **Unified undo/redo**:
  - Supports geometry edits, stroke creation, and stroke erasure.
  - Stroke undo removes from layer, redo re-inserts at original position.
  - Erase undo re-inserts all removed strokes at original z-order.
  - Erase redo re-removes by saved element pointers.

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
- Preview renderers are simplified (no pressure curves, no arc/bezier
  geometry, no grid/ruled/dotted background patterns).
- No segment eraser — only whole-stroke deletion.
- No layer management, selection system, sidebar, or tool palette in the
  Qt shell.

## Next Slices

### Phase 2 — Production rendering & selection

1. Extend `StrokeRenderer` for pressure-sensitive curves, cap styles, and
   dash patterns.
2. Extend `BackgroundRenderer` for grid/ruled/dotted/graph patterns.
3. Implement full `TextRenderer` with Pango/Qt layout parity.
4. Wire the element selection system (`EditSelection`) and overlay
   rendering (selection handles, resize grips).
5. Route interactive notebook painting through the render contracts so GTK
   and Qt share page rendering logic.

### Phase 3 — Feature parity & polish

6. Layer panel UI and layer operations.
7. Page templates, background colour/pattern dialogs.
8. PDF annotation overlay (pen/text on PDF pages).
9. Export (PDF, PNG), print.
10. Sidebar (page thumbnails, search).
11. Plugin system, fullscreen/presentation modes.
12. Toolbar with tool palette, colour picker, font selector, floating
    toolbox.
