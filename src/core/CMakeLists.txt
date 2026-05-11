cmake_minimum_required(VERSION 3.12)
cmake_policy(VERSION 3.12)

# Long-term shell migration seams live under ui/ and view/render/ and stay part of the core target.
file(GLOB_RECURSE vertexnote_sources CONFIGURE_DEPENDS *.cpp *.h)
include(${PROJECT_SOURCE_DIR}/src/legacy/LegacyBoundaries.cmake)

if(NOT ENABLE_AUDIO)
    list(FILTER vertexnote_sources EXCLUDE REGEX "${PROJECT_SOURCE_DIR}/src/core/audio/.*" )
    list(FILTER vertexnote_sources EXCLUDE REGEX "${PROJECT_SOURCE_DIR}/src/core/control/AudioController.cpp" )
endif()
set(vertexnote_include_dirs "${CMAKE_CURRENT_SOURCE_DIR}")

vertexnote_match_legacy_sources(vertexnote_sources vertexnote_legacy_gtk_sources ${VERTEXNOTE_LEGACY_GTK_CORE_PATTERNS})
if (vertexnote_legacy_gtk_sources)
    list(REMOVE_ITEM vertexnote_sources ${vertexnote_legacy_gtk_sources})
endif ()

add_library(vertexnote-core STATIC ${vertexnote_sources})
target_compile_features(vertexnote-core PUBLIC ${PROJECT_CXX_FEATURES})
target_compile_features(vertexnote-core PUBLIC cxx_std_20)
target_link_libraries(vertexnote-core
        PRIVATE vertexnote::util
                $<$<PLATFORM_ID:Windows>:winhttp>
        PUBLIC vertexnote::defaults vertexnote::external_common_modules
        )
target_include_directories(vertexnote-core PUBLIC ${vertexnote_include_dirs})
add_library(xoj::core ALIAS vertexnote-core)
add_library(vertexnote::core ALIAS vertexnote-core)

vertexnote_match_legacy_sources(vertexnote_sources vertexnote_legacy_render_sources ${VERTEXNOTE_LEGACY_RENDER_CORE_PATTERNS})

if (vertexnote_legacy_gtk_sources)
    source_group("legacy\\gtk" FILES ${vertexnote_legacy_gtk_sources})
endif ()

if (vertexnote_legacy_render_sources)
    source_group("legacy\\render" FILES ${vertexnote_legacy_render_sources})
endif ()

list(LENGTH vertexnote_legacy_gtk_sources vertexnote_legacy_gtk_count)
list(LENGTH vertexnote_legacy_render_sources vertexnote_legacy_render_count)
message(STATUS "VertexNote legacy GTK core surface: ${vertexnote_legacy_gtk_count} files (removed/excluded)")
message(STATUS "VertexNote legacy Cairo core surface: ${vertexnote_legacy_render_count} files")
