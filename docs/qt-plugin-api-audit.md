# Qt Plugin API Compatibility Audit

This compares `plugins/luapi_application.def.lua` against the Qt shell
`QT_APP_LIB` in `src/qt/QtLuaPluginRuntime.cpp`.

## Summary

- Lua stub APIs: 54
- Qt shell APIs exported: 44
- Missing from Qt shell: 10
- Extra Qt-only APIs: 0

## Implemented In Qt

These APIs are present in `QT_APP_LIB`:

`activateAction`, `addImages`, `addStrokes`, `addTexts`, `addToSelection`,
`changeActionState`, `changeBackgroundPdfPageNr`,
`changeCurrentPageBackground`, `changeToolColor`, `clearSelection`, `export`,
`fileDialogOpen`, `fileDialogSave`, `getActionState`, `getColorPalette`,
`getDisplayDpi`, `getDocumentStructure`, `getFilePath`, `getFolder`,
`getFont`, `getFonts`, `getImages`, `getPageLabel`, `getStrokes`,
`getTexts`, `getZoom`, `glib_rename`, `msgbox`, `openDialog`, `openFile`,
`refreshPage`, `registerPlaceholder`, `registerUi`, `saveAs`,
`scrollToPage`, `setBackgroundName`, `setCurrentLayer`,
`setCurrentLayerName`, `setCurrentPage`, `setFont`, `setLayerVisibility`,
`setPageSize`, `setPlaceholderValue`, `setZoom`.

`getActionState`, `changeActionState`, and `activateAction` are exported but
still partial. They currently cover the command/action families wired into the
Qt shell and should grow as more GTK action state becomes UI-neutral.

## Missing From Qt

| API | Status | Notes |
| --- | --- | --- |
| `addSplines` | Unsupported | Can likely be mapped onto stroke insertion once spline serialization is mirrored in the Qt runtime. |
| `getScrollPos` | Unsupported | Needs a UI-neutral viewport provider from `QtCanvas`. |
| `scrollToPos` | Unsupported | Needs a UI-neutral viewport scroller from `QtCanvas`. |
| `getSidebarPageNo` | Unsupported | Needs page-sidebar state exposed without GTK assumptions. |
| `setSidebarPageNo` | Unsupported | Needs page-sidebar navigation bridge. |
| `getToolInfo` | Unsupported | Needs a read-only Qt tool-state provider for current colour, size, line style, fill, and tool type. |
| `layerAction` | Unsupported | GTK action wrapper; should become explicit document/layer operations if needed. |
| `sidebarAction` | Unsupported | GTK sidebar wrapper; should become explicit Qt sidebar operations if needed. |
| `showFloatingToolbox` | Unsupported | GTK UI affordance; Qt equivalent needs a deliberate toolbar/palette design. |
| `uiAction` | Unsupported | GTK action wrapper; prefer explicit Qt command IDs or `activateAction` mappings. |

## Follow-Up

1. Add a Qt tool-state provider so `getActionState("select-tool")`,
   `getActionState("tool-color")`, `getActionState("tool-pen-line-style")`,
   fill state, and size state can return useful values.
2. Add viewport providers before implementing `getScrollPos` and
   `scrollToPos`.
3. Decide whether GTK wrapper APIs (`uiAction`, `sidebarAction`,
   `layerAction`, `showFloatingToolbox`) should remain unsupported or receive
   Qt-specific compatibility shims.
