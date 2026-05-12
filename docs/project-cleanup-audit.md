# Project Cleanup Audit

This audit records the current cleanup state after the Qt shell migration work.

## Plugin API

- Bundled plugins were checked for deprecated GTK UI wrapper use.
- Bundled plugins use `app.registerUi(...)` and document/data APIs; they do not
  call `app.uiAction`, `app.sidebarAction`, `app.getSidebarPageNo`,
  `app.setSidebarPageNo`, or `app.showFloatingToolbox`.
- The deprecated Lua functions stay exported as compatibility shims for
  third-party plugins. They route through Qt-neutral services, not GTK
  `Control`.

## SVG Assets

- UI SVG assets are centralized under `ui/`.
- The Qt menu checkmark asset now lives at `ui/iconsCommon/menu-check.svg`.
- Theme icons remain under the freedesktop-style theme directories:
  `ui/iconsColor-*` and `ui/iconsLucide-*`. The apparent duplication is
  intentional because light/dark and color/lucide themes are resolved through
  icon-theme lookup.
- `QtLegacyBoundaryTest.qtSourceTreeDoesNotOwnSvgAssets` guards against adding
  SVG assets directly under `src/qt`.

## Empty Files And Folders

- Removed empty, untracked directories:
  - `src/core/control/jobs`
  - `src/legacy/gtk`
  - `src/legacy/render`
  - `src/legacy`
  - `src/util/raii`
- Kept zero-byte tracked files that are semantically meaningful:
  - `debian/links`
  - `test/files/no_palettes/empty.txt`

## Remaining Audit Notes

- `.po` files still contain historical source references to removed legacy job
  paths. These are translation metadata references, not build inputs.
- Third-party plugins can still call deprecated wrappers by design. Removing
  those exports would be a breaking plugin API change.
