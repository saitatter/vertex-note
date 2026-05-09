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
- The Qt target opens real core documents and supports geometry editing,
  but does not yet host `Control`, drawing tools, or full editing workflow
  parity.
- Preview renderers are simplified (no pressure curves, no arc/bezier
  geometry, no grid/ruled/dotted background patterns).
- Undo/redo is local to geometry edits; the core `UndoRedoHandler` is not
  wired yet.
- No layer management, selection system, sidebar, or tool palette in the
  Qt shell.

## Next Slices

### Phase 1 — Core Control wiring (enable basic editing)

1. Instantiate `Control` in `QtAppShell` and map `ICommandHost` commands
   to `Control` methods (`selectTool`, `save`, `openFile`, `newFile`, …).
2. Create a `ToolInputBridge` that routes neutral `IInputEventSink` events
   to `Control`'s tool handlers (pen, eraser, text, shape, selection).
3. Wire `UndoRedoHandler` so all edits (not only geometry) flow through
   the shared undo stack.
4. Implement basic tool switching through the command host (pen,
   highlighter, eraser, text, selection).

### Phase 2 — Production rendering & selection

5. Extend `StrokeRenderer` for pressure-sensitive curves, cap styles, and
   dash patterns.
6. Extend `BackgroundRenderer` for grid/ruled/dotted/graph patterns.
7. Implement full `TextRenderer` with Pango/Qt layout parity.
8. Wire the element selection system (`EditSelection`) and overlay
   rendering (selection handles, resize grips).
9. Route interactive notebook painting through the render contracts so GTK
   and Qt share page rendering logic.

### Phase 3 — Feature parity & polish

10. Layer panel UI and layer operations.
11. Page templates, background colour/pattern dialogs.
12. PDF annotation overlay (pen/text on PDF pages).
13. Export (PDF, PNG), print.
14. Sidebar (page thumbnails, search).
15. Plugin system, fullscreen/presentation modes.
16. Toolbar with tool palette, colour picker, font selector, floating
    toolbox.
