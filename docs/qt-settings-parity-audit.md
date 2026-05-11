# Qt Settings Parity Audit

This audit compares GTK `Settings` keys from
`src/core/control/settings/Settings.cpp` with the Qt shell settings model in
`src/qt/QtSettingsDialog.h`, `QtAppShell`, and the Qt plugin runtime.

## Status Legend

- **Implemented**: Qt persists and applies the same user-visible behavior.
- **Qt-only equivalent**: Qt persists the behavior under a separate QSettings
  key because the Qt shell intentionally does not share GTK settings storage.
- **Partial**: Qt has a subset of the behavior.
- **Legacy GTK**: the key is tied to GTK widgets, GDK input, or the legacy
  shell layout and should not be copied into Qt directly.
- **Unsupported**: the behavior is still missing in the Qt shell.

## Implemented Or Qt-Only Equivalent

| GTK setting key(s) | Qt status | Qt storage/API |
| --- | --- | --- |
| `selectedToolbar` | Qt-only equivalent | `general/toolbarProfileId` plus `toolbar/custom/<toolbar>` tokens. |
| `showToolbar`, `menubarVisible`, `showSidebar` | Implemented | `view/showToolbar`, `view/showMenubar`, `view/showSidebar`. |
| `mainWndWidth`, `mainWndHeight`, `maximized` | Qt-only equivalent | `window/geometry`, `window/state`. |
| `numColumns`, `numRows`, `viewFixedRows` | Implemented | `view/layoutColumnsRows`; positive = columns, negative = rows. |
| `showPairedPages` | Implemented | `view/pairedPages`; also synchronized with 2 columns. |
| `layoutVertical`, `layoutRightToLeft`, `layoutBottomToTop` | Implemented | `view/verticalLayout`, `view/layoutRtl`, `view/layoutBtt`. |
| `displayDpi` | Partial / Qt-only equivalent | `view/displayDpi`; exposed as Automatic/manual DPI and used by the Qt Lua plugin `app.getDisplayDpi()` API. Qt canvas zoom remains device-independent. |
| `autoloadPdfXoj` | Implemented | `pdf/autoloadPdfXoj`. |
| `defaultPdfExportName` | Implemented | `pdf/defaultExportName`. |
| `pdfPageCacheSize`, `preloadPagesBefore`, `preloadPagesAfter`, `eagerPageCleanup`, `pageRerenderThreshold` | Implemented | `pdf/pageCacheSize`, `pdf/preloadPagesBefore`, `pdf/preloadPagesAfter`, `pdf/eagerPageCleanup`, `pdf/pageRerenderThreshold`; applied to the Qt PDF background raster cache. |
| `pageTemplate` | Partial | Qt persists default page width/height under `page/defaultWidth` and `page/defaultHeight`; background template presets exist in the Qt page template flow. |
| `addHorizontalSpace`, `addHorizontalSpaceAmountRight`, `addHorizontalSpaceAmountLeft`, `addVerticalSpace`, `addVerticalSpaceAmountAbove`, `addVerticalSpaceAmountBelow` | Qt-only equivalent | `page/addHorizontalSpace*` and `page/addVerticalSpace*`; applied to Qt canvas scrollable page padding. The Vertical Space tool also supports click-drag insertion with undo/redo. |
| `emptyLastPageAppend` | Implemented | `page/emptyLastPageAppend`; Qt appends a blank page on draw/scroll at the last page when no PDF is attached. |
| `sizeUnit` | Implemented | `page/sizeUnit`; Qt page defaults show cm/in/points while preserving model values in points. |
| `audioFolder`, `audioSampleRate`, `audioGain`, `defaultSeekTime`, `audioInputDevice`, `audioOutputDevice`, `disableAudio` | Implemented | `audio/folder`, `audio/sampleRate`, `audio/gain`, `audio/defaultSeekTimeSeconds`, `audio/inputDevice`, `audio/outputDevice`, `audio/disabled`; Qt lists PortAudio devices and applies the shared audio backend settings. |
| `pluginEnabled`, `pluginDisabled` | Qt-only equivalent | Qt stores per-plugin enabled overrides under `plugins/enabled/<plugin-key>`. |
| `latexSettings.*` | Implemented | `latex/templatePath`, `latex/defaultText`, `latex/genCmd`, `latex/sourceView*`, `latex/editor*`, `latex/externalEditor*`, `latex/temporaryFileExt`, `latex/autoCheckDependencies`; Qt feeds the shared `LatexGenerator` settings used by Math TeX insertion. |
| `touchDrawing` | Implemented | `general/touchDrawing`. |
| `snapRotation`, `vertexNoteGeometrySnapEnabled`, `vertexNoteGridSnapEnabled` | Implemented | `general/rotationSnap`, `general/geometrySnap`, `general/gridSnap`. |
| `snapRotationTolerance` | Implemented | `general/rotationSnapTolerance`; applied to Qt stroke/shape rotation snapping. |
| `snapGridTolerance`, `snapGridSize` | Implemented | `tools/snapGridTolerance`, `tools/snapGridSize`; applied to Qt geometry grid snapping. |
| `zoomStep`, `zoomStepScroll` | Implemented | `view/zoomStepPercent`, `view/zoomStepScrollPercent`; applied to Qt zoom commands and Ctrl+wheel zoom. |
| `unlimitedScrolling` | Implemented | `view/unlimitedScrolling`; when disabled, Qt clamps the viewport to the document scene bounds plus configured page padding. |
| `edgePanSpeed`, `edgePanMaxMult` | Implemented | `view/edgePanSpeed`, `view/edgePanMaxMult`; applied to Qt edge-pan while dragging strokes, erasers, selections, PDF text selections, vertical space, shapes, and geometry vertices. |
| `pressureSensitivity`, `minimumPressure`, `pressureMultiplier`, `pressureGuessing` | Implemented | `tools/defaultPressureSensitive`, `tools/minimumPressure`, `tools/pressureMultiplier`, `tools/pressureGuessing`; applied to Qt stroke input pressure. |
| `font` | Qt-only equivalent | `tools/defaultFontName`, `tools/defaultFontSize`; applied to the Qt text tool, toolbar font controls, and Lua font access. |
| `stabilizerAveragingMethod`, `stabilizerBuffersize`, `stabilizerFinalizeStroke` | Partial / Qt-only equivalent | `tools/strokeStabilizerEnabled`, `tools/strokeStabilizerSamples`, `tools/strokeStabilizerStrength`, `tools/strokeStabilizerFinalizeStroke`; Qt applies a native moving-average stabilizer instead of reusing GTK `StrokeHandler`. |
| Custom data `deviceClasses`, `buttonConfig` | Qt-only equivalent | Qt discovers `QInputDevice`s, persists a global button matrix and per-device override matrices, and applies them for mouse/tablet/touch input. |
| `stylusCursorType`, `eraserVisibility` | Partial / Qt-only equivalent | `devices/eraserCursorHidden`; Qt persists eraser cursor hiding and active/right-button/per-device eraser cursor behavior. |
| `filepathShownInTitlebar`, `pageNumberShownInTitlebar` | Implemented | `appearance/showFilePathInTitlebar`, `appearance/showPageNumberInTitlebar`. |
| `showPageShadow` | Implemented | `appearance/showPageShadow`; applied to the Qt page background renderer. |
| `themeVariant`, `iconTheme`, `useStockIcons` | Qt-only equivalent | `appearance/themeVariant`, `appearance/iconTheme`; Qt applies System, Light, Dark, Color icons, and Lucide icons without sharing GTK theme storage. |
| `colorPalette` | Implemented | `appearance/colorPalettePath`; Qt loads `.gpl` palette files for toolbar quick colors and Lua `app.getColorPalette()`, falling back to the built-in palette on parse errors. |
| `autosaveEnabled`, `autosaveTimeout` | Implemented | `general/autosaveEnabled`, `general/autosaveTimeoutMinutes`; autosaves dirty existing `.xopp`/`.xoj`/`.xopt` documents without prompting. |
| `autoloadMostRecent` | Implemented | `general/autoloadMostRecent`; opens the first available recent document during Qt shell startup. |
| `preferredLocale` | Partial / Qt-only equivalent | `general/preferredLocale`; Qt exposes the installed locale list and updates the `LANGUAGE` environment for subsequent gettext lookups/startup. Existing widgets are not dynamically retranslatable yet. |
| `vertexNoteAutomaticUpdateCheckEnabled` | Qt-only equivalent | `general/automaticUpdateCheckEnabled`; runs the shared update checker on startup and keeps manual Help > Check for Updates. |
| `presentationMode` | Implemented | `view/presentationModeDefault`; Qt can start directly in presentation mode. |
| `lastSavePath`, `lastOpenPath`, `lastImagePath` | Qt-only equivalent | `paths/lastSave`, `paths/lastOpen`, `paths/lastImage`, plus Qt-only `paths/lastPdf` and `paths/lastExport`. |
| `strokeRecognizerMinSize` | Implemented | `general/strokeRecognizerMinSize`. |
| `snapRecognizedShapesEnabled` | Implemented | `tools/snapRecognizedShapesEnabled`; applied to Qt shape recognizer finalization. |
| `drawDirModsEnabled`, `drawDirModsRadius` | Partial / Qt-only equivalent | `tools/drawDirModsEnabled`, `tools/drawDirModsRadius`; applied to Qt rectangle, ellipse, and coordinate-system drag tools for shift/control emulation. |
| `strokeFilterIgnoreTime`, `strokeFilterIgnoreLength`, `strokeFilterSuccessiveTime`, `strokeFilterEnabled`, `doActionOnStrokeFiltered`, `trySelectOnStrokeFiltered` | Partial / Qt-only equivalent | `tools/strokeFilter*`, `tools/doActionOnStrokeFiltered`, `tools/trySelectOnStrokeFiltered`; Qt filters very short, fast strokes and can try selecting under the filtered stroke endpoint. The legacy floating toolbox action is stored but not shown in Qt. |
| `laserPointerFadeOutTime` | Implemented | `general/laserPointerFadeOutMs`. |
| `recolor.enabled`, `recolor.sidebar`, `recolor.dark`, `recolor.light` | Implemented | `appearance/recolorMainView`, `appearance/recolorSidebarMiniatures`, `appearance/recolorLight`, `appearance/recolorDark`; applied to Qt canvas and page sidebar previews. |
| `backgroundColor` | Implemented | `appearance/backgroundColor`; applied to the Qt canvas/document background outside page bounds. |
| `useSpacesForTab`, `numberOfSpacesForTab` | Implemented | `tools/useSpacesForTab`, `tools/numberOfSpacesForTab`; applied to the Qt text editor overlay. |
| `selectionBorderColor`, `selectionMarkerColor`, `activeSelectionColor` | Partial / Qt-only equivalent | `appearance/selectionColor`; Qt uses one selection accent color for selection, hover, handles, and geometry overlays. |
| `highlightPosition`, `cursorHighlightColor`, `cursorHighlightBorderColor`, `cursorHighlightRadius`, `cursorHighlightBorderWidth` | Implemented | `appearance/highlightPosition`, `appearance/cursorHighlight*`; Qt draws the same kind of cursor position halo on the canvas. |
| `defaultViewModeAttributes`, `fullscreenViewModeAttributes`, `presentationViewModeAttributes` | Partial | Qt persists the active chrome/layout pieces individually rather than serializing GTK view-mode strings. |

