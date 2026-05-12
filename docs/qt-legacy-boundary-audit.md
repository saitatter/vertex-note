# Qt Legacy Boundary Audit

The Qt shell must not depend directly on GTK widgets, GDK devices, legacy drawing
contexts, or the legacy GTK `Control` class. Shared model, PDF, renderer, and
plugin logic may still use older non-UI infrastructure while the migration is in
progress, but those dependencies must stay behind explicit boundaries.

## Enforced Boundary

`test/unit_tests/vertexnote/QtLegacyBoundaryTest.cpp` scans every `.h` and
`.cpp` file under `src/qt` and rejects direct includes of:

- GTK headers
- GDK headers
- Legacy drawing headers
- `control/Control.h`

This protects future Qt features from accidentally reaching back into GTK UI
classes.

## Current Allowed Legacy Boundaries

- `src/core/pdf/*`: PDF loading, text, links, bookmarks, and preview
  rasterization stay behind the shared `PdfDocument`/`PdfPage` boundary and use
  the Poppler C++/internal APIs without exposing backend-specific types to Qt.
- `src/core/view/render/*`: shared render models remain backend-neutral and are
  painted by the Qt shell.
- `src/core/control/settings/*`: legacy settings still model GTK/GDK device
  concepts; Qt mirrors only UI-neutral or Qt-native settings in `QSettings`.
- `src/core/control/*`: legacy control classes that have no Qt owner yet should
  not be included by `src/qt`.

## Cleanup Order

1. Keep Qt UI features using `QtDocumentController`, `QtCanvas`, Qt renderers,
   and `IPluginUiBridge` rather than GTK `Control`.
2. When a Qt feature needs model behavior that currently lives in GTK control
   code, extract a UI-neutral service first.
3. Keep PDF backend use behind `PdfPage`, raster models, and
   `PageRenderSnapshot`; do not pass backend-specific types into Qt widgets.
4. Add unit coverage when a new boundary is introduced so future Qt work cannot
   bypass it accidentally.
