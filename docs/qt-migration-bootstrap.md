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
- **Command host**: `QtCommandHost` — ~80 bootstrap commands wired to
  `QAction`/`QMenu` across 14 phases.
- **Services**: `QtClipboardService`, `QtDialogService`,
  `QtRecentFilesService` (working), `QtUpdatePresentationService` and
  `QtPluginUiBridge` (real callbacks, menu/toolbar integration).
- **Canvas**: `QtCanvas` — viewport rendering with translated mouse, pen,
  wheel, key, and touch input; pan / zoom / fit-page / fit-width interactions.
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
  - Constraint application (Coincident, Horizontal, Vertical, FixedLength,
    Radius, Parallel, Perpendicular) with validation and value editing.
- **Tool system** (`QtToolState`):
  - Tool types: Hand, Pen, Eraser, Highlighter, Text, SelectRect,
    DrawLine, DrawRectangle, DrawCircle, DrawEllipse, DrawArrow,
    DrawDoubleArrow, DrawCoordinateSystem, DrawSpline, DrawArc,
    Setsquare, Compass,
    DrawPolyline, DrawConstructionLine, DrawConstructionCircle.
  - Tool commands with keyboard shortcuts and toolbar buttons.
  - Per-tool state: color, width, pressure sensitivity, line style, fill, font.
- **Stroke drawing**:
  - Pressure-sensitive input from mouse and tablet (Wacom) devices.
  - Pen and Highlighter tools create `Stroke` elements on the active layer.
  - Active stroke rendered live through shared `StrokeRenderer` pipeline.
  - Pen line style (solid/dash/dashdot/dot) and stroke fill support.
- **Eraser**:
  - Whole-stroke eraser deletes strokes that intersect the eraser path.
  - Segment eraser splits strokes at intersection points.
  - Both modes batch operations into single undo entries.
- **Element operations**:
  - Click-to-select with distance-based hit testing.
  - Rubber-band rectangle selection for multi-select.
  - Shift+click additive/toggle selection.
  - Drag selected elements to move them.
  - Copy/cut/paste with in-memory clipboard.
  - Delete selection, select all, z-order (bring to front/back/forward/backward).
- **Shape drawing**:
  - 12 click-based shape tools with live dashed-line preview.
  - 2-click tools (line, rectangle, circle, construction geometry).
  - 2-click legacy stroke tools (ellipse, arrow, double arrow,
    coordinate system).
  - 3-click arc tool, multi-click polyline and spline
    (double-click to finalize).
- **Toolbar parity infrastructure**:
  - `QtToolbarLayoutEngine` parses `toolbar.ini.in` profiles.
  - Top, bottom, left, right, and auxiliary floating Qt toolbars are now
    built from toolbar tokens instead of fixed hardcoded rows.
  - Toolbar profile selection is available in Qt Preferences.
- **Page & layer management**:
  - Add, duplicate, delete, reorder pages.
  - Layer panel with visibility toggles, add/remove/reorder/rename/copy/merge.
  - Show/hide all layers, add layer above/below current.
- **Navigation**:
  - Page navigation: first/last/next/previous/goto with keyboard shortcuts.
  - Navigation history with back/forward (Alt+Left/Right).
  - Annotated page navigation (next/previous page with content).
  - Layer navigation: next/previous/top layer switching.
- **Document workflow**:
  - Save to `.xopp`, PDF export, PNG export, print.
  - Image insertion with undo support.
  - Text search across all text elements.
  - Settings dialog with tabbed preferences.
  - Font selection dialog.
- **Shared page rendering** (`PageContentRenderer`):
  - Dispatches all drawable types through abstract renderer interfaces.
  - Both committed strokes and in-progress active strokes render through
    the same `StrokeRenderer` pipeline.
- **Unified undo/redo**:
  - Supports geometry edits, stroke creation, stroke erasure, element
    moves, element deletion, and shape creation.
  - Full undo/redo for all document-modifying operations.

## Build

The Qt shell is intentionally opt-in.

```powershell
pacman -S mingw-w64-x86_64-qt6-base mingw-w64-x86_64-qt6-printsupport
powershell -ExecutionPolicy Bypass -File scripts/mingw64-dev.ps1 build-qt
```

Current binary name:

- `vertex-note-qt-shell`

Run it with:

```powershell
powershell -ExecutionPolicy Bypass -File scripts/mingw64-dev.ps1 run-qt
```

