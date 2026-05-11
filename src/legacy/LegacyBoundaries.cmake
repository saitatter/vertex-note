function(vertexnote_match_legacy_sources source_list_var out_var)
    set(_matched)
    foreach(_source IN LISTS ${source_list_var})
        foreach(_pattern IN LISTS ARGN)
            if (_source MATCHES "${_pattern}")
                list(APPEND _matched "${_source}")
                break()
            endif ()
        endforeach ()
    endforeach ()
    set(${out_var} "${_matched}" PARENT_SCOPE)
endfunction()

set(VERTEXNOTE_LEGACY_GTK_CORE_PATTERNS
        ".*/src/core/control/(Control|ClipboardHandler|AudioController|LatexController|PrintHandler|RecentManager|VertexNoteMain)\\.(cpp|h)$"
        ".*/src/core/control/(ToolHandler|Tool)\\.(cpp|h)$"
        ".*/src/core/control/(GeometryToolController|UndoRedoController)\\.(cpp|h)$"
        ".*/src/core/control/(CompassController|SetsquareController|ScrollHandler|NavigationHistory|ExportHelper)\\.(cpp|h)$"
        ".*/src/core/control/DeviceListHelper\\.(cpp|h)$"
        ".*/src/core/control/PageBackgroundChangeController\\.cpp$"
        ".*/src/core/control/jobs/.*"
        ".*/src/core/control/layer/LayerController\\.(cpp|h)$"
        ".*/src/core/control/tools/.*Handler\\.(cpp|h)$"
        ".*/src/core/control/tools/(EditSelection|EditSelectionContents|PdfElemSelection|Selector|TextEditor)\\.(cpp|h)$"
        ".*/src/core/control/zoom/.*"
        ".*/src/core/control/actions/.*"
        ".*/src/core/gui/.*"
        ".*/src/core/model/(Compass|DocumentGtkContentsModel|ImageGtk|LinkDestinationGtk|Setsquare)\\.(cpp|h)$"
        ".*/src/core/pdf/base/(CairoPdfExport|HybridPdfExport|PdfExport|PdfExportFactory|QPdfExport)\\.(cpp|h)$"
        ".*/src/core/plugin/.*"
        ".*/src/core/undo/.*"
        ".*/src/core/view/(background|overlays)/.*"
        ".*/src/core/view/[^/]+\\.(cpp|h)$"
)

set(VERTEXNOTE_LEGACY_RENDER_CORE_PATTERNS
        ".*/src/core/control/(PdfCache|CompassController|SetsquareController)\\.(cpp|h)$"
        ".*/src/core/control/jobs/(ImageExport|PreviewJob|RenderJob|SaveJob)\\.(cpp|h)$"
        ".*/src/core/control/tools/(EditSelection|EditSelectionContents|PdfElemSelection|Selector|StrokeHandler|VerticalToolHandler)\\.(cpp|h)$"
        ".*/src/core/pdf/base/CairoPdfExport\\.(cpp|h)$"
        ".*/src/core/pdf/base/HybridPdfExport\\.cpp$"
        ".*/src/core/pdf/popplerapi/PopplerGlibPage\\.(cpp|h)$"
        ".*/src/core/model/(GeometryTool|SplineSegment|Stroke|StrokeContour)\\.(cpp|h)$"
        ".*/src/core/model/TexImage\\.cpp$"
        ".*/src/core/model/eraser/.*"
)

set(VERTEXNOTE_LEGACY_GTK_UTIL_PATTERNS
        ".*/src/util/(AppMessageBox|AppMessageBoxFallback|GtkUtil|gtk4_helper|UtilGtk)\\.(cpp|h)$"
        ".*/src/util/include/util/(AppMessageBox|ColorGtk|GtkUtil|gtk4_helper|PopupWindowWrapper)\\.h$"
        ".*/src/util/include/util/raii/(GtkPaperSizeUPtr|GtkWindowUPtr)\\.h$"
)

set(VERTEXNOTE_LEGACY_RENDER_UTIL_PATTERNS
        ".*/src/util/(Recolor|Util)\\.cpp$"
        ".*/src/util/include/util/(Recolor|Util)\\.h$"
        ".*/src/util/include/util/raii/CairoWrappers\\.h$"
)
