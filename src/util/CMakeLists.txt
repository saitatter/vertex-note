cmake_minimum_required(VERSION 3.12)
cmake_policy(VERSION 3.12)

# Util is pretty small so GLOB_RECURSE is sufficient
unset(util_SOURCES)
file(GLOB_RECURSE util_SOURCES *.cpp *.h)
include(${PROJECT_SOURCE_DIR}/src/legacy/LegacyBoundaries.cmake)

add_library(util STATIC ${util_SOURCES})
target_link_libraries(util PUBLIC vertexnote::defaults vertexnote::external_modules)
target_compile_features(util PUBLIC ${PROJECT_CXX_FEATURES})
target_compile_features(util PUBLIC cxx_std_20)
target_include_directories(util PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>)
add_library(xoj::util ALIAS util)
add_library(vertexnote::util ALIAS util)

vertexnote_match_legacy_sources(util_SOURCES util_legacy_gtk_sources ${VERTEXNOTE_LEGACY_GTK_UTIL_PATTERNS})
vertexnote_match_legacy_sources(util_SOURCES util_legacy_render_sources ${VERTEXNOTE_LEGACY_RENDER_UTIL_PATTERNS})

if (util_legacy_gtk_sources)
    source_group("legacy\\gtk" FILES ${util_legacy_gtk_sources})
endif ()

if (util_legacy_render_sources)
    source_group("legacy\\render" FILES ${util_legacy_render_sources})
endif ()

list(LENGTH util_legacy_gtk_sources util_legacy_gtk_count)
list(LENGTH util_legacy_render_sources util_legacy_render_count)
message(STATUS "VertexNote legacy GTK util surface: ${util_legacy_gtk_count} files")
message(STATUS "VertexNote legacy Cairo util surface: ${util_legacy_render_count} files")
