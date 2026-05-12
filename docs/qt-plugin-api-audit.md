# Qt Plugin API Compatibility Audit

This compares `plugins/luapi_application.def.lua` against the Qt shell
`QT_APP_LIB` in `src/qt/services/QtLuaPluginRuntime.cpp`.

## Summary

- Lua stub APIs: 54
- Qt shell APIs exported: 54
- Missing from Qt shell: 0
- Extra Qt-only APIs: 0

## Implemented In Qt

These APIs are present in `QT_APP_LIB`:

`activateAction`, `addImages`, `addSplines`, `addStrokes`, `addTexts`,
`addToSelection`, `changeActionState`, `changeBackgroundPdfPageNr`,
`changeCurrentPageBackground`, `changeToolColor`, `clearSelection`, `export`,
`fileDialogOpen`, `fileDialogSave`, `getActionState`, `getColorPalette`,
`getDisplayDpi`, `getDocumentStructure`, `getFilePath`, `getFolder`,
`getFont`, `getFonts`, `getImages`, `getPageLabel`, `getScrollPos`,
`getSidebarPageNo`, `getStrokes`, `getTexts`, `getToolInfo`, `getZoom`,
`glib_rename`, `layerAction`, `msgbox`, `openDialog`, `openFile`,
`refreshPage`, `registerPlaceholder`, `registerUi`, `saveAs`,
`scrollToPage`, `scrollToPos`, `setBackgroundName`, `setCurrentLayer`,
`setCurrentLayerName`, `setCurrentPage`, `setFont`, `setLayerVisibility`,
`setPageSize`, `setPlaceholderValue`, `setSidebarPageNo`, `setZoom`,
`showFloatingToolbox`, `sidebarAction`, `uiAction`.

`getActionState`, `changeActionState`, and `activateAction` are exported but
still partial. `getActionState` now covers layout, zoom, snapping, active tool,
tool colour, pen line style, fill, fill opacity, active tool width, sidebar /
toolbar / menubar visibility, paired pages, fullscreen, and presentation mode.
These APIs should keep growing as more GTK action state becomes UI-neutral.

## Missing From Qt

None.

## Compatibility Shims

| API | Status | Notes |
| --- | --- | --- |
| `addSplines` | Implemented | Rasterizes cubic spline coordinates into stroke elements, matching the legacy plugin data shape. |
| `getScrollPos` | Implemented | Reads the active `QtCanvas` viewport. |
| `scrollToPos` | Implemented | Supports relative and absolute canvas scrolling without marking the document dirty. |
| `getSidebarPageNo` | Implemented / compatibility | Returns the active Qt sidebar compatibility page. `1` raises page previews, `2` raises layers; higher positive values are stored for plugin compatibility without a matching Qt dock. |
| `setSidebarPageNo` | Implemented / compatibility | Raises the page or layer dock for known values and stores higher positive values so older plugins do not fail. |
| `layerAction` | Shim | Maps known legacy layer action constants to Qt command IDs. |
| `sidebarAction` | Shim | Maps legacy page sidebar actions to Qt commands: copy/duplicate, delete, new before, new after, move up, and move down. |
| `showFloatingToolbox` | Implemented / compatibility | Shows Qt floating toolbar windows when the active toolbar profile has floating toolbar contents, and positions the first visible floating toolbar when coordinates are provided. |
| `uiAction` | Shim | Maps known legacy action constants to Qt command IDs; disabled actions update command enabled state where available. |

## Bundled Plugin Wrapper Audit

Bundled plugins no longer depend on GTK UI wrapper behavior directly. The
current bundled plugin set uses `app.registerUi(...)` for menu/toolbar entry
points and document/data APIs for the actual work. It does not call the legacy
compatibility wrappers `app.uiAction`, `app.sidebarAction`,
`app.getSidebarPageNo`, `app.setSidebarPageNo`, or
`app.showFloatingToolbox`.

`plugins/Export/main.lua` still passes `backend = "cairo"` to `app.export`.
That is an export backend selector rather than GTK UI wrapper use.

The deprecated functions remain exported intentionally so third-party plugins
continue to load. Qt maps them through `QtCommandHost`,
`QtDocumentController`, and `IPluginUiBridge` instead of instantiating GTK
`Control` or GTK widgets.

## Follow-Up

1. Encourage third-party plugins to move from legacy wrappers to explicit
   Qt-neutral document, command, menu, and toolbar APIs over time.
2. Expand `sidebarAction` only if Qt gains a unified sidebar tab model with
   additional GTK-only sidebar actions.
