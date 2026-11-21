# Formats or verifies every first-party C++ source file with clang-format.
#
#   cmake -P Tools/Format.cmake                Format all files in place.
#   cmake -DCHECK=ON -P Tools/Format.cmake     Change nothing; fail if any file is not formatted.
#
# Options (pass with -D before -P):
#   CLANG_FORMAT=<path>   The clang-format executable to use. Found automatically by default.
#   ALLOW_SKIP=ON         If the required clang-format is unavailable, print "FORMAT CHECK SKIPPED"
#                         and succeed instead of failing. Used by the CTest style test only.
#
# The project formatter is clang-format 18. Other major versions can produce different output,
# so they are rejected rather than allowed to disagree with CI.

cmake_minimum_required(VERSION 3.25)

set(required_major_version 18)
set(source_directories Engine Editor Runtime Plugins Tools)

get_filename_component(repository_root "${CMAKE_CURRENT_LIST_DIR}/.." ABSOLUTE)

# Reports that the formatter is unusable: a visible skip under ALLOW_SKIP, an error otherwise.
function(report_formatter_unavailable reason)
    set(help
        "Install LLVM ${required_major_version} (for example `winget install LLVM.LLVM --version 18.1.8`, "
        "or the clang-format-${required_major_version} package on Linux), or pass -DCLANG_FORMAT=<path>.")
    list(JOIN help "" help)
    if(ALLOW_SKIP)
        message(STATUS "FORMAT CHECK SKIPPED: ${reason} ${help}")
    else()
        message(FATAL_ERROR "${reason} ${help}")
    endif()
endfunction()

if(NOT CLANG_FORMAT)
    find_program(CLANG_FORMAT
        NAMES clang-format-${required_major_version} clang-format
        HINTS
            "$ENV{ProgramFiles}/LLVM/bin"
            "$ENV{VSINSTALLDIR}/VC/Tools/Llvm/x64/bin")
endif()
if(NOT CLANG_FORMAT)
    report_formatter_unavailable("clang-format was not found.")
    return()
endif()

execute_process(
    COMMAND "${CLANG_FORMAT}" --version
    OUTPUT_VARIABLE version_output
    ERROR_VARIABLE version_output
    RESULT_VARIABLE version_result)
if(NOT version_result EQUAL 0 OR NOT version_output MATCHES "version ([0-9]+)\\.")
    report_formatter_unavailable("Could not run '${CLANG_FORMAT} --version'.")
    return()
endif()
if(NOT CMAKE_MATCH_1 EQUAL required_major_version)
    report_formatter_unavailable(
        "Found clang-format ${CMAKE_MATCH_1} at '${CLANG_FORMAT}', but this project requires clang-format ${required_major_version}.")
    return()
endif()

set(source_files "")
foreach(directory IN LISTS source_directories)
    file(GLOB_RECURSE directory_files
        "${repository_root}/${directory}/*.h"
        "${repository_root}/${directory}/*.cpp")
    list(APPEND source_files ${directory_files})
endforeach()
list(LENGTH source_files source_file_count)
if(source_file_count EQUAL 0)
    message(FATAL_ERROR "No C++ source files were found under ${repository_root}.")
endif()

# A response file keeps the command line short however many files there are.
set(response_file "${repository_root}/Build/FormatFileList.txt")
list(JOIN source_files "\n" response_file_content)
file(WRITE "${response_file}" "${response_file_content}")

if(CHECK)
    set(mode_arguments --dry-run --Werror)
else()
    set(mode_arguments -i)
endif()

execute_process(
    COMMAND "${CLANG_FORMAT}" --style=file ${mode_arguments} "--files=${response_file}"
    WORKING_DIRECTORY "${repository_root}"
    RESULT_VARIABLE format_result)

if(NOT format_result EQUAL 0)
    if(CHECK)
        message(FATAL_ERROR
            "Some files are not formatted. Run `cmake -P Tools/Format.cmake` and review the changes.")
    else()
        message(FATAL_ERROR "clang-format failed with exit code ${format_result}.")
    endif()
endif()

if(CHECK)
    message(STATUS "Format check passed: ${source_file_count} files are formatted.")
else()
    message(STATUS "Formatted ${source_file_count} files.")
endif()
