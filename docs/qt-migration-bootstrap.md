# Qt Migration Bootstrap

This document tracks the first executable slice of the Qt migration.

## Current Scope

- `src/core/ui/common/` now contains platform-neutral shell interfaces.
- `ENABLE_QT_EXPERIMENTAL` adds an optional `Qt Widgets` bootstrap target.
- The experimental target currently builds a minimal shell:
  - `QApplication`
  - `QMainWindow`
  - command host bootstrap
  - canvas placeholder widget

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

## Next Slices

1. Map `Control` entrypoints to the neutral shell interfaces.
2. Introduce a backend-neutral command model between `ActionDatabase` and the shell.
3. Start a render seam for interactive notebook painting.
