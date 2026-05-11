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
| `autoloadPdfXoj` | Implemented | `pdf/autoloadPdfXoj`. |
| `defaultPdfExportName` | Implemented | `pdf/defaultExportName`. |
| `pageTemplate` | Partial | Qt persists default page width/height under `page/defaultWidth` and `page/defaultHeight`; background template presets exist in the Qt page template flow. |
| `audioFolder`, `audioSampleRate`, `audioGain`, `defaultSeekTime` | Implemented | `audio/folder`, `audio/sampleRate`, `audio/gain`, `audio/defaultSeekTimeSeconds`. |
| `pluginEnabled`, `pluginDisabled` | Qt-only equivalent | Qt stores per-plugin enabled overrides under `plugins/enabled/<plugin-key>`. |
| `latexSettings.globalTemplatePath` | Implemented | `latex/templatePath`. |
| `touchDrawing` | Implemented | `general/touchDrawing`. |
| `snapRotation`, `vertexNoteGeometrySnapEnabled`, `vertexNoteGridSnapEnabled` | Implemented | `general/rotationSnap`, `general/geometrySnap`, `general/gridSnap`. |
| `strokeRecognizerMinSize` | Implemented | `general/strokeRecognizerMinSize`. |
| `laserPointerFadeOutTime` | Implemented | `general/laserPointerFadeOutMs`. |
| `defaultViewModeAttributes`, `fullscreenViewModeAttributes`, `presentationViewModeAttributes` | Partial | Qt persists the active chrome/layout pieces individually rather than serializing GTK view-mode strings. |

## Partial Coverage

| GTK setting key(s) | Current Qt behavior | Gap |
| --- | --- | --- |
| `pressureSensitivity`, `minimumPressure`, `pressureMultiplier`, `pressureGuessing` | Qt exposes default pressure-sensitive drawing as `tools/defaultPressureSensitive`. | No Qt UI for minimum pressure, multiplier, or speed-based pressure guessing. |
| `stylusCursorType`, `eraserVisibility` | Qt canvas can hide the cursor for eraser/right-button eraser behavior. | No persisted Qt preferences for stylus cursor style or eraser visibility policy. |
| `iconTheme`, `themeVariant`, `useStockIcons` | Qt uses bundled Qt icons and native Qt palette defaults. | No user-selectable icon theme or light/dark override in Qt settings. |
| `showPageShadow` | Qt rendering/layout has its own page presentation. | No explicit persisted toggle. |
| `presentationMode` | Qt has the `view.presentation` command. | Presentation state is not persisted as a settings default. |
| `lastSavePath`, `lastOpenPath`, `lastImagePath` | Qt uses native file dialogs and recent documents. | Last dialog directories are not separately persisted. |
| `colorPalette` | Qt has built-in quick colors/tool state. | No Qt palette-file selection or cycling settings parity yet. |
| `latexSettings.defaultText`, `latexSettings.genCmd`, `latexSettings.*editor*`, `latexSettings.sourceView*`, `latexSettings.useExternalEditor`, `latexSettings.externalEditor*`, `latexSettings.temporaryFileExt`, `latexSettings.autoCheckDependencies` | Qt can render LaTeX using the template path. | Advanced LaTeX editor/generator settings are not exposed in Qt. |
| `audioInputDevice`, `audioOutputDevice`, `disableAudio` | Qt audio settings cover folder, sample rate, gain, seek step and uses the shared audio backend. | No Qt device selector or global disable toggle in settings. |
| `numPairsOffset` | Qt pairs pages through two-column layout. | No pairs parity offset command/setting. |
| `emptyLastPageAppend` | Qt can add/append pages manually. | No automatic last-page append policy setting. |

## Legacy GTK Or Unsupported

