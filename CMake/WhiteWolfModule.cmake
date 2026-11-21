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

# ww_add_module(<name>
#     [OUTPUT_NAME <file name>]
#     [PUBLIC_DEPENDENCIES <target>...]
#     [PRIVATE_DEPENDENCIES <target>...])
#
# Declares the module DLL from every source file under Public/ and Private/. Files are found
# by globbing, so adding or removing a file needs no CMake change.
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
endfunction()

# ww_add_module_tests(<module> [DEPENDENCIES <target>...])
#
# Declares <module>Tests, a doctest executable built from every source file under Tests/, and
# registers it with CTest. Does nothing when WW_BUILD_TESTS is off.
function(ww_add_module_tests module)
    if(NOT WW_BUILD_TESTS)
        return()
    endif()

    cmake_parse_arguments(PARSE_ARGV 1 arg "" "" "DEPENDENCIES")
    if(arg_UNPARSED_ARGUMENTS)
        message(FATAL_ERROR "ww_add_module_tests(${module}): unknown arguments: ${arg_UNPARSED_ARGUMENTS}")
    endif()

    file(GLOB_RECURSE test_sources CONFIGURE_DEPENDS
        "${CMAKE_CURRENT_SOURCE_DIR}/Tests/*.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/Tests/*.h")

    set(test_target "${module}Tests")
    add_executable(${test_target} ${test_sources})
    ww_apply_compiler_settings(${test_target})
    target_include_directories(${test_target} PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/Tests")
    target_link_libraries(${test_target} PRIVATE ${module} doctest::doctest ${arg_DEPENDENCIES})
    set_target_properties(${test_target} PROPERTIES FOLDER "Tests")

    source_group(TREE "${CMAKE_CURRENT_SOURCE_DIR}" FILES ${test_sources})

    add_test(NAME ${test_target} COMMAND ${test_target})
endfunction()
