cmake_minimum_required(VERSION 3.18)
cmake_policy(VERSION ${CMAKE_VERSION})

## The CMAKE_OSX_DEPLOYMENT_TARGET must be set before project(), so don't move the following lines further down.
if(APPLE)
    if (NOT CMAKE_OSX_DEPLOYMENT_TARGET)
        # Use the same target as the dependencies shipped by the CI
        file(STRINGS mac-setup/jhbuild-version.lock _DEPLOY_TARGET REGEX "^deployment_target=.*")
        string(REGEX REPLACE "^[^=]*=" "" _DEPLOY_TARGET_VALUE ${_DEPLOY_TARGET})
        set(CMAKE_OSX_DEPLOYMENT_TARGET "${_DEPLOY_TARGET_VALUE}" CACHE STRING "Minimum OS X deployment version" FORCE)
    endif()
    message(STATUS "MacOS X Deployment target: ${CMAKE_OSX_DEPLOYMENT_TARGET}")
endif()


if(NOT CMAKE_BUILD_TYPE)
    # set default build type to RelWithDebInfo, we never supported multi config generators, therefore this is fine
    set(CMAKE_BUILD_TYPE "RelWithDebInfo" CACHE STRING "Choose the type of build, options are: Debug Release RelWithDebInfo MinSizeRel." FORCE)
endif()

## Project related variables
file(STRINGS pyproject.toml VERTEX_NOTE_VERSION_LINE
        REGEX "^[ ]*version[ ]*=[ ]*\"[0-9]+\\.[0-9]+\\.[0-9]+\"")
if(NOT VERTEX_NOTE_VERSION_LINE)
    message(FATAL_ERROR "Could not extract VertexNote version from pyproject.toml")
endif()
list(GET VERTEX_NOTE_VERSION_LINE 0 VERTEX_NOTE_VERSION_LINE)
string(REGEX REPLACE "^[ ]*version[ ]*=[ ]*\"([0-9]+\\.[0-9]+\\.[0-9]+)\".*$" "\\1"
        VERTEX_NOTE_VERSION "${VERTEX_NOTE_VERSION_LINE}")
if(NOT VERTEX_NOTE_VERSION)
    message(FATAL_ERROR "Parsed VertexNote version is empty")
endif()

project(VertexNote
        VERSION ${VERTEX_NOTE_VERSION}
        DESCRIPTION "VertexNote - CAD-inspired technical note-taking"
        HOMEPAGE_URL "https://github.com/saitatter/vertex-note"
        LANGUAGES CXX C)
# according to https://cmake.org/cmake/help/latest/command/project.html we must fix the suffix:
# And https://gitlab.kitware.com/cmake/cmake/-/issues/16716 is still open:
set(VERSION_SUFFIX "+dev")
set(PROJECT_VERSION "${PROJECT_VERSION}${VERSION_SUFFIX}")
set(vertexnote_VERSION ${PROJECT_VERSION})
set(CMAKE_PROJECT_VERSION ${PROJECT_VERSION})
set(PROJECT_CONTACT "saitatter@users.noreply.github.com")
set(PROJECT_ISSUES_URL "https://github.com/saitatter/vertex-note/issues")
set(VERTEX_NOTE_APP_ID "app.vertexnote.VertexNote")
set(VERTEX_NOTE_BINARY_NAME "vertex-note")
set(VERTEX_NOTE_WRAPPER_BINARY_NAME "vertex-note-wrapper")
set(VERTEX_NOTE_RESOURCE_DIR "vertex-note")

## CMAKE_Variables
set(CMAKE_MODULE_PATH "${PROJECT_SOURCE_DIR}/cmake/find" "${PROJECT_SOURCE_DIR}/cmake/include")
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON) # enable clang tidy to use correct includes
set_directory_properties(PROPERTIES EP_BASE "${PROJECT_BINARY_DIR}/external")

add_definitions(-D_USE_MATH_DEFINES)

include(CheckCXXCompilerFlag)
if (CMAKE_GENERATOR STREQUAL "Ninja")
    CHECK_CXX_COMPILER_FLAG("-fcolor-diagnostics" CLANG_HAS_COLOR)
    CHECK_CXX_COMPILER_FLAG("-fdiagnostics-color=always" GNU_HAS_COLOR)
    if(CLANG_HAS_COLOR)
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fcolor-diagnostics")
    elseif(GNU_HAS_COLOR)
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fdiagnostics-color=always")
    endif()
