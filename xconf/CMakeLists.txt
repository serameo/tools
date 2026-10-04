# CMakeLists.txt - top-level build description for the xconf library.
#
# Phase 8: cross-platform polish. Adds per-compiler warning flags,
# MSVC-specific CRT-deprecation-warning suppression (fopen() etc. are
# flagged "unsafe" by MSVC by default; this project intentionally uses
# plain portable C, not the Windows-only *_s() variants), strict C99
# (no compiler-specific extensions), and install() rules so the
# library can be packaged/installed, not just built in place.

cmake_minimum_required(VERSION 3.15)
project(xconf VERSION 1.0.0 LANGUAGES C)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)  # strict C99: catches accidental GNU-only
                             # extensions early, before they'd only
                             # surface as an MSVC build failure later.

option(XCONF_BUILD_TESTS "Build the xconf test suite" ON)

find_package(FLEX REQUIRED)
find_package(BISON REQUIRED)

# Generated files are placed under the build directory.
FLEX_TARGET(xconf_lexer
    ${CMAKE_CURRENT_SOURCE_DIR}/src/lexer.l
    ${CMAKE_CURRENT_BINARY_DIR}/lexer.yy.c
)
BISON_TARGET(xconf_parser
    ${CMAKE_CURRENT_SOURCE_DIR}/src/parser.y
    ${CMAKE_CURRENT_BINARY_DIR}/parser.tab.c
    DEFINES_FILE ${CMAKE_CURRENT_BINARY_DIR}/parser.tab.h
)
ADD_FLEX_BISON_DEPENDENCY(xconf_lexer xconf_parser)

add_library(xconf STATIC
    src/xconf.c
    src/xconf_value.c
    src/xconf_writer.c
    ${FLEX_xconf_lexer_OUTPUTS}
    ${BISON_xconf_parser_OUTPUTS}
)

target_include_directories(xconf
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
    PRIVATE
        ${CMAKE_CURRENT_BINARY_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)

if(MSVC)
    # fopen()/fclose() etc. are standard, portable C - silence MSVC's
    # "use the non-portable _s() variant instead" nudge rather than
    # taking on Windows-only code paths for it.
    target_compile_definitions(xconf PRIVATE _CRT_SECURE_NO_WARNINGS)
    target_compile_options(xconf PRIVATE /W4)
else()
    target_compile_options(xconf PRIVATE -Wall -Wextra)
endif()

include(GNUInstallDirs)
install(TARGETS xconf ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR})
install(FILES include/xconf.h DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})

if(XCONF_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
