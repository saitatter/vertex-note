# Qt Migration Bootstrap

This document tracks the first executable slices of the Qt migration.

## Current Scope

- `src/core/ui/common/` now contains platform-neutral shell interfaces.
- `src/core/ui/input/` now contains a Qt-independent input event shape and sink interface.
- `src/core/view/render/` now contains the first backend-neutral interactive render contracts.
- The Qt shell now also carries a minimal `QtPainterRenderContext` wrapper beside the Cairo wrapper.
- `ENABLE_QT_EXPERIMENTAL` adds an optional `Qt Widgets` bootstrap target.
- The experimental target now builds a runnable Qt shell:
  - `QApplication`
  - `QMainWindow`
  - command host bootstrap
  - clipboard / dialogs / recent files / updater / plugin UI bridge stubs
  - canvas viewport with translated mouse, pen, wheel, key, and touch input
  - pan / zoom / fit-page interactions
  - real document loading for `.xopp`, `.xoj`, `.xopt`, and `.pdf`
  - experimental `.vnsession` open/save flow as a viewport sidecar linked to a document path
  - shared-core page stack preview driven by `Document` and `NotePage`
  - first background preview renderer wired through the backend-neutral render seam
  - GTK page-type previews now consume the same background render model seam through a Cairo preview renderer

## Build

The Qt shell is intentionally opt-in.

```powershell
pacman -S mingw-w64-x86_64-qt6-base
powershell -ExecutionPolicy Bypass -File scripts/mingw64-dev.ps1 build-qt
```

Current binary name:

- `vertex-note-qt-experimental`

Run it with:

```powershell
powershell -ExecutionPolicy Bypass -File scripts/mingw64-dev.ps1 run-qt
```

## Intentional Limits

- The shipping GTK application remains the primary shell.
- The experimental Qt target now opens real core documents, but it does not yet host `Control`, editing tools, or full `.xopp` workflow parity.
- The canvas now renders the real notebook page stack shape and page background metadata, but not page contents or overlays from the production renderer yet.
- The render seam is still early; background preview now routes through a renderer contract, while Cairo remains the active production renderer.

## Next Slices

1. Map `Control` entrypoints to the neutral shell interfaces.
2. Route interactive notebook painting through the new render contracts so GTK and Qt can share page rendering logic.
3. Replace the page-preview canvas with real renderers for strokes, text, backgrounds, overlays, and selection state.