endif()

# package version
include(Version)
core_find_git_rev(RELEASE_IDENTIFIER)
string(TIMESTAMP PACKAGE_TIMESTAMP "%Y%m%d.%H%M" UTC)

set(PACKAGE_ARCH ${CMAKE_SYSTEM_PROCESSOR})


if (UNIX AND NOT APPLE AND NOT CYGWIN)
    set(INSTALL_DESKTOP_FILES true)
endif()

## I18n ##
add_subdirectory(po)
option (ONLY_CONFIGURE_FOR_TARGET_POT "Only configure the pot target" OFF)
if (ONLY_CONFIGURE_FOR_TARGET_POT)
    # We dont need anything else for just generating the .pot file.
    # This avoids having to install all the dependencies in the CI runs updating the translation files
    return()
endif ()

configure_file(cmake/VERSION.in VERSION)

configure_file(
        cmake/postinst.in
        cmake/postinst
        @ONLY
)

find_package(PkgConfig REQUIRED)

set(PROJECT_INSTALL_DIR "${CMAKE_INSTALL_PREFIX}")

# Git repo info
include(GitRepo)

################################
# CMake os-fixups
# this section should decrease in size for newer CMake versions.
################################

## Libraries ##
##############################

option(ENABLE_CPPTRACE "Compile with cpptrace stacktrace library" ON)
# Requires fetching content during build.
if (CMAKE_BUILD_TYPE STREQUAL "Release")
  set(ENABLE_CPPTRACE OFF)
  message(STATUS "Disable cpptrace by lack of debug symbols in Release mode")
endif ()

if (ENABLE_CPPTRACE)
    include(FetchContent)
    FetchContent_Declare(
        cpptrace
        GIT_REPOSITORY https://github.com/jeremy-rifkin/cpptrace.git
        GIT_TAG        v1.0.4 # <HASH or TAG>
        EXCLUDE_FROM_ALL
    )
    set(FETCHCONTENT_UPDATES_DISCONNECTED ON) # Prevent reloading if already downloaded
    set(CPPTRACE_BUILD_SHARED OFF)
    set(CPPTRACE_USE_EXTERNAL_ZSTD ON)
    if (UNIX)
        # Otherwise uses libdwarf - adding yet another dependency
        set(CPPTRACE_GET_SYMBOLS_WITH_LIBDL ON)
    endif()
    if (${CMAKE_VERSION} VERSION_GREATER_EQUAL "3.28")
        FetchContent_MakeAvailable(cpptrace)
    else()
        # FetchContent_Declare option EXCLUDE_FROM_ALL was only introduced in cmake 3.28
        # Workaround it for older versions (ubuntu 22 has cmake 3.22)
        FetchContent_GetProperties(cpptrace)
        if(NOT cpptrace_POPULATED)
            FetchContent_Populate(cpptrace)
            add_subdirectory(${cpptrace_SOURCE_DIR} ${cpptrace_BINARY_DIR} EXCLUDE_FROM_ALL)
        endif()
    endif()
endif()


##############################
# other externals
##############################

set(common-pkg-module-spec "")
list(APPEND common-pkg-module-spec "glib-2.0 >= 2.32.0")
list(APPEND common-pkg-module-spec "gthread-2.0 >= 2.4.0")
list(APPEND common-pkg-module-spec "gio-2.0 >= 2.32.0")
list(APPEND common-pkg-module-spec "libxml-2.0 >= 2.0.0")
list(APPEND common-pkg-module-spec "libzip >= 1.0.1")
list(APPEND common-pkg-module-spec "poppler-glib >= 0.41.0")
list(APPEND common-pkg-module-spec "poppler-cpp >= 0.41.0")

# Profiling
option (ENABLE_PROFILING "Link with gperftools" OFF)
if (ENABLE_PROFILING)
  list(APPEND common-pkg-module-spec "libprofiler >= 2.5")
  list(APPEND common-pkg-module-spec "libtcmalloc >= 2.5")
endif (ENABLE_PROFILING)

pkg_check_modules(ExternalCommonModules REQUIRED IMPORTED_TARGET ${common-pkg-module-spec})