## Partial Coverage

| GTK setting key(s) | Current Qt behavior | Gap |
| --- | --- | --- |
| `numPairsOffset` | Qt pairs pages through two-column layout. | No pairs parity offset command/setting. |
## Legacy GTK Or Unsupported

| GTK setting key(s) | Status | Notes |
| --- | --- | --- |
| `zoomGesturesEnabled`, `gtkTouchInertialScrolling`, `touchZoomStartThreshold` | Legacy GTK / unsupported | Gesture semantics need a Qt input design before migration. |
| `sidebarWidth`, `sidebarOnRight`, `scrollbarOnLeft`, `sidebarNumberingStyle`, `scrollbarHideType`, `disableScrollbarFadeout` | Legacy GTK / partial | Qt dock/sidebar placement is owned by `window/state`; numbering and scrollbar policies are not exposed. |
| `restoreLineWidthEnabled` | Unsupported | Qt selection resizing does not yet expose the GTK scale workflow that uses this setting. |
| `numIgnoredStylusEvents`, `inputSystemTPCButton`, `inputSystemDrawOutsideWindow` | Legacy GTK / unsupported | Device input system settings depend on GTK/GDK input handling. |
| `stabilizerPreprocessor`, `stabilizerSigma`, `stabilizerDeadzoneRadius`, `stabilizerDrag`, `stabilizerMass`, `stabilizerCuspDetection` | Unsupported | Qt has a native moving-average stabilizer, but not GTK's full deadzone/inertia/gaussian stabilizer matrix. |

## Next Settings Work

1. Add `numPairsOffset` if Qt needs GTK's asymmetric paired-page first-page offset instead of the current 2-column shortcut.
2. Decide whether GTK's remaining gesture/sidebar/window-policy settings should stay GTK-only or receive Qt-native behavior.
3. Add `restoreLineWidthEnabled` only after Qt exposes selection scaling/resizing; the current Qt selection workflow moves elements but does not scale them.
4. Add the full GTK stabilizer matrix only if Qt needs more than the current native moving-average stabilizer.
