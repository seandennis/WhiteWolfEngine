cmake_minimum_required(VERSION 3.25)

project(WhiteWolfEngine
    VERSION 0.1.0
    DESCRIPTION "A simple, production-grade 3D game engine for Windows"
    LANGUAGES CXX)

if(CMAKE_SOURCE_DIR STREQUAL CMAKE_BINARY_DIR)
    message(FATAL_ERROR
        "In-source builds are not supported. Use a preset, for example: cmake --workflow --preset windows-debug")
endif()

if(NOT (MSVC OR CMAKE_CXX_COMPILER_ID MATCHES "^(GNU|Clang)$"))
    message(FATAL_ERROR
        "Unsupported compiler '${CMAKE_CXX_COMPILER_ID}'. White Wolf Engine targets MSVC (Visual Studio 2022); "
        "GCC and Clang are supported for development builds only.")
endif()

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/CMake")

option(WW_BUILD_TESTS "Build the unit tests and register them with CTest" ON)
set(WW_SANITIZERS "" CACHE STRING
    "Sanitizers for GCC/Clang development builds, as a semicolon-separated list (for example: address;undefined)")

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Engine modules are DLLs. Hiding symbols by default gives GCC and Clang the same export
# semantics as MSVC, so a missing <MODULE>_API annotation fails to link on every compiler.
set(CMAKE_CXX_VISIBILITY_PRESET hidden)
set(CMAKE_VISIBILITY_INLINES_HIDDEN ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

# Every module and executable shares one C runtime, so objects can cross DLL boundaries.
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL")

# Executables and DLLs share one output directory so Windows finds the DLLs without PATH changes.
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/Binaries")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/Binaries")
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/Libraries")

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set_property(GLOBAL PROPERTY USE_FOLDERS ON)

include(WhiteWolfCompilerSettings)
include(WhiteWolfModule)

ww_enable_sanitizers("${WW_SANITIZERS}")

if(WW_BUILD_TESTS)
    enable_testing()
    include(WhiteWolfThirdPartyTesting)
endif()

add_subdirectory(Engine)

# Code style. `format` rewrites the sources with clang-format and `format-check` verifies them;
# CTest runs the same check, so a passing test run also means the code is formatted.
set(format_script "${CMAKE_CURRENT_SOURCE_DIR}/Tools/Format.cmake")
add_custom_target(format
    COMMAND "${CMAKE_COMMAND}" -P "${format_script}"
    COMMENT "Formatting C++ sources with clang-format"
    VERBATIM)
add_custom_target(format-check
    COMMAND "${CMAKE_COMMAND}" -DCHECK=ON -P "${format_script}"
    COMMENT "Checking C++ source formatting with clang-format"
    VERBATIM)
set_target_properties(format format-check PROPERTIES FOLDER "Tools")

if(WW_BUILD_TESTS)
    add_test(NAME CodeStyle.ClangFormat
        COMMAND "${CMAKE_COMMAND}" -DCHECK=ON -DALLOW_SKIP=ON -P "${format_script}")
    set_tests_properties(CodeStyle.ClangFormat PROPERTIES SKIP_REGULAR_EXPRESSION "FORMAT CHECK SKIPPED")
endif()
