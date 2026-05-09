## vertexnote-thumbnailer executable ##

if (WIN32)
  # Windows does not support the thumbnailer like this
  return ()
endif ()

add_definitions (-DBUILD_THUMBNAILER)

if (DEV_ENABLE_GCOV)
  add_compile_options("--coverage")
  add_link_options("--coverage")
endif (DEV_ENABLE_GCOV)

add_executable (vertexnote-thumbnailer
        vertex-note-thumbnailer.cpp
        "${PROJECT_SOURCE_DIR}/src/util/GzUtil.cpp"
        "${PROJECT_SOURCE_DIR}/src/util/PlaceholderString.cpp"
        "${PROJECT_SOURCE_DIR}/src/util/StringUtils.cpp"
        "${PROJECT_SOURCE_DIR}/src/util/PathUtil.cpp"
        "${PROJECT_SOURCE_DIR}/src/util/Stacktrace.cpp"
        "${PROJECT_SOURCE_DIR}/src/util/LegacyXojPreviewExtractor.cpp"
        )

target_compile_features (vertexnote-thumbnailer PRIVATE cxx_std_20)
target_link_libraries (vertexnote-thumbnailer
  util
  ${ZLIB_LIBRARIES}
  ${librsvg_LDFLAGS}
  ${Glib_LDFLAGS}
  ${ZIP_LDFLAGS}
  )

target_include_directories (vertexnote-thumbnailer PRIVATE ${librsvg_INCLUDE_DIRS})
set_target_properties(vertexnote-thumbnailer PROPERTIES OUTPUT_NAME "vertex-note-thumbnailer")

## Install ##

install (TARGETS vertexnote-thumbnailer
  RUNTIME DESTINATION bin
  COMPONENT vertex-note-thumbnailer
)

## Desktop install targets ##

configure_file (vertex-note-thumbnailer-xoj.schemas vertex-note-thumbnailer-xoj.schemas COPYONLY)

add_custom_target (desktop-install-thumbnailer
  COMMAND "${PROJECT_SOURCE_DIR}/src/xoj-preview-extractor/desktop-install.sh" install

  COMMENT "Install vertex-note-thumbnailer as xoj file thumbnail generator"
  SOURCES vertex-note-thumbnailer.cpp vertex-note-thumbnailer-xoj.schemas desktop-install.sh
)

add_custom_target (desktop-uninstall-thumbnailer
  COMMAND "${PROJECT_SOURCE_DIR}/src/xoj-preview-extractor/desktop-install.sh" uninstall

  COMMENT "Uninstall vertex-note-thumbnailer as xoj file thumbnail generator"
  SOURCES vertex-note-thumbnailer.cpp vertex-note-thumbnailer-xoj.schemas desktop-install.sh
)
