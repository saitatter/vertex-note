cmake_minimum_required(VERSION 3.13)
cmake_policy(VERSION 3.13)

# Todo: add to target by generator expression


##############################
# xoj default option target
##############################

add_library(defaults INTERFACE)
target_include_directories(defaults INTERFACE ${PROJECT_BINARY_DIR}/src)
add_library(xoj::defaults ALIAS defaults)
add_library(vertexnote::defaults ALIAS defaults)
if (MSVC) # clang-cl.exe, cl.exe
  target_compile_definitions(defaults INTERFACE NOMINMAX WIN32_LEAN_AND_MEAN)
  target_compile_options(defaults INTERFACE -utf-8)
else(MSVC) # gnu clang with gnu driver, 
  target_compile_options(defaults INTERFACE -Wall -Wreturn-type -Wuninitialized -Wunused-value -Wunused-variable -Wconversion)
endif ()
# Todo (support-v3.10): guard this variable with cmake_dependent_option for CMake 3.10, and NOT(GCC)
# then downgrade cmake_minimum_required to CMake 3.10
if (DEV_ENABLE_GCOV)
  target_compile_options(defaults INTERFACE --coverage)
  target_link_options(defaults INTERFACE --coverage) #cmake v3.13
endif (DEV_ENABLE_GCOV)

##############################
# util
##############################

add_subdirectory(util)

##############################
# vertexnote-core
##############################

add_subdirectory(core)

##############################
# vertex-note-wrapper: catches crashes
##############################