## Intentional Limits

- The GTK shell remains the primary shell for plugin execution (Lua runtime).
- The Qt shell opens real core documents and supports the full drawing, editing,
  and document management workflow, but does not host the GTK `Control` class.
  Tool management, undo/redo, and all editing operations are implemented
  Qt-natively using the same core model objects (`Stroke`, `Layer`, `Document`).
- Plugin system has UI bridge for menu/toolbar actions but no Qt-native
  Lua runtime; plugins requiring Lua execution need the GTK shell.
- The following toolbar-visible features are still partial or placeholder in
  the Qt shell and remain on the parity backlog:
  `AUDIO_*`.

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

### Phase 3 — Feature parity & polish ✓

6. ✓ Layer panel dock widget with visibility toggles, add/remove/reorder.
7. ✓ Page background dialog with colour picker and pattern dropdown.
8. ✓ PDF/Image raster preview via `PdfPage::renderPreviewRaster`.
9. ✓ Export: PDF via `QPdfWriter`, PNG via `QImage` at configurable scale.
10. ✓ Page sidebar dock with rendered thumbnails and click-to-scroll navigation.
11. ✓ Fullscreen (F11) and presentation mode (F5) with toolbar/sidebar toggle.
12. ✓ Toolbar palette with colour picker, pen width spinner, pressure toggle.

### Phase 4 — Advanced editing & plugin bridge ✓

13. ✓ Segment eraser — splits strokes at eraser intersection points, with full undo/redo.
14. ✓ Plugin UI bridge — real callbacks, separate menu/toolbar registration, true action removal.
15. ✓ Inline text editing — click to create/edit Text elements with overlay editor, undo/redo.

### Phase 5 — Document workflow & remaining features ✓

16. ✓ Page management — add page after, duplicate page (clones all elements), delete page (with guard).
17. ✓ Save document — `SaveHandler` serialisation to `.xopp` XML, with save-to-existing-path shortcut (Ctrl+S).
18. ✓ Image insertion — insert image from file into page layer, with undo support.
19. ✓ Text search — case-insensitive substring search across all Text elements, scroll-to-first-match.
20. ✓ Print — `QPrinter` + `QPrintDialog` with per-page sizing and high-resolution DPI scaling.
21. ✓ Settings dialog — tabbed preferences for tool defaults, page dimensions, eraser mode, snap toggles.
22. ✓ Text tool command — `tool.text` (T) wired in toolbar and menu alongside other tools.

### Phase 6 — Geometry constraints & shape drawing tools ✓

23. ✓ Constraint engine integration — `applyConstraint(ConstraintKind)` in `QtDocumentController` with
    full validation for all solver-supported kinds (Coincident, Horizontal, Vertical, FixedLength, Radius,
    Parallel, Perpendicular). Edge ID tracking added alongside vertex selection.
24. ✓ Constraint management — `deleteSelectedConstraints()`, `selectedFixedLengthConstraint()`,
    `updateFixedLengthConstraint(double)` for removing/editing constraints on selection.
25. ✓ Shape creation methods — `createLine`, `createRectangle`, `createCircle`, `createArc`,
    `createPolyline`, `createConstructionLine`, `createConstructionCircle` in `QtDocumentController`,
    each with undo support via `QtStrokeHistoryEntry`.
26. ✓ Shape tool interactions — 7 new `QtToolType` values, `QtCanvas` mouse dispatch with
    2-click tools (Line, Rectangle, Circle, ConstructionLine, ConstructionCircle), 3-click Arc,
    multi-click Polyline (double-click to finalize). Dashed-line preview with vertex handles.
27. ✓ AppShell commands — 7 shape tool commands (`tool.draw-*`) and 9 constraint commands
    (`constraint.*`) wired in menus. `updateToolCommandStates` extended for all new tools.
28. ✓ Constraint value editing — `editFixedLengthConstraint()` with `QInputDialog` for setting
    FixedLength/Radius constraint values.

### Phase 6.5 — Qt toolbar parity & legacy drawing tools ✓

29. ✓ Toolbar profile engine — `QtToolbarLayoutEngine` loads toolbar
    profiles from `toolbar.ini.in`, and `QtAppShell` rebuilds top/bottom/left/
    right/floating Qt toolbars from toolbar tokens.
30. ✓ Toolbar profile selection — Qt Preferences exposes toolbar profile
    switching and rebuilds the shell immediately.
