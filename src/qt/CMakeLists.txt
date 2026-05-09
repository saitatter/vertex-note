set(vertexnote_qt_sources
        main.cpp
        QtExperimentalAppShell.h
        QtExperimentalAppShell.cpp
        QtExperimentalCanvas.h
        QtExperimentalCanvas.cpp
        QtExperimentalCommandHost.h
        QtExperimentalCommandHost.cpp
        QtExperimentalMainWindow.h
        QtExperimentalMainWindow.cpp
)

add_executable(vertexnote-qt-experimental ${vertexnote_qt_sources})
target_link_libraries(vertexnote-qt-experimental PRIVATE Qt6::Widgets vertexnote::defaults vertexnote::util)
target_include_directories(vertexnote-qt-experimental PRIVATE
        "${PROJECT_SOURCE_DIR}/src/core"
        "${PROJECT_SOURCE_DIR}/src"
)
set_target_properties(vertexnote-qt-experimental PROPERTIES
        AUTOMOC ON
        AUTOUIC ON
        AUTORCC ON
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
        OUTPUT_NAME "vertex-note-qt-experimental"
)

if (WIN32)
    target_sources(vertexnote-qt-experimental PRIVATE "${PROJECT_SOURCE_DIR}/src/exe/win32/console.cpp")
endif ()