find_package(ZLIB REQUIRED)
find_package(Threads REQUIRED)

option(ENABLE_AUDIO "Compile with Audio recording/playing support" ON)
if (ENABLE_AUDIO)
    message(STATUS "Enable Audio support")
    pkg_check_modules(ExternalAudioModules REQUIRED IMPORTED_TARGET "portaudiocpp >= 12;sndfile >= 1.0.25")
else ()
    message(STATUS "Disable Audio support")
endif ()

option(ENABLE_QPDF "Compile with QPDF export support" ON)
if (ENABLE_QPDF)
    find_package(qpdf)
    if (NOT qpdf_FOUND)
        # Before version 11.0 (e.g. for ubuntu 22.04 LTS), qpdf did not provide qpdfConfig.cmake (and friends)
        # We cannot only use pkg_search_module() though: it fails to link on MacOS
        pkg_search_module(qpdf REQUIRED "libqpdf >= 10.6.0")
        add_library(qpdf_tgt INTERFACE)
        target_link_libraries(qpdf_tgt INTERFACE ${qpdf_LIBRARIES})
        target_include_directories(qpdf_tgt INTERFACE ${qpdf_INCLUDE_DIRS})
        add_library(qpdf::libqpdf ALIAS qpdf_tgt)
    endif ()
    message(STATUS "Found qpdf: ${qpdf_VERSION}")
endif ()

option(ENABLE_PLUGINS "Compile with plugin support" ON)
if (ENABLE_PLUGINS)
    message(STATUS "Enable VertexNote Plugins")
    find_package(Lua REQUIRED)
    add_library(lua INTERFACE)
    target_link_libraries(lua INTERFACE ${LUA_LIBRARIES})
    target_include_directories(lua INTERFACE ${LUA_INCLUDE_DIR})
    add_library(Lua::lua ALIAS lua)
else ()
    message(STATUS "Disable VertexNote Plugins")
endif ()

option(ENABLE_QT_SHELL "Build the Qt Widgets shell" ON)
if (NOT ENABLE_QT_SHELL)
    message(FATAL_ERROR "The GTK shell has been removed; ENABLE_QT_SHELL must remain enabled.")
endif ()

if (ENABLE_QT_SHELL)
    find_package(Qt6 REQUIRED COMPONENTS Gui Widgets PrintSupport)
    message(STATUS "Enable Qt Widgets shell")
else ()
    message(STATUS "Disable Qt Widgets shell")
endif ()

option(ENABLE_FLOAT_FROM_CHARS "Use floating point std::from_chars for parsing doubles" ON)
include(CheckCXXSourceCompiles)
check_cxx_source_compiles([[
    #include <charconv>
    #include <string_view>
    #include <system_error>
    int main() {
        const std::string_view sv = "7.381450";
        double val{};
        auto [ptr, ec] = std::from_chars(sv.data(), sv.data() + sv.size(), val);
        return ptr != (sv.data() + sv.size()) || ec != std::errc{};
    }
]] HAVE_FLOAT_FROM_CHARS)
if (HAVE_FLOAT_FROM_CHARS)
    if (ENABLE_FLOAT_FROM_CHARS)
        message(STATUS "Enable floating point std::from_chars")
    else()
        message(WARNING "Floating point std::from_chars is disabled, but available. "
                        "To avoid performance penalty, set ENABLE_FLOAT_FROM_CHARS to ON.")
    endif()
else()
    if (ENABLE_FLOAT_FROM_CHARS)
        message(SEND_ERROR "Floating point std::from_chars is not supported. Use a newer standard library, "
                           "or set ENABLE_FLOAT_FROM_CHARS to OFF to fall back to g_ascii_strtod. "
                           "std::from_chars offers higher performance and should be used whenever possible.")
    else()
        message(STATUS "Disable floating point std::from_chars")
    endif()
endif()

# X11 link settings

# Note: CMAKE_REQUIRED_FIND_PACKAGE is only supported by CMake >= 3.22
option(CMAKE_REQUIRED_FIND_PACKAGE_X11 "Force linking with X11" OFF)
# Disable X11 on Windows/MacOS by default
if (NOT CMAKE_REQUIRED_FIND_PACKAGE_X11 AND (CMAKE_SYSTEM_NAME MATCHES "Windows|Darwin"))
    set(_DISABLE_X11_default ON)
