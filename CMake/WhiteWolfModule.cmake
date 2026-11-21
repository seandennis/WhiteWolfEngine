# Helpers for declaring engine modules and their unit tests.
#
# A module is a DLL with this layout:
#   <Module>/Public/    headers other modules may include
#   <Module>/Private/   sources and module-internal headers
#   <Module>/Tests/     unit tests (see ww_add_module_tests)
#
# Symbols are exported with a per-module macro named <MODULE>_API (ENGINE_API for the Engine
# module). The macro expands to DLLEXPORT while the module itself is being compiled and to
# DLLIMPORT for every target that links to it.

# ww_generate_reflection(<target> ROOTS <dir>...)
#
# Runs WhiteWolfHeaderTool over every header below the given directories and compiles the source
# file it writes into <target>. That file registers the classes marked with WCLASS(), with their
# WPROPERTY() members and WFUNCTION() functions.
#
# The tool runs on every build, so a header that was added, changed or deleted is always picked
# up. It leaves the generated file untouched when its content would not change, so nothing is
# compiled again unless the reflected declarations really changed.
function(ww_generate_reflection target)
    cmake_parse_arguments(PARSE_ARGV 1 arg "" "" "ROOTS")
    if(arg_UNPARSED_ARGUMENTS OR NOT arg_ROOTS)
        message(FATAL_ERROR "ww_generate_reflection(${target}): expected ROOTS <dir>...")
    endif()

    set(generated_source "${CMAKE_CURRENT_BINARY_DIR}/Generated/${target}.generated.cpp")
    set(root_arguments "")
    foreach(root IN LISTS arg_ROOTS)
        list(APPEND root_arguments --root "${root}")
    endforeach()

    add_custom_target(${target}Reflection
        COMMAND WhiteWolfHeaderTool --output "${generated_source}" ${root_arguments}
        BYPRODUCTS "${generated_source}"
        COMMENT "Generating reflection code for ${target}"
        VERBATIM)
    set_target_properties(${target}Reflection PROPERTIES FOLDER "Generated")

    target_sources(${target} PRIVATE "${generated_source}")
    add_dependencies(${target} ${target}Reflection)
    source_group("Generated" FILES "${generated_source}")
endfunction()

# ww_add_module(<name>
#     [OUTPUT_NAME <file name>]
#     [PUBLIC_DEPENDENCIES <target>...]
#     [PRIVATE_DEPENDENCIES <target>...])
#
# Declares the module DLL from every source file under Public/ and Private/. Files are found
# by globbing, so adding or removing a file needs no CMake change. Reflection code is generated
# for the headers in both directories.
function(ww_add_module name)
    cmake_parse_arguments(PARSE_ARGV 1 arg "" "OUTPUT_NAME" "PUBLIC_DEPENDENCIES;PRIVATE_DEPENDENCIES")
    if(arg_UNPARSED_ARGUMENTS)
        message(FATAL_ERROR "ww_add_module(${name}): unknown arguments: ${arg_UNPARSED_ARGUMENTS}")
    endif()

    file(GLOB_RECURSE public_headers CONFIGURE_DEPENDS
        "${CMAKE_CURRENT_SOURCE_DIR}/Public/*.h")
    file(GLOB_RECURSE private_sources CONFIGURE_DEPENDS
        "${CMAKE_CURRENT_SOURCE_DIR}/Private/*.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/Private/*.h")

    add_library(${name} SHARED ${public_headers} ${private_sources})
    ww_apply_compiler_settings(${name})

    string(TOUPPER "${name}" upper_name)
    target_compile_definitions(${name}
        PRIVATE "${upper_name}_API=DLLEXPORT"
        INTERFACE "${upper_name}_API=DLLIMPORT")
    target_include_directories(${name}
        PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/Public"
        PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/Private")
    target_link_libraries(${name}
        PUBLIC ${arg_PUBLIC_DEPENDENCIES}
        PRIVATE ${arg_PRIVATE_DEPENDENCIES})

    if(arg_OUTPUT_NAME)
        set_target_properties(${name} PROPERTIES OUTPUT_NAME "${arg_OUTPUT_NAME}")
    endif()

    source_group(TREE "${CMAKE_CURRENT_SOURCE_DIR}" FILES ${public_headers} ${private_sources})

    ww_generate_reflection(${name} ROOTS
        "${CMAKE_CURRENT_SOURCE_DIR}/Public"
        "${CMAKE_CURRENT_SOURCE_DIR}/Private")
endfunction()

# ww_add_test_executable(<target> [DEPENDENCIES <target>...])
#
# Declares <target>, a doctest executable built from every source file under Tests/, and
# registers it with CTest. Does nothing when WW_BUILD_TESTS is off.
function(ww_add_test_executable target)
    if(NOT WW_BUILD_TESTS)
        return()
    endif()

    cmake_parse_arguments(PARSE_ARGV 1 arg "" "" "DEPENDENCIES")
    if(arg_UNPARSED_ARGUMENTS)
        message(FATAL_ERROR "ww_add_test_executable(${target}): unknown arguments: ${arg_UNPARSED_ARGUMENTS}")
    endif()

    file(GLOB_RECURSE test_sources CONFIGURE_DEPENDS
        "${CMAKE_CURRENT_SOURCE_DIR}/Tests/*.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/Tests/*.h")

    add_executable(${target} ${test_sources})
    ww_apply_compiler_settings(${target})
    target_include_directories(${target} PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/Tests")
    target_link_libraries(${target} PRIVATE doctest::doctest ${arg_DEPENDENCIES})
    # doctest keeps per-thread state in thread_local containers, and a thread_local is created in
    # every thread that starts, including the worker threads of a graphics driver. Those threads
    # are still running when the process exits, so their copies are never destroyed and show up
    # as leaks. Tests assert on the main thread only, so doctest needs no per-thread state.
    target_compile_definitions(${target} PRIVATE DOCTEST_CONFIG_NO_MULTITHREADING)
    set_target_properties(${target} PROPERTIES FOLDER "Tests")

    source_group(TREE "${CMAKE_CURRENT_SOURCE_DIR}" FILES ${test_sources})

    add_test(NAME ${target} COMMAND ${target})
    # The test executable switches on the memory leak report of the debug C runtime (see
    # FProcessDiagnostics). The report is written while the process exits, too late to change
    # the exit code, so the test is failed on its first line instead.
    set_tests_properties(${target} PROPERTIES FAIL_REGULAR_EXPRESSION "Detected memory leaks!")
endfunction()

# ww_add_module_tests(<module> [DEPENDENCIES <target>...])
#
# Declares <module>Tests, the unit tests of a module: a test executable (see
# ww_add_test_executable) that links to the module and has reflection code generated for the
# classes its test headers declare. Does nothing when WW_BUILD_TESTS is off.
function(ww_add_module_tests module)
    if(NOT WW_BUILD_TESTS)
        return()
    endif()

    cmake_parse_arguments(PARSE_ARGV 1 arg "" "" "DEPENDENCIES")
    if(arg_UNPARSED_ARGUMENTS)
        message(FATAL_ERROR "ww_add_module_tests(${module}): unknown arguments: ${arg_UNPARSED_ARGUMENTS}")
    endif()

    ww_add_test_executable(${module}Tests DEPENDENCIES ${module} ${arg_DEPENDENCIES})
    ww_generate_reflection(${module}Tests ROOTS "${CMAKE_CURRENT_SOURCE_DIR}/Tests")
endfunction()
