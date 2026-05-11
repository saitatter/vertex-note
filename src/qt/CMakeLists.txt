set(vertexnote_qt_sources
        main.cpp
        app/QtAppShell.h
        app/QtAppShell.cpp
        app/QtAppShellAudioActions.cpp
        app/QtAppShellCommandStates.cpp
        app/QtAppShellCommands.cpp
        app/QtAppShellConstraintActions.cpp
        app/QtAppShellDialogs.cpp
        app/QtAppShellDocumentIo.cpp
        app/QtAppShellEditCommands.cpp
        app/QtAppShellEditing.cpp
        app/QtAppShellFileCommands.cpp
        app/QtAppShellHelpCommands.cpp
        app/QtAppShellInsertActions.cpp
        app/QtAppShellJournalCommands.cpp
        app/QtAppShellLayerActions.cpp
        app/QtAppShellNavigationCommands.cpp
        app/QtAppShellNavigation.cpp
        app/QtAppShellPageActions.cpp
        app/QtAppShellPageDialogs.cpp
        app/QtAppShellPageExtraActions.cpp
        app/QtAppShellPdfActions.cpp
        app/QtAppShellPersistentStateLoad.cpp
        app/QtAppShellPersistentStateSave.cpp
        app/QtAppShellSettings.cpp
        app/QtAppShellStatus.cpp
        app/QtAppShellToolbar.cpp
        app/QtAppShellToolbarSync.cpp
        app/QtAppShellToolbarDialogs.cpp
        app/QtAppShellToolbarTokens.cpp
        app/QtAppShellToolbarWidgets.cpp
        app/QtAppShellToolStyleActions.cpp
        app/QtAppShellToolCommands.cpp
        app/QtAppShellUpdateActions.cpp
        app/QtAppShellViewActions.cpp
        app/QtAppShellViewCommands.cpp
        app/QtAppShellWindowState.cpp
        canvas/QtCanvas.h
        canvas/QtCanvas.cpp
        canvas/QtCanvasSelection.cpp
        canvas/QtCanvasLayout.h
        canvas/QtCanvasLayout.cpp
        canvas/QtPageContentRenderer.h
        canvas/QtPageContentRenderer.cpp
        canvas/QtPreviewBackgroundRenderer.h
        canvas/QtPreviewBackgroundRenderer.cpp
        canvas/QtPreviewGeometryRenderer.h
        canvas/QtPreviewGeometryRenderer.cpp
        canvas/QtPreviewImageRenderer.h
        canvas/QtPreviewImageRenderer.cpp
        canvas/QtPreviewStrokeRenderer.h
        canvas/QtPreviewStrokeRenderer.cpp
        canvas/QtPreviewTextRenderer.h
        canvas/QtPreviewTextRenderer.cpp
        document/QtDocumentController.h
        document/QtDocumentController.cpp
        document/QtDocumentControllerEraser.cpp
        document/QtDocumentControllerHistory.cpp
        document/QtDocumentControllerLayers.cpp
        document/QtDocumentControllerPageBackground.cpp
        document/QtDocumentControllerPages.cpp
        document/QtDocumentControllerPdfText.cpp
        document/QtDocumentControllerSelection.cpp
        document/QtDocumentControllerShapes.cpp
        document/QtDocumentSession.h
        document/QtDocumentSession.cpp
        document/QtDocumentExporter.h
        document/QtDocumentExporter.cpp
        services/QtAudioController.h
        services/QtAudioController.cpp
        services/QtClipboardService.h
        services/QtClipboardService.cpp
        services/QtCommandHost.h
        services/QtCommandHost.cpp
        services/QtDialogService.h
        services/QtDialogService.cpp
        services/QtRecentFilesService.h
        services/QtRecentFilesService.cpp
        services/QtPluginUiBridge.h
        services/QtPluginUiBridge.cpp
        services/QtLuaPluginRuntime.h
        services/QtLuaPluginRuntime.cpp
        services/QtUpdatePresentationService.h
        services/QtUpdatePresentationService.cpp
        services/QtInputAdapter.h
        services/QtInputAdapter.cpp
        services/QtInputDeviceKey.h
        toolbar/QtColorPalette.h
        toolbar/QtColorPalette.cpp
        toolbar/QtIconResources.h
        toolbar/QtIconResources.cpp
        toolbar/QtToolFamilies.h
        toolbar/QtToolFamilies.cpp
        toolbar/QtToolState.h
        toolbar/QtToolbarLayoutEngine.h
        toolbar/QtToolbarLayoutEngine.cpp
        toolbar/QtToolbarProfileStore.h
        toolbar/QtToolbarProfileStore.cpp
        widgets/QtMainWindow.h
        widgets/QtMainWindow.cpp
        widgets/QtBackgroundDialog.h
        widgets/QtBackgroundDialog.cpp
        widgets/QtLayerPanel.h
        widgets/QtLayerPanel.cpp
        widgets/QtPageSidebar.h
        widgets/QtPageSidebar.cpp
        widgets/QtToolPalette.h
        widgets/QtToolPalette.cpp
        widgets/QtTextEditor.h
        widgets/QtTextEditor.cpp
        widgets/QtSettingsDialog.h
        widgets/QtSettingsDialog.cpp
)

add_executable(vertexnote-qt-shell ${vertexnote_qt_sources})
target_compile_definitions(vertexnote-qt-shell PRIVATE QT_NO_KEYWORDS)
target_link_libraries(vertexnote-qt-shell PRIVATE Qt6::Widgets Qt6::PrintSupport vertexnote::defaults vertexnote::util vertexnote::core $<$<TARGET_EXISTS:Lua::lua>:Lua::lua>)
target_include_directories(vertexnote-qt-shell PRIVATE
        "${PROJECT_SOURCE_DIR}/src/core"
        "${PROJECT_SOURCE_DIR}/src"
        "${CMAKE_CURRENT_SOURCE_DIR}"
        "${CMAKE_CURRENT_SOURCE_DIR}/app"
        "${CMAKE_CURRENT_SOURCE_DIR}/canvas"
        "${CMAKE_CURRENT_SOURCE_DIR}/document"
        "${CMAKE_CURRENT_SOURCE_DIR}/services"
        "${CMAKE_CURRENT_SOURCE_DIR}/toolbar"
        "${CMAKE_CURRENT_SOURCE_DIR}/widgets"
)
set_target_properties(vertexnote-qt-shell PROPERTIES
        AUTOMOC ON
        AUTOUIC ON
        AUTORCC ON
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
        OUTPUT_NAME "vertex-note-qt-shell"
)

install(TARGETS vertexnote-qt-shell
        RUNTIME DESTINATION bin
        BUNDLE DESTINATION .
        COMPONENT vertex-note
)
