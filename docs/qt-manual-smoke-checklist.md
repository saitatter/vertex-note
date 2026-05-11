# Qt Manual Smoke Checklist

Use this checklist for the remaining Qt UI/UX parity pass after
`.\scripts\mingw64-dev.ps1 build` and `.\scripts\mingw64-dev.ps1 test` pass.

## PDF And Journal

1. Start the Qt shell with `.\scripts\mingw64-dev.ps1 run`.
2. Use `File > Annotate PDF...` with `test/files/cjk/测试.pdf`.
3. Choose both attach and external-link modes in separate runs.
4. Confirm both PDF pages appear as page backgrounds and page navigation works.
5. Delete the second page, then use `Journal > Append New PDF Pages`.
6. Confirm one page is appended, and a second append reports that there are no
   new PDF pages.
7. Use PDF text linear and area selection on
   `development/documentation/README-Eraser-and-padded-box.pdf`.
8. Confirm copied text includes visible PDF words, such as `Eraser`.
9. Use `Tools > PDF Text Marker Opacity`, change opacity, select PDF text, then
   run `Tools > Highlight Selected PDF Text`.
10. Confirm the highlight uses the selected opacity, participates in undo/redo,
    and does not create clipped or self-erasing stroke ends.

## Toolbar And Menus

1. Confirm the toolbar has separate stroke and vertex drawing dropdowns.
2. Confirm `DRAW` profile fallback still expands to both dropdowns.
3. Open `Tools > Stroke Drawing` and `Tools > Vertex Drawing`; compare group
   contents with GTK.
4. Open `Edit > Preferences... > Toolbar`; switch profiles, restart, and confirm
   the selected profile persists.
5. Run `Customize Toolbar...`, enter a known token list including
   `DRAW_STROKE` and `DRAW_VERTEX`, save, restart, and confirm the custom Qt
   profile persists without modifying GTK shared toolbar profiles.

## View Layout

1. Use `View > Layout > Columns` for 1, 2, 3, and 8 columns.
2. Use `View > Layout > Rows` for 1, 2, 3, and 8 rows.
3. Confirm `Pair Pages` checks only for two fixed columns.
4. Restart and confirm the last signed layout span persists.

## Chrome And Scaling

1. Compare GTK and Qt default windows for toolbar density, menu ordering,
   sidebar defaults, layer panel defaults, floating toolbar placement, and status
   bar feedback.
2. Repeat at Windows display scaling 100%, 125%, and 150%.
3. Resize the main window to a narrow layout and confirm toolbar/customize/settings
   text does not clip or overlap.
4. Toggle toolbar, menubar, sidebar, and presentation mode; restart and confirm
   expected persisted state.
5. Open the plugin manager and settings tabs; confirm tables and labels remain
   readable at narrow widths.

## Plugin Runtime

1. Open `Plugins > Plugin Manager...` and confirm bundled plugin status, enabled
   state, actions, placeholders, and load errors are visible.
2. Enable a bundled plugin that registers menu or toolbar UI, reload plugins, and
   confirm old UI registrations disappear before the new ones appear.
3. Exercise page sidebar plugin actions: new page, duplicate, delete, move up,
   and move down.

## Legacy Boundary

1. Run the GTK shell or GTK fallback build path, if enabled locally.
2. Confirm toolbar profile fallback still accepts `DRAW`.
3. Confirm no new Qt-only feature depends on GTK UI widgets or GTK `Control`.