31. ✓ Real Qt actions for previously inert toolbars:
    rotation snap, touch drawing, default preset, fill opacity.
32. ✓ Legacy stroke-based drawing tools in the Qt shell:
    ellipse, arrow, double arrow, coordinate system, spline.
33. ✓ View layout controls now affect the Qt canvas:
    paired pages, vertical/horizontal flow, RTL/LTR, BTT/TTB.
34. ✓ Laser pointer tools in the Qt shell:
    pen/highlighter variants, toolbar family button, temporary fading overlay.
35. ✓ Shape recognizer in the Qt shell:
    recognizer drawing tool, shared core recognition pass, recognizer settings.
36. ✓ PDF text selection tools in the Qt shell:
    linear and rectangle modes, toolbar family button, highlight overlay, clipboard copy.
37. ✓ Setsquare and Compass tools in the Qt shell:
    visible overlay instruments, guided stroke creation, Alt+wheel resize, Shift+wheel rotation.

### Phase 7 — Clipboard & element operations ✓

29. ✓ Delete selection — `deleteSelectedElements()` removes selected elements from layer with
    full undo/redo via `QtDeleteHistoryEntry`.
30. ✓ Select all — `selectAllElements(pageIndex)` selects all visible elements on a page.
31. ✓ Copy/Cut/Paste — `copySelectedElements()` clones via `Element::clone()`,
    `cutSelectedElements()` copies then deletes, `pasteElements()` inserts clones with offset.
    In-memory clipboard in `QtAppShell::elementClipboard`, supports repeated paste.

### Phase 8 — Page navigation ✓

32. ✓ Current page tracking — `QtCanvas::currentPageIndex()` determines the most visible page
    from viewport center position.
33. ✓ Scroll to page — `QtCanvas::scrollToPage(pageIndex)` scrolls viewport to target page.
34. ✓ Navigation commands — First (Home), Last (End), Next (PgDown), Previous (PgUp),
    Go to Page dialog (Ctrl+G) with `QInputDialog::getInt`.

### Phase 9 — Layer operations ✓

35. ✓ Copy layer — `copyLayer()` deep-clones via `Layer::clone()`, inserts above current.
36. ✓ Merge layer down — `mergeLayerDown()` moves all elements to layer below, removes source.
37. ✓ Show/Hide all layers — batch visibility toggle for all layers on current page.
38. ✓ Rename layer — `renameLayerDialog()` with `QInputDialog::getText`.

### Phase 10 — Page management ✓

39. ✓ Add page before — `addPageBefore(pageIndex)` inserts blank page before current.
40. ✓ Move page up/down — `movePageTowards(pageIndex, direction)` reorders pages in document,
    scrolls to new position after move.

### Phase 11 — Z-order & zoom ✓

41. ✓ Bring to front / send to back / bring forward / send backward — z-order commands
    via `Layer::removeElement` + `insertElement`/`addElement`.
42. ✓ Fit width — `fitWidth()` zooms so the widest page fills the viewport.
43. ✓ Zoom to 100% — `zoomToActualSize()` resets zoom factor to 1.0.

### Phase 12 — Pen styling & font ✓

44. ✓ Pen line style — `setPenLineStyle(style)` sets `penLineStyle` in `QtToolState`,
    applied to strokes via `StrokeStyle::parseStyle` in `beginStroke`.
45. ✓ Stroke fill — `setStrokeFill(opacity)` toggles fill on pen strokes,
    passed through `beginStroke` to `Stroke::setFill`.
46. ✓ Font selection — `selectFont()` uses `QFontDialog` to set `fontName`/`fontSize`
    in `QtToolState`.

### Phase 13 — Navigation history & layer navigation ✓

47. ✓ Navigate back/forward — `NavPoint` history vector in `QtAppShell`, records position
    on page jumps, Alt+Left / Alt+Right to traverse.
48. ✓ Layer navigation — `gotoNextLayer`, `gotoPrevLayer`, `gotoTopLayer` switch selected
    layer via `QtDocumentController::selectLayer`.
49. ✓ Add layer above/below — `addLayerAbove`/`addLayerBelow` create new layers relative
    to the current selection.

### Phase 14 — Annotated page navigation ✓

50. ✓ Next/previous annotated page — `gotoNextAnnotatedPage`/`gotoPrevAnnotatedPage`
    iterate pages checking `isPageAnnotated()`, records nav point before jumping.