else()
    set(_DISABLE_X11_default OFF)
endif()
option(CMAKE_DISABLE_FIND_PACKAGE_X11 "Disable linking with X11" ${_DISABLE_X11_default})
unset(_DISABLE_X11_default)

# To configure X11 linking, set the following definitions:
#   CMAKE_DISABLE_FIND_PACKAGE_X11 - turn on to disable X11 features
#   CMAKE_REQUIRED_FIND_PACKAGE_X11 - turn on to force use of X11 features

# backwards compatibility for CMAKE_REQUIRED_FIND_PACKAGE_*
if (CMAKE_REQUIRED_FIND_PACKAGE_X11)
    find_package(X11 REQUIRED COMPONENTS Xi Xext)
else()
    find_package(X11 COMPONENTS Xi Xext)
endif ()

add_library(external_common_modules INTERFACE)
target_link_libraries(external_common_modules INTERFACE
        PkgConfig::ExternalCommonModules
        $<$<TARGET_EXISTS:Lua::lua>:Lua::lua>
        $<$<TARGET_EXISTS:PkgConfig::ExternalAudioModules>:PkgConfig::ExternalAudioModules>
        $<$<TARGET_EXISTS:X11::Xi>:X11::Xi>
        $<$<TARGET_EXISTS:X11::X11>:X11::X11>
        $<$<TARGET_EXISTS:cpptrace::cpptrace>:cpptrace::cpptrace>
        $<$<TARGET_EXISTS:qpdf::libqpdf>:qpdf::libqpdf>
        ZLIB::ZLIB
        Threads::Threads
        )

target_compile_definitions(external_common_modules INTERFACE
        GLIB_VERSION_MIN_REQUIRED=GLIB_VERSION_2_40
        $<$<TARGET_EXISTS:X11::X11>:X11_ENABLED>
        )
target_link_options(external_common_modules INTERFACE
        $<$<AND:$<NOT:$<PLATFORM_ID:Windows>>,$<OR:$<CXX_COMPILER_ID:Clang>,$<CXX_COMPILER_ID:GNU>>>:-rdynamic>
        )
add_library(xoj::external_common_modules ALIAS external_common_modules)
add_library(vertexnote::external_common_modules ALIAS external_common_modules)

add_library(external_modules INTERFACE)
target_link_libraries(external_modules INTERFACE
        vertexnote::external_common_modules
        )
add_library(xoj::external_modules ALIAS external_modules)
add_library(vertexnote::external_modules ALIAS external_modules)

## Additional features ##

configure_file(
        src/config-features.h.in
        src/config-features.h
        ESCAPE_QUOTES @ONLY
)

## Configuration headers and development options ##

# Development options
option(DEV_CALL_LOG "Call log" OFF)
add_compile_definitions(G_LOG_DOMAIN="vertexnote")

# Debug options
option(DEBUG_INPUT "Input debugging, e.g. eraser events etc" OFF)
option(DEBUG_INPUT_PRINT_ALL_MOTION_EVENTS "Input debugging, print all motion events" OFF)
option(DEBUG_RECOGNIZER "Shape recognizer debug: output score etc" OFF)
option(DEBUG_SCHEDULER "Scheduler debug: show jobs etc" OFF)
option(DEBUG_SHOW_ELEMENT_BOUNDS "Draw a surrounding border to all elements" OFF)
option(DEBUG_SHOW_REPAINT_BOUNDS "Draw a border around all repaint rects" OFF)
option(DEBUG_SHOW_PAINT_BOUNDS "Draw a border around all painted rects" OFF)
mark_as_advanced(FORCE
        DEBUG_INPUT DEBUG_RECOGNIZER DEBUG_SHEDULER DEBUG_SHOW_ELEMENT_BOUNDS DEBUG_SHOW_REPAINT_BOUNDS DEBUG_SHOW_PAINT_BOUNDS
        )

