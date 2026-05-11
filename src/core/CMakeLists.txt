cmake_minimum_required(VERSION 3.12)
cmake_policy(VERSION 3.12)

# Long-term shell migration seams live under ui/ and view/render/ and stay part of the core target.
file(GLOB_RECURSE vertexnote_sources CONFIGURE_DEPENDS *.cpp *.h)

if(NOT ENABLE_AUDIO)
    list(FILTER vertexnote_sources EXCLUDE REGEX "${PROJECT_SOURCE_DIR}/src/core/audio/.*" )
    list(FILTER vertexnote_sources EXCLUDE REGEX "${PROJECT_SOURCE_DIR}/src/core/control/AudioController.cpp" )
endif()
set(vertexnote_include_dirs "${CMAKE_CURRENT_SOURCE_DIR}")

add_library(vertexnote-core STATIC ${vertexnote_sources})
target_compile_features(vertexnote-core PUBLIC ${PROJECT_CXX_FEATURES})
target_compile_features(vertexnote-core PUBLIC cxx_std_20)
target_link_libraries(vertexnote-core
        PRIVATE vertexnote::util
                Qt6::Gui
                Qt6::Network
                $<$<PLATFORM_ID:Windows>:winhttp>
        PUBLIC vertexnote::defaults vertexnote::external_common_modules
        )
target_include_directories(vertexnote-core PUBLIC ${vertexnote_include_dirs})
add_library(xoj::core ALIAS vertexnote-core)
add_library(vertexnote::core ALIAS vertexnote-core)
