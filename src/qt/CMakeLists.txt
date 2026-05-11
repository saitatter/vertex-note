set(vertexnote_qt_sources
        main.cpp
        QtAudioController.h
        QtAudioController.cpp
        QtAppShell.h
        QtAppShell.cpp
        QtAppShellStatus.cpp
        QtAppShellToolbarSync.cpp
        QtBackgroundDialog.h
        QtBackgroundDialog.cpp
        QtCanvas.h
        QtCanvas.cpp
        QtCanvasSelection.cpp
        QtCanvasLayout.h
        QtCanvasLayout.cpp
        QtColorPalette.h
        QtColorPalette.cpp
        QtClipboardService.h
        QtClipboardService.cpp
        QtCommandHost.h
        QtCommandHost.cpp
        QtDialogService.h
        QtDialogService.cpp
        QtDocumentController.h
        QtDocumentController.cpp
        QtDocumentControllerEraser.cpp
        QtDocumentControllerPdfText.cpp
        QtDocumentSession.h
        QtDocumentSession.cpp
        QtMainWindow.h
        QtMainWindow.cpp
        QtPluginUiBridge.h
        QtPluginUiBridge.cpp
        QtLuaPluginRuntime.h
        QtLuaPluginRuntime.cpp
        QtPreviewBackgroundRenderer.h
        QtPreviewBackgroundRenderer.cpp
        QtPreviewGeometryRenderer.h
        QtPreviewGeometryRenderer.cpp
        QtPreviewImageRenderer.h
        QtPreviewImageRenderer.cpp
        QtPreviewStrokeRenderer.h
        QtPreviewStrokeRenderer.cpp
        QtPreviewTextRenderer.h
        QtPreviewTextRenderer.cpp
        QtRecentFilesService.h
        QtRecentFilesService.cpp
        QtToolState.h
        QtToolFamilies.h
        QtToolFamilies.cpp
        QtInputDeviceKey.h
        QtUpdatePresentationService.h
        QtUpdatePresentationService.cpp
        QtInputAdapter.h
        QtInputAdapter.cpp
        QtIconResources.h
        QtIconResources.cpp
        QtLayerPanel.h
        QtLayerPanel.cpp
        QtPageContentRenderer.h
        QtPageContentRenderer.cpp
        QtPageSidebar.h
        QtPageSidebar.cpp
        QtDocumentExporter.h
        QtDocumentExporter.cpp
        QtToolPalette.h
        QtToolPalette.cpp
        QtTextEditor.h
        QtTextEditor.cpp
        QtToolbarLayoutEngine.h
        QtToolbarLayoutEngine.cpp
        QtToolbarProfileStore.h
        QtToolbarProfileStore.cpp
        QtSettingsDialog.h
        QtSettingsDialog.cpp
)

add_executable(vertexnote-qt-shell ${vertexnote_qt_sources})
target_compile_definitions(vertexnote-qt-shell PRIVATE QT_NO_KEYWORDS)
target_link_libraries(vertexnote-qt-shell PRIVATE Qt6::Widgets Qt6::PrintSupport vertexnote::defaults vertexnote::util vertexnote::core $<$<TARGET_EXISTS:Lua::lua>:Lua::lua>)
target_include_directories(vertexnote-qt-shell PRIVATE
        "${PROJECT_SOURCE_DIR}/src/core"
        "${PROJECT_SOURCE_DIR}/src"
)
set_target_properties(vertexnote-qt-shell PROPERTIES
        AUTOMOC ON
        AUTOUIC ON
        AUTORCC ON
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
        OUTPUT_NAME "vertex-note-qt-shell"
)

if (WIN32)
    target_sources(vertexnote-qt-shell PRIVATE "${PROJECT_SOURCE_DIR}/src/exe/win32/console.cpp")
endif ()