# Advanced development config
set(DEV_TOOLBAR_CONFIG "toolbar.ini" CACHE STRING "Toolbar config file name")
set(DEV_SETTINGS_XML_FILE "settings.xml" CACHE STRING "Settings file name")
set(DEV_DEFAULT_PALETTE_FILE "vertex-note.gpl" CACHE STRING "Default Color Palette")
set(DEV_PRINT_CONFIG_FILE "print-config.ini" CACHE STRING "Print config file name")
set(DEV_METADATA_FILE "metadata.ini" CACHE STRING "Metadata file name")
set(DEV_ERRORLOG_DIR "errorlogs" CACHE STRING "Directory where errorlogfiles will be placed")
set(DEV_FILE_FORMAT_VERSION 4 CACHE STRING "File format version" FORCE)

option(DEV_ENABLE_GCOV "Build with gcov support" OFF) # Enabel gcov support – expanded in src/

mark_as_advanced(FORCE
        DEV_TOOLBAR_CONFIG DEV_SETTINGS_XML_FILE DEV_PRINT_CONFIG_FILE DEV_METADATA_FILE
        DEV_ENABLE_GCOV
        )

configure_file(
        src/config.h.in
        src/config.h
        ESCAPE_QUOTES @ONLY
)

configure_file(
        src/config-debug.h.in
        src/config-debug.h
        ESCAPE_QUOTES @ONLY
)

configure_file(
        src/config-dev.h.in
        src/config-dev.h
        ESCAPE_QUOTES @ONLY
)

configure_file(
        src/config-git.h.in
        src/config-git.h
        ESCAPE_QUOTES @ONLY
)

configure_file(
        src/config-paths.h.in
        src/config-paths.h
        ESCAPE_QUOTES @ONLY
)

option(VERTEX_NOTE_LINT "enable lint (clang-tidy) target" OFF)
if (VERTEX_NOTE_LINT)
    include(clang-tidy)
endif ()

## Source building ##
add_subdirectory(src)

## Tests ##
option (ENABLE_GTEST "Enable gtest build for VertexNote application" OFF)
if (ENABLE_GTEST)
  enable_testing()
  add_subdirectory (test ${CMAKE_BINARY_DIR}/test EXCLUDE_FROM_ALL)
endif (ENABLE_GTEST)

## Man page generation ##
add_subdirectory (man)

## Tests ##
# ./test is added via src to enable cleaner includes

## Final targets and installing ##

# Install resources
install(DIRECTORY ui
        DESTINATION "share/${VERTEX_NOTE_RESOURCE_DIR}"
        COMPONENT vertex-note
        )
install(DIRECTORY plugins
        DESTINATION "share/${VERTEX_NOTE_RESOURCE_DIR}"
        COMPONENT vertex-note
        )
install(DIRECTORY resources
        DESTINATION "share/${VERTEX_NOTE_RESOURCE_DIR}"
        COMPONENT vertex-note
        )
install(DIRECTORY palettes
        DESTINATION "share/${VERTEX_NOTE_RESOURCE_DIR}"
        COMPONENT vertex-note
        )

# Install desktop shortcuts for Linux
if (${INSTALL_DESKTOP_FILES})
    message("Installing desktop files")

    # Install icons
    install(FILES ui/pixmaps/app.vertexnote.VertexNote.svg
            DESTINATION share/icons/hicolor/scalable/apps)
    install(FILES ui/pixmaps/application-x-xopp.svg
            DESTINATION share/icons/hicolor/scalable/mimetypes/)
    install(FILES ui/pixmaps/application-x-xopt.svg
            DESTINATION share/icons/hicolor/scalable/mimetypes/)
    install(FILES ui/pixmaps/application-x-xojpp.svg
            DESTINATION share/icons/hicolor/scalable/mimetypes/)
    install(FILES ui/pixmaps/gnome-mime-application-x-xopp.svg
            DESTINATION share/icons/hicolor/scalable/mimetypes/)
    install(FILES ui/pixmaps/gnome-mime-application-x-xopt.svg
            DESTINATION share/icons/hicolor/scalable/mimetypes/)
    install(FILES ${CMAKE_BINARY_DIR}/generated/app.vertexnote.VertexNote.xml
            DESTINATION share/mime/packages)
    install(FILES desktop/app.vertexnote.VertexNote.thumbnailer
            DESTINATION share/thumbnailers)
    install(FILES ${CMAKE_BINARY_DIR}/generated/app.vertexnote.VertexNote.appdata.xml
            DESTINATION share/metainfo)
