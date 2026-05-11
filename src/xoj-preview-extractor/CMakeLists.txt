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
  Qt6::Gui
  ${ZLIB_LIBRARIES}
  ${ZIP_LDFLAGS}
  )

set_target_properties(vertexnote-thumbnailer PROPERTIES OUTPUT_NAME "vertex-note-thumbnailer")

## Install ##

install (TARGETS vertexnote-thumbnailer
  RUNTIME DESTINATION bin
  COMPONENT vertex-note-thumbnailer
)

