# Qt Migration Bootstrap

This document tracks the first executable slice of the Qt migration.

## Current Scope

- `src/core/ui/common/` now contains platform-neutral shell interfaces.
- `src/core/ui/input/` now contains a Qt-independent input event shape and sink interface.
- `src/core/view/render/` now contains the first backend-neutral interactive render contracts.
- The Qt shell now also carries a minimal `QtPainterRenderContext` wrapper beside the Cairo wrapper.
- `ENABLE_QT_EXPERIMENTAL` adds an optional `Qt Widgets` bootstrap target.
- The experimental target currently builds a minimal shell:
  - `QApplication`
  - `QMainWindow`
  - command host bootstrap
  - clipboard / dialogs / recent files / updater / plugin UI bridge stubs
  - canvas placeholder widget with translated mouse, pen, wheel, key, and touch input debug overlay

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
- The experimental Qt target does not yet host `Control`, document workflows, dialogs, or rendering parity.
- The canvas is currently a placeholder bootstrap surface, not the notebook renderer.
- The render seam is scaffolded only; Cairo remains the active production renderer.

## Next Slices

1. Map `Control` entrypoints to the neutral shell interfaces.
2. Connect the experimental canvas to a real viewport/document shell instead of the debug overlay.
3. Route interactive GTK painting through the new render contracts, then add a Qt painter backend beside it.