endif ()

# Uninstall target
configure_file(
        cmake/cmake_uninstall.cmake.in
        cmake/cmake_uninstall.cmake
        @ONLY
)

add_custom_target(uninstall
        COMMAND ${CMAKE_COMMAND} -P ${CMAKE_CURRENT_BINARY_DIR}/cmake/cmake_uninstall.cmake

        COMMENT "Uninstall entire VertexNote"
        )

message("
Configuration:
    Compiler:                   ${CMAKE_CXX_COMPILER}
    X11 support enabled:        ${X11_FOUND}
    GTEST enabled:              ${ENABLE_GTEST}
    GCOV enabled:               ${DEV_ENABLE_GCOV}
    Profiling enabled:          ${ENABLE_PROFILING}
")

##############################
# Packaging options
# !!! only override defaults if necessary !!!
###
# Defaults:
# CPACK_PACKAGE_NAME
# CPACK_PACKAGE_VERSION
# CPACK_PACKAGE_DESCRIPTION_SUMMARY
# ...
##############################

set(CPACK_PACKAGE_NAME ${PROJECT_NAME})
set(CPACK_PACKAGE_VERSION ${PROJECT_VERSION}~git${PACKAGE_TIMESTAMP}-${RELEASE_IDENTIFIER}-${DISTRO_CODENAME})
set(CPACK_PACKAGE_DESCRIPTION_FILE "${PROJECT_SOURCE_DIR}/package_description")
set(CPACK_PACKAGE_CONTACT "${PROJECT_CONTACT}")
set(CPACK_PACKAGE_INSTALL_DIRECTORY "VertexNote")
set(CPACK_PACKAGE_FILE_NAME "vertex-note-${PROJECT_VERSION}-${DISTRO_NAME}-${DISTRO_CODENAME}-${PACKAGE_ARCH}")
set(CPACK_OUTPUT_FILE_PREFIX packages)
set(CPACK_RESOURCE_FILE_LICENSE "${PROJECT_SOURCE_DIR}/LICENSE")
set(CPACK_STRIP_FILES ON) # Debian packages must be stripped from debug messages, so lintian doesn't complain

# .deb package options
set(CPACK_DEBIAN_PACKAGE_RELEASE 2)
set(CPACK_DEBIAN_PACKAGE_VERSION "${CPACK_PROJECT_VERSION}")
set(CPACK_DEBIAN_PACKAGE_SECTION "graphics")
set(CPACK_DEBIAN_PACKAGE_DEPENDS
        "libglib2.0-0 (>= 2.32), libpoppler-glib8 (>= 0.41.0), libpoppler-cpp0v5 (>= 0.41.0), libxml2 (>= 2.0.0), libportaudiocpp0 (>= 12), libsndfile1 (>= 1.0.25), liblua5.3-0 | liblua5.4-0, libzip4 (>= 1.0.1) | libzip5, zlib1g, libc6, qpdf (>= 10.6.0)")
set(CPACK_DEBIAN_PACKAGE_SUGGESTS "texlive-base, texlive-latex-extra")  # Latex tool
# Use debian's arch scheme; we only care about x86/amd64 for now but feel free to add more
if (${PACKAGE_ARCH} STREQUAL "x86_64")
    set(CPACK_DEBIAN_PACKAGE_ARCHITECTURE "amd64")
endif ()

if (CPACK_GENERATOR MATCHES "DEB")
    message("Preparing documentation for DEB package")
    add_custom_target(package_documentation ALL)

    # Compress changelog and save it as share/doc/vertex-note/changelog.Debian.gz
    add_custom_command(TARGET package_documentation PRE_BUILD
            COMMAND gzip -c -9 -n "${PROJECT_SOURCE_DIR}/debian/changelog" > "changelog.Debian.gz" VERBATIM)
    install(FILES "${CMAKE_CURRENT_BINARY_DIR}/changelog.Debian.gz" DESTINATION "share/doc/vertex-note/")

    message("Install copyright for DEB package")
    install(FILES "${PROJECT_SOURCE_DIR}/debian/copyright" DESTINATION "share/doc/vertex-note/")
endif ()

if (NOT DEFINED CPACK_GENERATOR)
    set(CPACK_GENERATOR "TGZ")
endif ()

include(CPack)