| GTK setting key(s) | Status | Notes |
| --- | --- | --- |
| `zoomGesturesEnabled`, `gtkTouchInertialScrolling`, `touchZoomStartThreshold` | Legacy GTK / unsupported | Gesture semantics need a Qt input design before migration. |
| `edgePanSpeed`, `edgePanMaxMult`, `zoomStep`, `zoomStepScroll`, `displayDpi` | Unsupported | Qt uses hardcoded/current viewport behavior for now. |
| `sidebarWidth`, `sidebarOnRight`, `scrollbarOnLeft`, `sidebarNumberingStyle`, `scrollbarHideType`, `disableScrollbarFadeout` | Legacy GTK / partial | Qt dock/sidebar placement is owned by `window/state`; numbering and scrollbar policies are not exposed. |
| `filepathShownInTitlebar`, `pageNumberShownInTitlebar` | Unsupported | Qt title/status presentation is fixed by the shell. |
| `autoloadMostRecent` | Unsupported | Qt persists recent documents but does not auto-open the newest one. |
| `autosaveEnabled`, `autosaveTimeout` | Unsupported | No Qt autosave scheduler yet. |
| `addHorizontalSpace`, `addHorizontalSpaceAmountRight`, `addHorizontalSpaceAmountLeft`, `addVerticalSpace`, `addVerticalSpaceAmountAbove`, `addVerticalSpaceAmountBelow` | Unsupported | Space insertion settings are not wired into Qt tools. |
| `unlimitedScrolling` | Unsupported | Qt canvas currently uses its own scroll model. |
| `drawDirModsEnabled`, `drawDirModsRadius` | Unsupported | No Qt directional drawing modifier support yet. |
| `snapRotationTolerance`, `snapGridTolerance`, `snapGridSize` | Partial / unsupported | Boolean snap toggles exist; tolerances/grid size are not configurable. |
| `vertexNoteAutomaticUpdateCheckEnabled` | Unsupported | Qt can check for updates manually, but no persisted automatic check setting. |
| `highlightPosition`, `cursorHighlightColor`, `cursorHighlightBorderColor`, `cursorHighlightRadius`, `cursorHighlightBorderWidth` | Unsupported | GTK position-highlighting plugin/settings are not a Qt shell feature yet. |
| `recolor.enabled`, `recolor.sidebar`, `recolor.dark`, `recolor.light` | Unsupported | No Qt recolor mode. |
| `selectionBorderColor`, `backgroundColor`, `selectionMarkerColor`, `activeSelectionColor` | Unsupported | Qt selection/theme colors are not user-configurable. |
| `pageRerenderThreshold`, `pdfPageCacheSize`, `preloadPagesBefore`, `preloadPagesAfter`, `eagerPageCleanup` | Unsupported | Qt rendering cache policy is not settings-backed yet. |
| `sizeUnit` | Unsupported | Qt numeric settings currently use fixed point/second units in labels. |
| `strokeFilterIgnoreTime`, `strokeFilterIgnoreLength`, `strokeFilterSuccessiveTime`, `strokeFilterEnabled`, `doActionOnStrokeFiltered`, `trySelectOnStrokeFiltered` | Unsupported | No Qt stroke filtering settings path yet. |
| `snapRecognizedShapesEnabled`, `restoreLineWidthEnabled` | Unsupported | Shape recognizer exists, but these GTK recognizer settings are not migrated. |
| `numIgnoredStylusEvents`, `inputSystemTPCButton`, `inputSystemDrawOutsideWindow` | Legacy GTK / unsupported | Device input system settings depend on GTK/GDK input handling. |
| `preferredLocale` | Unsupported | Qt shell does not expose localization selection yet. |
| `useSpacesForTab`, `numberOfSpacesForTab` | Unsupported | Text editor tab behavior is not user-configurable in Qt. |
| `stabilizerAveragingMethod`, `stabilizerPreprocessor`, `stabilizerBuffersize`, `stabilizerSigma`, `stabilizerDeadzoneRadius`, `stabilizerDrag`, `stabilizerMass`, `stabilizerCuspDetection`, `stabilizerFinalizeStroke` | Unsupported | Stroke stabilizer settings are not surfaced in Qt. |
| `font` | Partial | Qt text tool has current font state, but no persisted full GTK default font model. |
| Custom data `deviceClasses`, `buttonConfig` | Legacy GTK | Qt settings dialog intentionally shows Devices as unavailable until a clean Qt backend exists. |

## Next Settings Work

1. Add a Qt-native autosave service before migrating `autosaveEnabled` and
   `autosaveTimeout`.
2. Add Qt input-device discovery before migrating `deviceClasses`,
   `buttonConfig`, stylus cursor policy, and ignored stylus events.
3. Add advanced Tools settings for pressure thresholds, snap tolerances, grid
   size, stroke filtering, and stabilizer controls.
4. Add Appearance settings for theme variant, icon theme, selection colors,
   page shadow, titlebar content, and recolor mode.
5. Add PDF cache/performance settings only after the Qt PDF render/cache layer
   has stable knobs to expose.
