# Qt Plugin API Compatibility Audit

This compares `plugins/luapi_application.def.lua` against the Qt shell
`QT_APP_LIB` in `src/qt/QtLuaPluginRuntime.cpp`.

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
tool colour, pen line style, fill, fill opacity, and active tool width. These
APIs should keep growing as more GTK action state becomes UI-neutral.

## Missing From Qt

None.

## Compatibility Shims

| API | Status | Notes |
| --- | --- | --- |
| `addSplines` | Implemented | Rasterizes cubic spline coordinates into stroke elements, matching the legacy plugin data shape. |
| `getScrollPos` | Implemented | Reads the active `QtCanvas` viewport. |
| `scrollToPos` | Implemented | Supports relative and absolute canvas scrolling without marking the document dirty. |
| `getSidebarPageNo` | Compatibility no-op | Returns `1`; Qt has separate page/layer docks instead of GTK sidebar tabs. |
| `setSidebarPageNo` | Compatibility no-op | Accepts positive page numbers so older plugins do not fail when switching GTK sidebar tabs. |
| `layerAction` | Shim | Maps known legacy layer action constants to Qt command IDs. |
| `sidebarAction` | Partial shim | Maps page sidebar actions that exist in Qt: copy/duplicate, delete, new before, new after. Move up/down are accepted as no-ops until Qt has page movement commands. |
| `showFloatingToolbox` | Compatibility no-op | The Qt shell does not expose the GTK floating toolbox. |
| `uiAction` | Shim | Maps known legacy action constants to Qt command IDs; disabled actions update command enabled state where available. |

## Follow-Up

1. Replace deprecated GTK wrapper use in bundled and third-party plugins with
   explicit Qt-neutral APIs over time.
2. Expand `sidebarAction` only if Qt gains page movement commands or a unified
   sidebar tab model.