if (ENABLE_LEGACY_GTK_SHELL)
  # The crash dialog's ui file is stored as gresource to make sure the wrapper is standalone
  find_program(GLIB_COMPILE_RESOURCES NAMES glib-compile-resources REQUIRED)

  set(XOURNAL_WRAPPER_GRESOURCE_C   vertex-note-wrapper.gresource.c)
  set(XOURNAL_WRAPPER_GRESOURCE_XML exe/vertex-note-wrapper.gresource.xml)

  add_custom_command(
      OUTPUT ${XOURNAL_WRAPPER_GRESOURCE_C}
      WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
      COMMAND ${GLIB_COMPILE_RESOURCES}
      ARGS
          --generate-source
          --sourcedir=${CMAKE_SOURCE_DIR}
          --target=${CMAKE_CURRENT_BINARY_DIR}/${XOURNAL_WRAPPER_GRESOURCE_C}
          ${XOURNAL_WRAPPER_GRESOURCE_XML}
      VERBATIM
      MAIN_DEPENDENCY ${XOURNAL_WRAPPER_GRESOURCE_XML}
      DEPENDS
          ${CMAKE_SOURCE_DIR}/ui/crashDialog.glade
  )

  add_custom_target(
      vertexnote-wrapper-resource
      DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/${XOURNAL_WRAPPER_GRESOURCE_C}
  )

  add_executable(vertexnote-wrapper exe/VertexNote-wrapper.cpp ${CMAKE_CURRENT_BINARY_DIR}/${XOURNAL_WRAPPER_GRESOURCE_C})
  set_source_files_properties(
      ${CMAKE_CURRENT_BINARY_DIR}/${XOURNAL_WRAPPER_GRESOURCE_C}
      PROPERTIES GENERATED TRUE
  )

  add_dependencies(vertexnote-wrapper vertexnote-wrapper-resource)
  target_link_libraries(vertexnote-wrapper
          PUBLIC vertexnote::defaults vertexnote::util vertexnote::external_common_modules vertexnote::external_gtk_modules)
  set_target_properties(vertexnote-wrapper PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
          OUTPUT_NAME "${VERTEX_NOTE_WRAPPER_BINARY_NAME}"
          MACOSX_BUNDLE $<IF:$<PLATFORM_ID:Darwin>,TRUE,${CMAKE_MACOSX_BUNDLE}>
          WIN32_EXECUTABLE $<IF:$<PLATFORM_ID:Windows>,TRUE,${CMAKE_WIN32_EXECUTABLE}>
          )
  target_include_directories(vertexnote-wrapper PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")


  ##############################
  # vertexnote main program
  ##############################

  add_executable(vertexnote exe/VertexNote.cpp)
  target_link_libraries(vertexnote PRIVATE vertexnote::core vertexnote::util)
  set_target_properties(vertexnote PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
          OUTPUT_NAME "${VERTEX_NOTE_BINARY_NAME}"
          MACOSX_BUNDLE $<IF:$<PLATFORM_ID:Darwin>,TRUE,${CMAKE_MACOSX_BUNDLE}>
          WIN32_EXECUTABLE $<IF:$<PLATFORM_ID:Windows>,TRUE,${CMAKE_WIN32_EXECUTABLE}>
          )
  if (WIN32)
    # Make sure Windows executable has all symbols
    target_link_options(vertexnote PRIVATE "-Wl,--export-all-symbols")
  endif()
endif ()

###############################
# Generate icons
# Todo(fabian): move this into an own windows specific cmake script
# e.g. exe/win32/CMakeLists.txt
###############################

###############################
# Add MacOS specific files
# Todo(Roland): consider moving this into an own MacOS specific cmake script
# e.g. exe/macos/CMakeLists.txt
##############################

if (ENABLE_LEGACY_GTK_SHELL AND APPLE)
  # MacOS specific source files
  file (GLOB vertexnote-sources-osx exe/osx/*.cpp exe/osx/*.h)
  target_sources(vertexnote PRIVATE ${vertexnote-sources-osx})
  target_include_directories(vertexnote PRIVATE exe)
endif()

if (ENABLE_LEGACY_GTK_SHELL AND WIN32)
  # generate .ico from .png
  find_package(ImageMagick REQUIRED COMPONENTS magick)
  set (ICON_SIZES 16 32 48 256)
  foreach (SIZE IN LISTS ICON_SIZES)
    set (RESIZED_ICON ${PROJECT_BINARY_DIR}/vertex-note-icon-${SIZE}.png)
    add_custom_command (
                OUTPUT ${RESIZED_ICON}
                COMMAND rsvg-convert ${PROJECT_SOURCE_DIR}/ui/pixmaps/app.vertexnote.VertexNote.svg --width=${SIZE} --height=${SIZE} -o ${RESIZED_ICON}
	)
  set (ICON_DEPS ${ICON_DEPS} ${RESIZED_ICON})
  unset (RESIZED_ICON)
  endforeach()
  add_custom_command (
    OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/win32/vertex-note.ico
                COMMAND magick ${ICON_DEPS} ${CMAKE_CURRENT_BINARY_DIR}/win32/vertex-note.ico
                DEPENDS ${ICON_DEPS}
  )
  unset(ICON_SIZES)
  unset(ICON_DEPS)
  add_custom_target(vertexnote_icon DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/win32/vertex-note.ico)
  add_dependencies(vertexnote vertexnote_icon)
  add_dependencies(vertexnote-wrapper vertexnote_icon)
  
  # windows specific source files
  target_sources(vertexnote PRIVATE exe/win32/console.cpp)
  target_sources(vertexnote-wrapper PRIVATE exe/win32/console.cpp)
  if (MSVC)
    target_sources(vertexnote PRIVATE exe/win32/gui_main.cpp)
    target_sources(vertexnote-wrapper PRIVATE exe/win32/gui_main.cpp)
  endif()
  
  # windows specific appinfo
  if(NOT MSVC)
    set(CMAKE_RC_COMPILE_OBJECT "<CMAKE_RC_COMPILER> -O coff -i <SOURCE> -o <OBJECT>")
    set(CMAKE_RC_COMPILER_INIT windres)
  endif()
  enable_language(RC)

  configure_file(exe/win32/xpp.rc.in ${CMAKE_CURRENT_BINARY_DIR}/win32/xpp.rc)
  target_sources(vertexnote PRIVATE ${CMAKE_CURRENT_BINARY_DIR}/win32/xpp.rc)
  target_sources(vertexnote-wrapper PRIVATE ${CMAKE_CURRENT_BINARY_DIR}/win32/xpp.rc)
endif ()
# Until here
################################

if (ENABLE_LEGACY_GTK_SHELL)
  install(TARGETS vertexnote-wrapper
          RUNTIME DESTINATION bin
          COMPONENT vertex-note-wrapper
          )
  install(TARGETS vertexnote
          RUNTIME DESTINATION bin
          COMPONENT vertex-note
          )
endif ()

if (ENABLE_QT_SHELL)
  add_subdirectory(qt)
endif ()

if (ENABLE_LEGACY_GTK_SHELL)
  add_subdirectory(xoj-preview-extractor)
endif ()
